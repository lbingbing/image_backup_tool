#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>

#include "pixel_codec.h"
#include "decode_image_task.h"

Task::Task(const std::string& path) : m_path(path), m_task_path(path + ".task"), m_blob_path(path + ".blob") {
}

void Task::Init(const Dim& dim, PixelType pixel_type, int pixel_size, int space_size, uint32_t part_num) {
    m_dim = dim;
    m_pixel_type = pixel_type;
    m_pixel_size = pixel_size;
    m_space_size = space_size;
    m_part_num = part_num;
    m_done_part_num = 0;
    m_task_status_bytes.resize(static_cast<size_t>((m_part_num + 7) / 8), 0);
}

void Task::Load() {
    std::ifstream f(m_task_path, std::ios_base::binary);
    f.read(reinterpret_cast<char*>(&m_dim), sizeof(Dim));
    int pixel_type = 0;
    f.read(reinterpret_cast<char*>(&pixel_type), sizeof(pixel_type));
    m_pixel_type = static_cast<PixelType>(pixel_type);
    f.read(reinterpret_cast<char*>(&m_pixel_size), sizeof(m_pixel_size));
    f.read(reinterpret_cast<char*>(&m_space_size), sizeof(m_space_size));
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

    if ((m_done_part_num & 0x3ff) == 0) {
        Flush();
    }
}

void Task::Flush() {
    auto blob_mode = std::ios_base::out | std::ios_base::binary;
    if (std::filesystem::is_regular_file(m_blob_path)) blob_mode |= std::ios_base::in;
    std::fstream blob_file(m_blob_path, blob_mode);
    for (const auto& [part_id, part_bytes] : m_blob_buf) {
        blob_file.seekp(part_id * part_bytes.size());
        blob_file.write(reinterpret_cast<const char*>(part_bytes.data()), part_bytes.size());
    }
    m_blob_buf.clear();

    std::ofstream f(m_task_path, std::ios_base::binary);
    f.write(reinterpret_cast<char*>(&m_dim), sizeof(Dim));
    int pixel_type = static_cast<int>(m_pixel_type);
    f.write(reinterpret_cast<char*>(&pixel_type), sizeof(pixel_type));
    f.write(reinterpret_cast<char*>(&m_pixel_size), sizeof(m_pixel_size));
    f.write(reinterpret_cast<char*>(&m_space_size), sizeof(m_space_size));
    f.write(reinterpret_cast<char*>(&m_part_num), sizeof(m_part_num));
    f.write(reinterpret_cast<char*>(&m_done_part_num), sizeof(m_done_part_num));
    f.write(reinterpret_cast<char*>(m_task_status_bytes.data()), m_task_status_bytes.size());
}

bool Task::IsDone() const {
    return m_done_part_num == m_part_num;
}

void Task::Finalize() {
    Flush();
    std::fstream blob_file(m_blob_path, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    blob_file.seekg(0);
    uint64_t file_size = 0;
    blob_file.read(reinterpret_cast<char*>(&file_size), sizeof(file_size));
    constexpr uint64_t BLOCK_SIZE = 1024 * 1024;
    Bytes block(BLOCK_SIZE);
    uint64_t block_num = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (m_finalization_start_cb) m_finalization_start_cb({0, block_num});
    for (uint64_t block_index = 0; block_index < block_num; ++block_index) {
        uint64_t offset = BLOCK_SIZE * block_index;
        blob_file.seekg(8 + offset);
        blob_file.read(reinterpret_cast<char*>(block.data()), block.size());
        auto size = blob_file.gcount();
        if (!size) break;
        blob_file.clear();
        blob_file.seekp(offset);
        blob_file.write(reinterpret_cast<const char*>(block.data()), size);
        if (m_finalization_progress_cb) m_finalization_progress_cb({block_index+1, block_num});
    }
    blob_file.close();
    std::filesystem::resize_file(m_blob_path, file_size);
    std::filesystem::rename(m_blob_path, m_path);
    std::filesystem::remove(m_task_path);
    if (m_finalization_complete_cb) m_finalization_complete_cb();
}

void Task::Print(int64_t show_undone_part_num) const {
    std::cout << "dim=" << m_dim << "\n";
    std::cout << "pixel_type=" << get_pixel_type_str(m_pixel_type) << "\n";
    std::cout << "pixel_size=" << m_pixel_size << "\n";
    std::cout << "space_size=" << m_space_size << "\n";
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
