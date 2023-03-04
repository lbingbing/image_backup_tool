#include <iostream>
#include <fstream>
#include <filesystem>
#include <system_error>

#include "symbol_codec.h"
#include "image_decode_task.h"

Task::Task(const std::string& path) : m_path(path), m_task_path(path + ".task"), m_blob_path(path + ".blob") {
}

void Task::Init(SymbolType symbol_type, const Dim& dim, uint32_t part_num) {
    m_symbol_type = symbol_type;
    m_dim = dim;
    m_part_num = part_num;
    m_done_part_num = 0;
    m_task_status_bytes.resize(static_cast<size_t>((m_part_num + 7) / 8), 0);
}

void Task::Load() {
    std::ifstream f(m_task_path, std::ios_base::binary);
    int symbol_type = 0;
    f.read(reinterpret_cast<char*>(&symbol_type), sizeof(symbol_type));
    m_symbol_type = static_cast<SymbolType>(symbol_type);
    f.read(reinterpret_cast<char*>(&m_dim), sizeof(m_dim));
    f.read(reinterpret_cast<char*>(&m_part_num), sizeof(m_part_num));
    f.read(reinterpret_cast<char*>(&m_done_part_num), sizeof(m_done_part_num));
    m_task_status_bytes.resize(static_cast<size_t>((m_part_num + 7) / 8), 0);
    f.read(reinterpret_cast<char*>(m_task_status_bytes.data()), m_task_status_bytes.size());
}

void Task::SetFinalizationCb(FinalizationStartCb finalization_start_cb, FinalizationProgressCb finalization_progress_cb, FinalizationCompleteCb finalization_complete_cb) {
    m_finalization_start_cb = finalization_start_cb;
    m_finalization_progress_cb = finalization_progress_cb;
    m_finalization_complete_cb = finalization_complete_cb;
}

bool Task::AllocateBlob() {
    if (std::filesystem::is_regular_file(m_blob_path)) return true;
    std::ofstream(m_blob_path, std::ios_base::binary);
    uint64_t part_byte_num = get_part_byte_num(m_symbol_type, m_dim);
    std::error_code ec;
    std::filesystem::resize_file(m_blob_path, part_byte_num * m_part_num, ec);
    return !ec;
}

bool Task::IsPartDone(uint32_t part_id) const {
    auto byte_index = part_id / 8;
    char mask = 0x1 << (part_id % 8);
    return m_task_status_bytes[byte_index] & mask;
}

void Task::UpdatePart(uint32_t part_id, const Bytes& part_bytes) {
    if (IsPartDone(part_id)) return;

    auto byte_index = part_id / 8;
    char mask = 0x1 << (part_id % 8);
    m_task_status_bytes[byte_index] |= mask;
    m_done_part_num += 1;

    m_blob_buf.emplace_back(part_id, part_bytes);

    if ((m_done_part_num & 0x1fff) == 0) {
        Flush();
    }
}

Bytes Task::ToTaskBytes() const {
    Bytes bytes;
    const uint8_t* ptr = nullptr;
    int symbol_type = static_cast<int>(m_symbol_type);
    ptr = reinterpret_cast<const uint8_t*>(&symbol_type);
    bytes.insert(bytes.end(), ptr, ptr+sizeof(symbol_type));
    ptr = reinterpret_cast<const uint8_t*>(&m_dim);
    bytes.insert(bytes.end(), ptr, ptr+sizeof(m_dim));
    ptr = reinterpret_cast<const uint8_t*>(&m_part_num);
    bytes.insert(bytes.end(), ptr, ptr+sizeof(m_part_num));
    ptr = reinterpret_cast<const uint8_t*>(&m_done_part_num);
    bytes.insert(bytes.end(), ptr, ptr+sizeof(m_done_part_num));
    bytes.insert(bytes.end(), m_task_status_bytes.begin(), m_task_status_bytes.end());
    return bytes;
}

void Task::Flush() {
    std::fstream blob_file(m_blob_path, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    for (const auto& [part_id, part_bytes] : m_blob_buf) {
        blob_file.seekp(part_id * part_bytes.size());
        blob_file.write(reinterpret_cast<const char*>(part_bytes.data()), part_bytes.size());
    }
    m_blob_buf.clear();

    std::ofstream f(m_task_path, std::ios_base::binary);
    int symbol_type = static_cast<int>(m_symbol_type);
    f.write(reinterpret_cast<char*>(&symbol_type), sizeof(symbol_type));
    f.write(reinterpret_cast<char*>(&m_dim), sizeof(m_dim));
    f.write(reinterpret_cast<char*>(&m_part_num), sizeof(m_part_num));
    f.write(reinterpret_cast<char*>(&m_done_part_num), sizeof(m_done_part_num));
    f.write(reinterpret_cast<char*>(m_task_status_bytes.data()), m_task_status_bytes.size());
}

bool Task::IsDone() const {
    return m_done_part_num == m_part_num;
}

void Task::Finalize() {
    Flush();
    std::ifstream blob_file(m_blob_path, std::ios_base::binary);
    uint64_t part_byte_num = get_part_byte_num(m_symbol_type, m_dim);
    blob_file.seekg(part_byte_num * (m_part_num - 1));
    uint64_t file_size = 0;
    blob_file.read(reinterpret_cast<char*>(&file_size), sizeof(file_size));
    blob_file.close();
    std::filesystem::resize_file(m_blob_path, file_size);
    std::filesystem::rename(m_blob_path, m_path);
    std::filesystem::remove(m_task_path);
    if (m_finalization_complete_cb) m_finalization_complete_cb();
}

void Task::Print(int64_t show_undone_part_num) const {
    std::cout << "symbol_type=" << get_symbol_type_str(m_symbol_type) << "\n";
    std::cout << "dim=" << m_dim << "\n";
    std::cout << "part_num=" << m_part_num << "\n";
    std::cout << "done_part_num=" << m_done_part_num << "\n";
    if (show_undone_part_num > 0) {
        std::cout << "undone parts:\n";
        for (uint32_t part_id = 0; part_id < m_part_num; ++part_id) {
            if (!IsPartDone(part_id)) {
                std::cout << part_id << "\n";
                --show_undone_part_num;
                if (show_undone_part_num == 0) break;
            }
        }
    }
}

int get_part_byte_num(SymbolType symbol_type, const Dim& dim) {
    auto codec = create_symbol_codec(symbol_type);
    return dim.tile_x_num * dim.tile_y_num * dim.tile_x_size * dim.tile_y_size * codec->BitNumPerSymbol() / 8 - SymbolCodec::META_BYTE_NUM;
}

std::tuple<Bytes, uint32_t> get_task_bytes(const std::string& file_path, int part_byte_num) {
    std::ifstream f(file_path, std::ios_base::binary);
    f.seekg(0, std::ios_base::end);
    uint64_t file_size = f.tellg();
    Bytes raw_bytes(file_size);
    f.seekg(0, std::ios_base::beg);
    f.read(reinterpret_cast<char*>(raw_bytes.data()), file_size);
    int left_bytes_num1 = file_size % part_byte_num;
    Bytes padding_bytes1;
    if (left_bytes_num1) padding_bytes1.resize(part_byte_num - left_bytes_num1, 0);
    Bytes size_bytes(reinterpret_cast<Byte*>(&file_size), reinterpret_cast<Byte*>(&file_size)+sizeof(file_size));
    Bytes padding_bytes2(part_byte_num - size_bytes.size(), 0);
    raw_bytes.insert(raw_bytes.end(), padding_bytes1.begin(), padding_bytes1.end());
    raw_bytes.insert(raw_bytes.end(), size_bytes.begin(), size_bytes.end());
    raw_bytes.insert(raw_bytes.end(), padding_bytes2.begin(), padding_bytes2.end());
    uint32_t part_num = raw_bytes.size() / part_byte_num;
    return std::make_tuple(std::move(raw_bytes), part_num);
}

std::tuple<SymbolType, Dim, uint32_t, uint32_t, Bytes> from_task_bytes(const Bytes& task_bytes) {
    size_t count = 0;
    auto ptr = task_bytes.data();
    int symbol_type_v = 0;
    count = sizeof(symbol_type_v);
    std::copy_n(ptr, count, reinterpret_cast<Byte*>(&symbol_type_v));
    ptr += count;
    SymbolType symbol_type = static_cast<SymbolType>(symbol_type_v);
    Dim dim;
    count = sizeof(dim);
    std::copy_n(ptr, count, reinterpret_cast<Byte*>(&dim));
    ptr += count;
    uint32_t part_num = 0;
    count = sizeof(part_num);
    std::copy_n(ptr, count, reinterpret_cast<Byte*>(&part_num));
    ptr += count;
    uint32_t done_part_num = 0;
    count = sizeof(done_part_num);
    std::copy_n(ptr, count, reinterpret_cast<Byte*>(&done_part_num));
    ptr += count;
    Bytes task_status_bytes(ptr, task_bytes.data()+task_bytes.size());
    size_t expected_task_byte_num = 4 * 7 + (part_num + 7) / 8;
    if (task_bytes.size() != expected_task_byte_num) {
        throw invalid_image_codec_argument("invalid task bytes of size '" + std::to_string(task_bytes.size()) + "', '" + std::to_string(expected_task_byte_num) + "' expected");
    }
    return std::make_tuple(symbol_type, dim, part_num, done_part_num, std::move(task_status_bytes));
}

bool is_part_done(const Bytes& task_status_bytes, uint32_t part_id) {
    auto byte_index = part_id / 8;
    char mask = 0x1 << (part_id % 8);
    return task_status_bytes[byte_index] & mask;
}
