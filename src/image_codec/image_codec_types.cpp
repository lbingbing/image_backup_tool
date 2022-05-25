#include <iomanip>
#include <exception>

#include "image_codec_types.h"

std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& v) {
    os << "[";
    os << std::hex;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) os << ",";
        os << std::setw(2) << std::setfill('0') << static_cast<int>(v[i]);
    }
    os << "]";
    return os;
}

std::string get_pixel_name(Pixel pixel) {
    switch (pixel) {
        case PIXEL_WHITE:   return "pixel_white";
        case PIXEL_BLACK:   return "pixel_black";
        case PIXEL_RED:     return "pixel_red";
        case PIXEL_BLUE:    return "pixel_blue";
        case PIXEL_GREEN:   return "pixel_green";
        case PIXEL_CYAN:    return "pixel_cyan";
        case PIXEL_MAGENTA: return "pixel_magenta";
        case PIXEL_YELLOW:  return "pixel_yellow";
        case PIXEL_UNKNOWN: return "pixel_unknown";
        case PIXEL_NUM:     return "pixel_num";
        default:            return "pixel_default";
    }
}

PixelType parse_pixel_type(const std::string& pixel_type_str) {
    PixelType pixel_type = PixelType::PIXEL2;
    if (pixel_type_str == "pixel2") {
        pixel_type = PixelType::PIXEL2;
    } else if (pixel_type_str == "pixel4") {
        pixel_type = PixelType::PIXEL4;
    } else if (pixel_type_str == "pixel8") {
        pixel_type = PixelType::PIXEL8;
    } else {
        throw std::invalid_argument("invalid pixel type '" + pixel_type_str + "'");
    }
    return pixel_type;
}

std::string get_pixel_type_str(PixelType pixel_type) {
    if (pixel_type == PixelType::PIXEL2)      return "pixel2";
    else if (pixel_type == PixelType::PIXEL4) return "pixel4";
    else if (pixel_type == PixelType::PIXEL8) return "pixel8";
    else                                      throw std::invalid_argument("invalid pixel type " + std::to_string(static_cast<int>(pixel_type)));
}

bool operator==(const Dim& dim1, const Dim& dim2) {
    return dim1.tile_x_num == dim2.tile_x_num && dim1.tile_y_num == dim2.tile_y_num && dim1.tile_x_size == dim2.tile_x_size && dim1.tile_y_size == dim2.tile_y_size;
}

bool operator!=(const Dim& dim1, const Dim& dim2) {
    return !(dim1 == dim2);
}

std::ostream& operator<<(std::ostream& os, const Dim& dim) {
    os << "(" << dim.tile_x_num << "," << dim.tile_y_num << "," << dim.tile_x_size << "," << dim.tile_y_size << ")";
    return os;
}

Dim parse_dim(const std::string& dim_str) {
    auto dims = parse_vec<int>(dim_str);
    if (dims.size() != 4) throw std::invalid_argument("invalid dim");
    return { dims[0], dims[1], dims[2], dims[3] };
}
