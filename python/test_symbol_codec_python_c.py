import sys

import symbol_codec
import symbol_codec_c_wrapper
import image_decode_task

def test_symbol_codec(symbol_type_str):
    symbol_type = symbol_codec.parse_symbol_type(symbol_type_str)
    symbol_codec_p = symbol_codec.create_symbol_codec(symbol_type)
    symbol_codec_c = symbol_codec_c_wrapper.SymbolCodec(symbol_type)

    part_id = 0
    for frame_size in range(0, 2048, 7):
        part_byte_num = image_decode_task.get_part_byte_num(symbol_type, (1, 1, frame_size, 1))
        if part_byte_num >= image_decode_task.Task.min_part_byte_num:
            for padding_byte_num in range(0, 64, 3):
                if padding_byte_num < part_byte_num:
                    part_bytes = bytes([i % 256 for i in range(part_byte_num - padding_byte_num)])

                    symbols_p = symbol_codec_p.encode(part_id, part_bytes, frame_size)
                    success_p, part_id_p, padded_part_bytes_p = symbol_codec_p.decode(symbols_p)
                    part_bytes_p = padded_part_bytes_p[:part_byte_num-padding_byte_num]

                    symbols_c = symbol_codec_c.encode(part_id, part_bytes, frame_size)
                    success_c, part_id_c, padded_part_bytes_c = symbol_codec_c.decode(symbols_c)
                    part_bytes_c = padded_part_bytes_c[:part_byte_num-padding_byte_num]

                    if symbols_p != symbols_c or \
                       not success_p or part_id != part_id_p or part_bytes != part_bytes_p or \
                       not success_c or part_id != part_id_c or part_bytes != part_bytes_c:
                        print('{} codec {} {} fail'.format(symbol_type_str, frame_size, padding_byte_num))
                        if symbols_p != symbols_c:
                            print('symbols mismatch')
                            print('symbols_p={}'.format(symbols_p))
                            print('symbols_c={}'.format(symbols_c))
                        if not success_p or part_id != part_id_p or part_bytes != part_bytes_p:
                            print('p decode fail')
                            print('success_p={}'.format(success_p))
                            print('part_id={}'.format(part_id))
                            print('part_id_p={}'.format(part_id_p))
                            print('part_bytes={}'.format(part_bytes))
                            print('part_bytes_p={}'.format(part_bytes_p))
                        if not success_c or part_id != part_id_c or part_bytes != part_bytes_c:
                            print('c decode fail')
                            print('success_c={}'.format(success_c))
                            print('part_id={}'.format(part_id))
                            print('part_id_c={}'.format(part_id_c))
                            print('part_bytes={}'.format(part_bytes))
                            print('part_bytes_c={}'.format(part_bytes_p))
                        return False
                    part_id += 1
    print('{} codec {} tests pass'.format(symbol_type_str, part_id))
    symbol_codec_c.destroy()
    return True

is_pass = True
is_pass = is_pass and test_symbol_codec('symbol1')
is_pass = is_pass and test_symbol_codec('symbol2')
is_pass = is_pass and test_symbol_codec('symbol3')
if is_pass:
    print('pass')
    sys.exit(0)
else:
    print('fail')
    sys.exit(1)
