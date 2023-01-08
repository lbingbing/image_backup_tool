#include <cassert>
#include <iomanip>
#include <algorithm>
#include <boost/crc.hpp>

#include "pixel_codec.h"

namespace {
    Bytes encrypt_bytes(const Bytes& bytes) {
        Bytes encrypted_bytes(bytes.rbegin(), bytes.rend());
        for (auto& e : encrypted_bytes) {
            e ^= Byte(170);
        }
        return encrypted_bytes;
    }

    Bytes uint32_to_bytes(uint32_t v) {
        return {
            static_cast<Byte>((v >> 0) & 0xff),
            static_cast<Byte>((v >> 8) & 0xff),
            static_cast<Byte>((v >> 16) & 0xff),
            static_cast<Byte>((v >> 24) & 0xff),
        };
    }

    uint32_t bytes_to_uint32(const Bytes& bytes) {
        return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
    }

    uint32_t crc32(const Bytes& bytes) {
        boost::crc_32_type crc_gen;
        crc_gen.process_bytes(bytes.data(), bytes.size());
        return crc_gen.checksum();
    }

    Bytes scramble_part_id_bytes(const Bytes& part_id_bytes, const Bytes& crc_bytes) {
        Bytes scrambled_part_id_bytes = part_id_bytes;
        for (size_t i = 0; i < scrambled_part_id_bytes.size(); ++i) {
            scrambled_part_id_bytes[i] ^= crc_bytes[i&0x3];
        }
        return scrambled_part_id_bytes;
    }
}

Pixels PixelCodec::Encode(uint32_t part_id, const Bytes& part_bytes, int frame_size) {
    if (frame_size < ((META_BYTE_NUM + part_bytes.size()) * 8 + BitNumPerPixel() - 1) / BitNumPerPixel()) throw std::invalid_argument("invalid encode arguments");
    Bytes part_id_bytes = uint32_to_bytes(part_id);
    Bytes padded_part_bytes = part_bytes;
    padded_part_bytes.resize(static_cast<size_t>(frame_size * BitNumPerPixel() / 8 - META_BYTE_NUM), 0);
    Bytes bytes_for_crc;
    bytes_for_crc.insert(bytes_for_crc.end(), part_id_bytes.begin(), part_id_bytes.end());
    bytes_for_crc.insert(bytes_for_crc.end(), padded_part_bytes.begin(), padded_part_bytes.end());
    uint32_t crc = crc32(bytes_for_crc);
    Bytes crc_bytes = uint32_to_bytes(crc);
    Bytes scrambled_part_id_bytes = scramble_part_id_bytes(part_id_bytes, crc_bytes);
    Bytes bytes;
    bytes.insert(bytes.end(), crc_bytes.begin(), crc_bytes.end());
    bytes.insert(bytes.end(), scrambled_part_id_bytes.begin(), scrambled_part_id_bytes.end());
    bytes.insert(bytes.end(), padded_part_bytes.begin(), padded_part_bytes.end());
    Pixels pixels = BytesToPixels(encrypt_bytes(bytes));
    pixels.resize(frame_size);
    if (0) {
        std::cout << "encode\n";
        std::cout << "frame_size: " << std::dec << frame_size << "\n";
        std::cout << "part_id: " << std::hex << part_id << "\n";
        std::cout << "part_id_bytes: " << part_id_bytes << "\n";
        std::cout << "part_bytes: " << part_bytes << "\n";
        std::cout << "padded_part_bytes: " << padded_part_bytes << "\n";
        std::cout << "bytes_for_crc: " << bytes_for_crc << "\n";
        std::cout << "crc: " << std::hex << crc << "\n";
        std::cout << "crc_bytes: " << crc_bytes << "\n";
        std::cout << "bytes: " << bytes << "\n";
        std::cout << "pixels: " << pixels << "\n";
    }
    return pixels;
}

DecodeResult PixelCodec::Decode(const Pixels& pixels) {
    if (static_cast<int>(pixels.size()) < ((META_BYTE_NUM + 1) * 8 + BitNumPerPixel() - 1) / BitNumPerPixel()) throw std::invalid_argument("invalid encode arguments");
    Pixels truncated_pixels(pixels.begin(), pixels.begin() + (pixels.size() * BitNumPerPixel() / 8 * 8 + BitNumPerPixel() - 1) / BitNumPerPixel());
    Bytes bytes = encrypt_bytes(PixelsToBytes(truncated_pixels));
    Bytes crc_bytes(bytes.begin(), bytes.begin() + CRC_BYTE_NUM);
    uint32_t crc = bytes_to_uint32(crc_bytes);
    Bytes scrambled_part_id_bytes(bytes.begin() + CRC_BYTE_NUM, bytes.begin() + META_BYTE_NUM);
    Bytes part_id_bytes = scramble_part_id_bytes(scrambled_part_id_bytes, crc_bytes);
    uint32_t part_id = bytes_to_uint32(part_id_bytes);
    Bytes padded_part_bytes(bytes.begin() + META_BYTE_NUM, bytes.end());
    Bytes bytes_for_crc;
    bytes_for_crc.insert(bytes_for_crc.end(), part_id_bytes.begin(), part_id_bytes.end());
    bytes_for_crc.insert(bytes_for_crc.end(), padded_part_bytes.begin(), padded_part_bytes.end());
    uint32_t crc_computed = crc32(bytes_for_crc);
    bool success = crc_computed == crc;
    if (0) {
        std::cout << "decode\n";
        std::cout << "pixels: " << pixels << "\n";
        std::cout << "truncated_pixels: " << truncated_pixels << "\n";
        std::cout << "bytes: " << bytes << "\n";
        std::cout << "crc_bytes: " << crc_bytes << "\n";
        std::cout << "crc: " << std::hex << crc << "\n";
        std::cout << "part_id_bytes: " << part_id_bytes << "\n";
        std::cout << "part_id: " << std::hex << part_id << "\n";
        std::cout << "padded_part_bytes: " << padded_part_bytes << "\n";
        std::cout << "bytes_for_crc: " << bytes_for_crc << "\n";
        std::cout << "crc_computed: " << crc_computed << "\n";
    }
    return {success, part_id, padded_part_bytes};
}

Pixels Pixel2Codec::BytesToPixels(const Bytes& bytes) {
    Pixels pixels;
    for (auto byte : bytes) {
        for (int i = 7; i >= 0; --i) {
            pixels.push_back((byte >> i) & 0x1);
        }
    }
    return pixels;
}

Bytes Pixel2Codec::PixelsToBytes(const Pixels& pixels) {
    Bytes bytes;
    for (size_t i = 0; i < pixels.size(); i += 8) {
        Byte byte = 0;
        for (int j = 0; j < 8; ++j) {
            byte |= (pixels[i + j] & 0x1) << (7 - j);
        }
        bytes.push_back(byte);
    }
    return bytes;
}

Pixels Pixel4Codec::BytesToPixels(const Bytes& bytes) {
    Pixels pixels;
    for (auto byte : bytes) {
        for (int i = 6; i >= 0; i -= 2) {
            pixels.push_back((byte >> i) & 0x3);
        }
    }
    return pixels;
}

Bytes Pixel4Codec::PixelsToBytes(const Pixels& pixels) {
    Bytes bytes;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        Byte byte = 0;
        for (int j = 0; j < 4; ++j) {
            byte |= (pixels[i + j] & 0x3) << (6 - j * 2);
        }
        bytes.push_back(byte);
    }
    return bytes;
}

Pixels Pixel8Codec::BytesToPixels(const Bytes& bytes) {
    Pixels pixels;
    Byte left = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        Byte byte = bytes[i];
        if (i % 3 == 0) {
            pixels.push_back((byte >> 5) & 0x7);
            pixels.push_back((byte >> 2) & 0x7);
            left = byte & 0x3;
        } else if (i % 3 == 1) {
            pixels.push_back((left << 1) | ((byte >> 7) & 0x1));
            pixels.push_back((byte >> 4) & 0x7);
            pixels.push_back((byte >> 1) & 0x7);
            left = byte & 0x1;
        } else if (i % 3 == 2) {
            pixels.push_back((left << 2)  | ((byte >> 6) & 0x3));
            pixels.push_back((byte >> 3) & 0x7);
            pixels.push_back(byte & 0x7);
            left = 0;
        }
    }
    if (bytes.size() % 3 != 0) {
        pixels.push_back(left << (bytes.size() % 3));
    }
    return pixels;
}

Bytes Pixel8Codec::PixelsToBytes(const Pixels& pixels) {
    Bytes bytes;
    Byte byte = 0;
    for (size_t i = 0; i < pixels.size(); ++i) {
        Pixel pixel = pixels[i];
        if (i % 8 == 0) {
            byte = pixel << 5;
        } else if (i % 8 == 1) {
            byte = byte | pixel << 2;
        } else if (i % 8 == 2) {
            byte = byte | pixel >> 1;
            bytes.push_back(byte);
            byte = (pixel & 0x1) << 7;
        } else if (i % 8 == 3) {
            byte = byte | pixel << 4;
        } else if (i % 8 == 4) {
            byte = byte | pixel << 1;
        } else if (i % 8 == 5) {
            byte = byte | pixel >> 2;
            bytes.push_back(byte);
            byte = (pixel & 0x3) << 6;
        } else if (i % 8 == 6) {
            byte = byte | pixel << 3;
        } else if (i % 8 == 7) {
            byte = byte | pixel;
            bytes.push_back(byte);
        }
    }
    return bytes;
}

std::unique_ptr<PixelCodec> create_pixel_codec(PixelType pixel_type) {
    std::unique_ptr<PixelCodec> pixel_codec;
    if (pixel_type == PixelType::PIXEL2) {
        pixel_codec = std::make_unique<Pixel2Codec>();
    } else if (pixel_type == PixelType::PIXEL4) {
        pixel_codec = std::make_unique<Pixel4Codec>();
    } else if (pixel_type == PixelType::PIXEL8) {
        pixel_codec = std::make_unique<Pixel8Codec>();
    } else {
        throw std::invalid_argument("invalid pixel type '" + std::to_string(static_cast<int>(pixel_type)) + "'");
    }
    return pixel_codec;
}
