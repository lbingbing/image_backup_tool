#pragma once

#include <string>
#include <functional>
#include <optional>

#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "image_codec_types.h"
#include "symbol_codec.h"

using GenPartImageFn1 = std::function<std::optional<std::pair<uint32_t, cv::Mat>>()>;
using GenPartImageFn2 = std::function<std::optional<std::pair<std::string, cv::Mat>>()>;

IMAGE_CODEC_API std::tuple<std::unique_ptr<SymbolCodec>, uint32_t, Bytes, uint32_t> prepare_part_images(const std::string& target_file, SymbolType symbol_type, const Dim& dim);
IMAGE_CODEC_API GenPartImageFn1 generate_part_images(const Dim& dim, int pixel_size, int space_size, SymbolCodec* symbol_codec, uint32_t part_byte_num, const Bytes& raw_bytes, uint32_t part_num);
IMAGE_CODEC_API std::string get_part_image_file_name(uint32_t part_num, uint32_t part_id);
IMAGE_CODEC_API void start_image_stream_server(GenPartImageFn2 gen_image_fn, int port);
