#include <utility>

#include "base64.h"

namespace {

const char* get_alphabet() {
    static constexpr char tab[] = {
        "ABCDEFGHIJKLMNOP"
        "QRSTUVWXYZabcdef"
        "ghijklmnopqrstuv"
        "wxyz0123456789+/"
    };
    return &tab[0];
}

const char* get_inverse() {
    static constexpr char tab[] = {
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //   0-15
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, //  16-31
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, //  32-47
         52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, //  48-63
         -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, //  64-79
         15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, //  80-95
         -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, //  96-111
         41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, // 112-127
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 128-143
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 144-159
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 160-175
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 176-191
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 192-207
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 208-223
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 224-239
         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  // 240-255
    };
    return &tab[0];
}

size_t encoded_size(size_t n) {
    return 4 * ((n + 2) / 3);
}

size_t decoded_size(size_t n) {
    return n / 4 * 3;
}

size_t encode(void* dest, const void* src, size_t len) {
    char* out = static_cast<char*>(dest);
    const char* in = static_cast<const char*>(src);
    const auto tab = get_alphabet();
    for(auto n = len / 3; n--;) {
        *out++ = tab[ (in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
        *out++ = tab[((in[2] & 0xc0) >> 6) + ((in[1] & 0x0f) << 2)];
        *out++ = tab[  in[2] & 0x3f];
        in += 3;
    }
    switch(len % 3) {
    case 2:
        *out++ = tab[ (in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)];
        *out++ = tab[                         (in[1] & 0x0f) << 2];
        *out++ = '=';
        break;
    case 1:
        *out++ = tab[ (in[0] & 0xfc) >> 2];
        *out++ = tab[((in[0] & 0x03) << 4)];
        *out++ = '=';
        *out++ = '=';
        break;
    case 0:
        break;
    }
    return out - static_cast<char*>(dest);
}

std::pair<size_t, size_t> decode(void* dest, const void* src, size_t len) {
    char* out = static_cast<char*>(dest);
    auto in = reinterpret_cast<const unsigned char*>(src);
    unsigned char c3[3];
    unsigned char c4[4];
    int i = 0;
    int j = 0;
    const auto inverse = get_inverse();
    while(len-- && *in != '=') {
        const auto v = inverse[*in];
        if (v == -1) break;
        ++in;
        c4[i] = v;
        if (++i == 4) {
            c3[0] =  (c4[0]        << 2) + ((c4[1] & 0x30) >> 4);
            c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
            c3[2] = ((c4[2] & 0x3) << 6) +   c4[3];
            for(i = 0; i < 3; i++) {
                *out++ = c3[i];
            }
            i = 0;
        }
    }
    if (i) {
        c3[0] = ( c4[0]        << 2) + ((c4[1] & 0x30) >> 4);
        c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
        c3[2] = ((c4[2] & 0x3) << 6) +   c4[3];
        for(j = 0; j < i - 1; j++) {
            *out++ = c3[j];
        }
    }
    return {out - static_cast<char*>(dest), in - reinterpret_cast<const unsigned char*>(src)};
}

}

Bytes b64encode(const Bytes& bytes) {
    auto len = encoded_size(bytes.size());
    std::vector<uint8_t> bytes1(len);
    encode(bytes1.data(), bytes.data(), bytes.size());
    return bytes1;
}

Bytes b64decode(const Bytes& bytes) {
    auto len = decoded_size(bytes.size());
    std::vector<uint8_t> bytes1(len);
    auto [len_o, len_in] = decode(bytes1.data(), bytes.data(), bytes.size());
    bytes1.resize(len_o);
    return bytes1;
}
