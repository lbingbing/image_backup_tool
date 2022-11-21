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

    IMAGE_CODEC_API Task(const std::string& path);
    IMAGE_CODEC_API void Init(const Dim& dim, PixelType pixel_type, int pixel_size, int space_size, uint32_t part_num);
    IMAGE_CODEC_API void Load();
    IMAGE_CODEC_API void SetFinalizationCb(FinalizationStartCb finalization_start_cb, FinalizationProgressCb finalization_progress_cb, FinalizationCompleteCb finalization_complete_cb);
    IMAGE_CODEC_API void UpdatePart(uint32_t part_id, const Bytes& part_bytes);
    IMAGE_CODEC_API void Flush();
    IMAGE_CODEC_API bool IsDone() const;
    IMAGE_CODEC_API void Finalize();
    IMAGE_CODEC_API void Print(int64_t show_undone_part_num) const;

    uint32_t DonePartNum() const { return m_done_part_num; }
    const std::string& TaskPath() const { return m_task_path; }
    Dim GetDim() const { return m_dim; }
    PixelType GetPixelType() const { return m_pixel_type; }
    int GetPixelSize() const { return m_pixel_size; }
    int GetSpaceSize() const { return m_space_size; }
    uint32_t GetPartNum() const { return m_part_num; }
    const Bytes& GetTaskStatusBytes() const { return m_task_status_bytes; }

private:
    IMAGE_CODEC_API bool IsPartDone(uint32_t part_id) const;

    std::string m_path;
    std::string m_task_path;
    std::string m_blob_path;

    Dim m_dim;
    PixelType m_pixel_type = PixelType::PIXEL2;
    int m_pixel_size = 0;
    int m_space_size = 0;
    uint32_t m_part_num = 0;
    uint32_t m_done_part_num = 0;
    Bytes m_task_status_bytes;

    std::vector<std::pair<uint32_t, Bytes>> m_blob_buf;

    FinalizationStartCb m_finalization_start_cb;
    FinalizationProgressCb m_finalization_progress_cb;
    FinalizationCompleteCb m_finalization_complete_cb;
};
