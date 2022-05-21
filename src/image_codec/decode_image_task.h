#pragma once

#include <string>
#include <vector>

#include "image_codec_api.h"
#include "pixel_codec.h"
#include "decode_image.h"

class Task {
public:
	IMAGE_CODEC_API Task(const std::string& path);
	IMAGE_CODEC_API ~Task();
	IMAGE_CODEC_API void Init(const Dim& dim, PixelType pixel_type, int pixel_size, int space_size, uint32_t part_num);
	IMAGE_CODEC_API void Load();
	IMAGE_CODEC_API void UpdatePart(uint32_t part_id, Bytes part_bytes);
	IMAGE_CODEC_API bool IsDone() const;
	IMAGE_CODEC_API void Print(int64_t show_undone_part_num) const;

	uint32_t DonePartNum() const { return m_done_part_num; }
	const std::string& TaskPath() const { return m_task_path; }
	Dim GetDim() const { return m_dim; }
	PixelType GetPixelType() const { return m_pixel_type; }
	int GetPixelSize() const { return m_pixel_size; }
	int GetSpaceSize() const { return m_space_size; }
	uint32_t GetPartNum() const { return m_part_num; }

private:
	IMAGE_CODEC_API bool IsPartDone(uint32_t part_id) const;
	IMAGE_CODEC_API void Flush();
	IMAGE_CODEC_API void Finalize();

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
};