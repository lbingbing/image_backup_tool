#pragma once

#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <exception>

#include "image_codec_api.h"

using Byte = uint8_t;
using Bytes = std::vector<Byte>;
using Symbol = uint8_t;
using Symbols = std::vector<Symbol>;

IMAGE_CODEC_API std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& v);

enum class PixelColor {
    WHITE   = 0,
    BLACK   = 1,
    RED     = 2,
    BLUE    = 3,
    GREEN   = 4,
    CYAN    = 5,
    MAGENTA = 6,
    YELLOW  = 7,
    UNKNOWN = 8,
    NUM     = 9,
};

IMAGE_CODEC_API std::string get_pixel_color(Symbol symbol);

class invalid_image_codec_argument : public std::invalid_argument {
public:
    invalid_image_codec_argument(const std::string& what_arg) : std::invalid_argument(what_arg) {
#if _WIN32
        std::cerr << what_arg << "\n";
#endif
    }
};

template <typename T>
inline const char* TypeNameStr = "unknown";
template <>
inline const char* TypeNameStr<int> = "int";
template <>
inline const char* TypeNameStr<float> = "float";

template <typename T> inline T stox(const std::string& s);
template <> inline int stox(const std::string& s) { return std::stoi(s); }
template <> inline float stox(const std::string& s) { return std::stof(s); }

template <typename T, size_t N>
inline std::array<T, N> parse_array(const std::string& str) {
    std::array<T, N> arr;
    int index = 0;
    size_t pos = 0;
    while (true) {
        if (index >= N) throw invalid_image_codec_argument("element num > N (" + std::to_string(N) + ") '" + str + "'");
        auto pos1 = str.find(",", pos);
        auto s = str.substr(pos, pos1 == std::string::npos ? std::string::npos : pos1 - pos);
        if (s[0] == 'n') s.replace(0, 1, "-");
        try {
            arr[index] = stox<T>(s);
            ++index;
        }
        catch (const std::exception&) {
            throw invalid_image_codec_argument("invalid " + std::string(TypeNameStr<T>) +"s '" + str + "'");
        }
        if (pos1 == std::string::npos) break;
        pos = pos1 + 1;
    }
    if (index < N) throw invalid_image_codec_argument("element num < N (" + std::to_string(N) + ") '" + str + "'");
    return arr;
}
template <typename T>
inline std::vector<T> parse_vector(const std::string& str) {
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
