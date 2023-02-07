import image_codec_types
import symbol_codec
import part_image_utils

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('target_file', help='target file')
    parser.add_argument('symbol_type', help='symbol type')
    parser.add_argument('dim', help='dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size')
    parser.add_argument('pixel_size', type=int, help='pixel size')
    parser.add_argument('space_size', type=int, help='space size')
    parser.add_argument('port', type=int, help='server port')
    args = parser.parse_args()

    symbol_type = symbol_codec.parse_symbol_type(args.symbol_type)
    dim = image_codec_types.parse_dim(args.dim)
    codec, part_byte_num, raw_bytes, part_num = part_image_utils.prepare_part_images(args.target_file, symbol_type, dim)
    gen_images = map(lambda x: (part_image_utils.get_part_image_file_name(part_num, x[0]), x[1]), part_image_utils.generate_part_images(dim, args.pixel_size, args.space_size, codec, part_byte_num, raw_bytes, part_num))
    part_image_utils.start_image_stream_server(gen_images, args.port)
