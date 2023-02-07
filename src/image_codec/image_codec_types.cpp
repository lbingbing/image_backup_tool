#include <iomanip>

#include "image_codec_types.h"

std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& v) {
    os << std::hex;
    os << "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) os << ",";
        os << std::setw(2) << std::setfill('0') << static_cast<int>(v[i]);
    }
    os << "]";
    os << std::dec;
    return os;
}

std::string get_pixel_color(Symbol symbol) {
    switch (symbol) {
        case static_cast<int>(PixelColor::WHITE):   return "white";
        case static_cast<int>(PixelColor::BLACK):   return "black";
        case static_cast<int>(PixelColor::RED):     return "red";
        case static_cast<int>(PixelColor::BLUE):    return "blue";
        case static_cast<int>(PixelColor::GREEN):   return "green";
        case static_cast<int>(PixelColor::CYAN):    return "cyan";
        case static_cast<int>(PixelColor::MAGENTA): return "magenta";
        case static_cast<int>(PixelColor::YELLOW):  return "yellow";
        case static_cast<int>(PixelColor::UNKNOWN): return "unknown";
        case static_cast<int>(PixelColor::NUM):     return "num";
        default:                                    return "default";
    }
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
    auto dims = parse_array<int, 4>(dim_str);
    if (dims.size() != 4) throw std::invalid_argument("invalid dim");
    return {dims[0], dims[1], dims[2], dims[3]};
}
