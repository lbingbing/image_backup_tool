#pragma once

#include <string>

#include "image_codec_api.h"
#include "image_codec_types.h"

enum class SymbolType {
    SYMBOL1 = 0,
    SYMBOL2 = 1,
    SYMBOL3 = 2,
};

IMAGE_CODEC_API SymbolType parse_symbol_type(const std::string& symbol_type_str);
IMAGE_CODEC_API std::string get_symbol_type_str(SymbolType symbol_type);

using DecodeResult = std::tuple<bool, uint32_t, Bytes>;

class SymbolCodec {
public:
    static constexpr int CRC_BYTE_NUM = 4;
    static constexpr int PART_ID_BYTE_NUM = 4;
    static constexpr int META_BYTE_NUM = CRC_BYTE_NUM + PART_ID_BYTE_NUM;

    IMAGE_CODEC_API virtual ~SymbolCodec() {}
    IMAGE_CODEC_API virtual SymbolType GetSymbolType() const = 0;
    IMAGE_CODEC_API virtual int BitNumPerSymbol() const = 0;
    IMAGE_CODEC_API int SymbolValueNum() const { return 1 << BitNumPerSymbol(); }
    IMAGE_CODEC_API Symbols Encode(uint32_t part_id, const Bytes& part_bytes, int frame_size);
    IMAGE_CODEC_API DecodeResult Decode(const Symbols& symbols);

protected:
    IMAGE_CODEC_API virtual Symbols BytesToSymbols(const Bytes& bytes) = 0;
    IMAGE_CODEC_API virtual Bytes SymbolsToBytes(const Symbols& symbols) = 0;
};

class Symbol1Codec : public SymbolCodec {
protected:
    IMAGE_CODEC_API SymbolType GetSymbolType() const override { return SymbolType::SYMBOL1; }
    IMAGE_CODEC_API int BitNumPerSymbol() const override { return 1; }
    IMAGE_CODEC_API Symbols BytesToSymbols(const Bytes& bytes) override;
    IMAGE_CODEC_API Bytes SymbolsToBytes(const Symbols& symbols) override;
};

class Symbol2Codec : public SymbolCodec {
protected:
    IMAGE_CODEC_API SymbolType GetSymbolType() const override { return SymbolType::SYMBOL2; }
    IMAGE_CODEC_API int BitNumPerSymbol() const override { return 2; }
    IMAGE_CODEC_API Symbols BytesToSymbols(const Bytes& bytes) override;
    IMAGE_CODEC_API Bytes SymbolsToBytes(const Symbols& symbols) override;
};

class Symbol3Codec : public SymbolCodec {
protected:
    IMAGE_CODEC_API SymbolType GetSymbolType() const override { return SymbolType::SYMBOL3; }
    IMAGE_CODEC_API int BitNumPerSymbol() const override { return 3; }
    IMAGE_CODEC_API Symbols BytesToSymbols(const Bytes& bytes) override;
    IMAGE_CODEC_API Bytes SymbolsToBytes(const Symbols& symbols) override;
};

IMAGE_CODEC_API std::unique_ptr<SymbolCodec> create_symbol_codec(SymbolType symbol_type);
