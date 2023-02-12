#pragma once

#include "image_codec_api.h"
#include "image_codec_types.h"
#include "transform_utils.h"
#include "symbol_codec.h"

struct TileCalibration {
    std::vector<std::vector<std::array<float, 2>>> centers;
};

struct Calibration {
    bool valid = false;
    Dim dim;
    std::vector<std::vector<TileCalibration>> tiles;

    IMAGE_CODEC_API void Load(const std::string& path);
    IMAGE_CODEC_API void Save(const std::string& path);
};

using CalibrateResult = std::tuple<cv::Mat, Calibration, std::vector<std::vector<cv::Mat>>>;
using ImageDecodeResult = std::tuple<bool, uint32_t, Bytes, Symbols, cv::Mat, std::vector<std::vector<cv::Mat>>>;

class ImageDecoder {
public:
    IMAGE_CODEC_API ImageDecoder(SymbolType symbol_type, const Dim& dim);
    IMAGE_CODEC_API SymbolCodec& GetSymbolCodec() { return *m_symbol_codec; }
    IMAGE_CODEC_API const Dim& GetDim() { return m_dim; }
    IMAGE_CODEC_API CalibrateResult Calibrate(const cv::Mat& img, const Transform& transform, bool result_image = false);
    IMAGE_CODEC_API ImageDecodeResult Decode(const cv::Mat& img, const Transform& transform, const Calibration& calibration, bool result_image = false);

private:
    std::unique_ptr<SymbolCodec> m_symbol_codec;
    Dim m_dim;
};
