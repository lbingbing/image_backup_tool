#pragma once

#include <string>
#include <vector>
#include <functional>

#include "image_codec_api.h"
#include "image_codec_types.h"

class Task {
public:
    struct FinalizationProgress {
        uint64_t done_block_num = 0;
        uint64_t block_num = 0;
    };

    using FinalizationStartCb = std::function<void(const FinalizationProgress&)>;
    using FinalizationProgressCb = std::function<void(const FinalizationProgress&)>;
    using FinalizationCompleteCb = std::function<void()>;

    static constexpr int MIN_PART_BYTE_NUM = 8;

    IMAGE_CODEC_API Task(const std::string& path);
    IMAGE_CODEC_API void Init(SymbolType symbol_type, const Dim& dim, uint32_t part_num);
    IMAGE_CODEC_API void Load();
    IMAGE_CODEC_API void SetFinalizationCb(FinalizationStartCb finalization_start_cb, FinalizationProgressCb finalization_progress_cb, FinalizationCompleteCb finalization_complete_cb);
    IMAGE_CODEC_API bool AllocateBlob();
    IMAGE_CODEC_API bool IsPartDone(uint32_t part_id) const;
    IMAGE_CODEC_API void UpdatePart(uint32_t part_id, const Bytes& part_bytes);
    IMAGE_CODEC_API void Flush();
    IMAGE_CODEC_API bool IsDone() const;
    IMAGE_CODEC_API void Finalize();
    IMAGE_CODEC_API void Print(uint32_t show_undone_part_num) const;

    IMAGE_CODEC_API uint32_t DonePartNum() const { return m_done_part_num; }
    IMAGE_CODEC_API const std::string& TaskPath() const { return m_task_path; }
    IMAGE_CODEC_API const std::string& BlobPath() const { return m_blob_path; }
    IMAGE_CODEC_API SymbolType GetSymbolType() const { return m_symbol_type; }
    IMAGE_CODEC_API Dim GetDim() const { return m_dim; }
    IMAGE_CODEC_API uint32_t GetPartNum() const { return m_part_num; }
    IMAGE_CODEC_API Bytes ToTaskBytes() const;

private:
    std::string m_path;
    std::string m_task_path;
    std::string m_blob_path;

    SymbolType m_symbol_type = SymbolType::SYMBOL1;
    Dim m_dim;
    uint32_t m_part_num = 0;
    uint32_t m_done_part_num = 0;
    Bytes m_task_status_bytes;

    std::vector<std::pair<uint32_t, Bytes>> m_blob_buf;

    FinalizationStartCb m_finalization_start_cb;
    FinalizationProgressCb m_finalization_progress_cb;
    FinalizationCompleteCb m_finalization_complete_cb;
};

IMAGE_CODEC_API int get_part_byte_num(SymbolType symbol_type, const Dim& dim);
IMAGE_CODEC_API std::tuple<Bytes, uint32_t> get_task_bytes(const std::string& file_path, int part_byte_num);
IMAGE_CODEC_API std::tuple<SymbolType, Dim, uint32_t, uint32_t, Bytes> from_task_bytes(const Bytes& task_bytes);
IMAGE_CODEC_API bool is_part_done(const Bytes& task_status_bytes, uint32_t part_id);
