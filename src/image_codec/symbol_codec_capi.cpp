#include <algorithm>

#include "image_codec_api.h"
#include "image_codec_types.h"
#include "symbol_codec.h"

extern "C" {

IMAGE_CODEC_API void* create_symbol_codec_c(const char* symbol_type_str) {
    SymbolType symbol_type = parse_symbol_type(symbol_type_str);
    if (symbol_type == SymbolType::SYMBOL1) {
        return new Symbol1Codec;
    } else if (symbol_type == SymbolType::SYMBOL2) {
        return new Symbol2Codec;
    } else if (symbol_type == SymbolType::SYMBOL3) {
        return new Symbol3Codec;
    } else {
        return nullptr;
    }
}

IMAGE_CODEC_API int symbol_codec_meta_byte_num_c() {
    return SymbolCodec::META_BYTE_NUM;
}

IMAGE_CODEC_API int symbol_codec_bit_num_per_symbol_c(void* symbol_codec) {
    return reinterpret_cast<SymbolCodec*>(symbol_codec)->BitNumPerSymbol();
}

IMAGE_CODEC_API void symbol_codec_encode_c(void* symbol_codec, Byte* symbol_byte_p, uint32_t part_id, const Byte* part_byte_p, int part_byte_num, int frame_size) {
    Bytes part_bytes(part_byte_p, part_byte_p+part_byte_num);
    Symbols symbols = reinterpret_cast<SymbolCodec*>(symbol_codec)->Encode(part_id, part_bytes, frame_size);
    std::copy_n(symbols.data(), frame_size, symbol_byte_p);
}

IMAGE_CODEC_API void symbol_codec_decode_c(void* symbol_codec, bool* success_p, uint32_t* part_id_p, Byte* part_byte_p, const Byte* symbol_byte_p, size_t frame_size) {
    Symbols symbols(symbol_byte_p, symbol_byte_p+frame_size);
    auto [success, part_id, part_bytes] = reinterpret_cast<SymbolCodec*>(symbol_codec)->Decode(symbols);
    *success_p = success;
    *part_id_p = part_id;
    std::copy(part_bytes.begin(), part_bytes.end(), part_byte_p);
}

IMAGE_CODEC_API void destroy_symbol_codec_c(void* symbol_codec) {
    delete reinterpret_cast<SymbolCodec*>(symbol_codec);
}

}
