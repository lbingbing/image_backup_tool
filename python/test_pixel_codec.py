import sys

import pixel_codec

def test_pixel_codec(pixel_type_str):
    pixel_codec1 = pixel_codec.create_pixel_codec(pixel_codec.parse_pixel_type(pixel_type_str))
    for i in range(128):
        b = bytes([j % 256 for j in range(i + 1)])
        for j in range(16):
            pixels = pixel_codec1.encode(i, b, ((pixel_codec1.meta_byte_num + (i + 1)) * 8 + pixel_codec1.bit_num_per_pixel - 1) // pixel_codec1.bit_num_per_pixel + j)
            success, part_id, part_bytes = pixel_codec1.decode(pixels)
            if not success:
                print('{} encode {} {} fail'.format(pixel_type_str, i, j))
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
