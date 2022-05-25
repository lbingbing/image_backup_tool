#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "image_codec_api.h"

using Byte = uint8_t;
using Bytes = std::vector<Byte>;
using Pixel = uint8_t;
using Pixels = std::vector<Pixel>;

IMAGE_CODEC_API std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& v);

enum class PixelType {
    PIXEL2 = 0,
    PIXEL4 = 1,
    PIXEL8 = 2,
};

constexpr uint8_t PIXEL_WHITE   = 0;
constexpr uint8_t PIXEL_BLACK   = 1;
constexpr uint8_t PIXEL_RED     = 2;
constexpr uint8_t PIXEL_BLUE    = 3;
constexpr uint8_t PIXEL_GREEN   = 4;
constexpr uint8_t PIXEL_CYAN    = 5;
constexpr uint8_t PIXEL_MAGENTA = 6;
constexpr uint8_t PIXEL_YELLOW  = 7;
constexpr uint8_t PIXEL_UNKNOWN = 8;
constexpr uint8_t PIXEL_NUM     = 9;

IMAGE_CODEC_API std::string get_pixel_name(Pixel pixel);

IMAGE_CODEC_API PixelType parse_pixel_type(const std::string& pixel_type_str);
IMAGE_CODEC_API std::string get_pixel_type_str(PixelType pixel_type);

class invalid_image_codec_argument : public std::invalid_argument {
public:
    invalid_image_codec_argument(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};

template <typename T>
const char* TypeNameStr = "unknown";
template <>
const char* TypeNameStr<int> = "int";
template <>
const char* TypeNameStr<float> = "float";

template <typename T> inline T stox(const std::string& s);
template <> inline int stox(const std::string& s) { return std::stoi(s); }
template <> inline float stox(const std::string& s) { return std::stof(s); }

template <typename T>
inline std::vector<T> parse_vec(const std::string& str) {
    std::vector<T> vec;
    size_t pos = 0;
    while (true) {
        auto pos1 = str.find(",", pos);
        auto s = str.substr(pos, pos1 == std::string::npos ? std::string::npos : pos1 - pos);
        if (s[0] == 'n') s.replace(0, 1, "-");
        try {
            vec.push_back(stox<T>(s));
        }
        catch (const std::exception&) {
            throw invalid_image_codec_argument("invalid " + std::string(TypeNameStr<T>) +"s '" + str + "'");
        }
        if (pos1 == std::string::npos) break;
        pos = pos1 + 1;
    }
    return vec;
}

struct Dim {
    int tile_x_num = 0;
    int tile_y_num = 0;
    int tile_x_size = 0;
    int tile_y_size = 0;
};

IMAGE_CODEC_API bool operator==(const Dim& dim1, const Dim& dim2);
IMAGE_CODEC_API bool operator!=(const Dim& dim1, const Dim& dim2);
IMAGE_CODEC_API std::ostream& operator<<(std::ostream& os, const Dim& dim);

IMAGE_CODEC_API Dim parse_dim(const std::string& dim_str);
