import platform
import ctypes

system_name = platform.system()
if system_name == 'Windows':
    clib = ctypes.cdll.LoadLibrary('./image_codec.dll')
elif system_name == 'Linux':
    clib = ctypes.cdll.LoadLibrary('./libimage_codec.so')
elif system_name == 'Darwin':
    clib = ctypes.cdll.LoadLibrary('./libimage_codec.dylib')
else:
    print('unknown system {}'.format(system_name))
    sys.exit(1)

clib.create_symbol_codec_c.argtypes = [ctypes.c_char_p]
clib.create_symbol_codec_c.restype = ctypes.c_void_p
clib.destroy_symbol_codec_c.argtypes = [ctypes.c_void_p]
clib.symbol_codec_meta_byte_num_c.argtypes = []
clib.symbol_codec_meta_byte_num_c.restype = ctypes.c_int
clib.symbol_codec_bit_num_per_symbol_c.argtypes = [ctypes.c_void_p]
clib.symbol_codec_bit_num_per_symbol_c.restype = ctypes.c_int

class SymbolCodec:
    def __init__(self, symbol_type):
        self.handle = clib.create_symbol_codec_c(symbol_type.name.lower().encode())
        self.meta_byte_num = clib.symbol_codec_meta_byte_num_c()
        self.bit_num_per_symbol = clib.symbol_codec_bit_num_per_symbol_c(self.handle)

    def destroy(self):
        clib.destroy_symbol_codec_c(self.handle)

    def encode(self, part_id, part_bytes, frame_size):
        symbol_bytes = bytearray(frame_size)
        clib.symbol_codec_encode_c.argtypes = [ctypes.c_void_p, ctypes.c_uint8 * frame_size, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
        clib.symbol_codec_encode_c(self.handle, (ctypes.c_uint8 * frame_size).from_buffer(symbol_bytes), part_id, part_bytes, len(part_bytes), frame_size)
        return list(symbol_bytes)

    def decode(self, symbols):
        symbol_ctypes_bytes = bytes(symbols)
        success = ctypes.c_bool(False)
        part_id = ctypes.c_uint32(0)
        padded_part_byte_num = len(symbols) * self.bit_num_per_symbol // 8 - self.meta_byte_num
        padded_part_bytes = bytearray(padded_part_byte_num)
        clib.symbol_codec_decode_c.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_bool), ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint8 * padded_part_byte_num, ctypes.c_char_p, ctypes.c_int]
        clib.symbol_codec_decode_c(self.handle, ctypes.pointer(success), ctypes.pointer(part_id), (ctypes.c_uint8 * padded_part_byte_num).from_buffer(padded_part_bytes), symbol_ctypes_bytes, len(symbols))
        return success.value, part_id.value, padded_part_bytes
