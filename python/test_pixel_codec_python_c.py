import sys
import ctypes

import image_codec_types
import pixel_codec

def test_pixel_codec(pixel_type_str):
    pixel_codec_p = pixel_codec.create_pixel_codec(image_codec_types.parse_pixel_type(pixel_type_str))
    clib = ctypes.cdll.LoadLibrary('./image_codec.dll')
    clib.create_pixel_codec_c.restype = ctypes.c_void_p
    pixel_codec_c = clib.create_pixel_codec_c(pixel_type_str.encode())

    for i in range(128):
        part_bytes = bytes([j % 256 for j in range(i + 1)])
        for j in range(16):
            part_id = i
            pixel_num = ((pixel_codec_p.meta_byte_num + (i + 1)) * 8 + pixel_codec_p.bit_num_per_pixel - 1) // pixel_codec_p.bit_num_per_pixel + j

            pixels_p = bytes(pixel_codec_p.encode(i, part_bytes, pixel_num))
            success_c = ctypes.c_bool(False)
            part_id_c = ctypes.c_uint32(0)
            padded_part_byte_num = pixel_num * pixel_codec_p.bit_num_per_pixel // 8 - pixel_codec_p.meta_byte_num
            padded_part_bytes_c = bytearray(padded_part_byte_num)
            padded_part_bytes_c_buf = (ctypes.c_uint8 * padded_part_byte_num).from_buffer(padded_part_bytes_c)
            clib.pixel_codec_decode_c.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_bool), ctypes.POINTER(ctypes.c_uint32), ctypes.c_uint8 * padded_part_byte_num, ctypes.c_char_p, ctypes.c_int]
            clib.pixel_codec_decode_c(pixel_codec_c, ctypes.pointer(success_c), ctypes.pointer(part_id_c), padded_part_bytes_c_buf, pixels_p, pixel_num)

            pixels_c = bytearray(pixel_num)
            pixels_c_buf = (ctypes.c_uint8 * pixel_num).from_buffer(pixels_c)
            clib.pixel_codec_encode_c.argtypes = [ctypes.c_void_p, ctypes.c_uint8 * pixel_num, ctypes.c_uint32, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int]
            clib.pixel_codec_encode_c(pixel_codec_c, pixels_c_buf, part_id, part_bytes, len(part_bytes), pixel_num)
            success_p, part_id_p, padded_part_bytes_p = pixel_codec_p.decode(pixels_c)

            if pixels_p != pixels_c or not success_p or not success_c.value or part_id_p != part_id_c.value or padded_part_bytes_p != padded_part_bytes_c:
                print('{} encode {} {} fail'.format(pixel_type_str, part_id, j))
                if pixels_p != pixels_c:
                    print('pixels_p != pixels_c')
                if not success_p or not success_c.value:
                    print('success_p={}'.format(success_p))
                    print('success_c={}'.format(success_c.value))
                if part_id_p != part_id_c.value:
                    print('part_id_p={}'.format(part_id_p))
                    print('part_id_c={}'.format(part_id_c.value))
                if padded_part_bytes_p != padded_part_bytes_c:
                    print('padded_part_bytes_p != padded_part_bytes_c')
                return False
    print('{} pass'.format(pixel_type_str))
    return True

is_pass = True
is_pass = is_pass and test_pixel_codec('pixel2')
is_pass = is_pass and test_pixel_codec('pixel4')
is_pass = is_pass and test_pixel_codec('pixel8')
if is_pass:
    print('pass')
    sys.exit(0)
else:
    print('fail')
    sys.exit(1)
