import os
import sys

import cv2 as cv

import image_codec_types
import symbol_codec
import part_image_utils

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('save_image_dir_path', help='save image dir path')
    parser.add_argument('target_file', help='target file')
    parser.add_argument('symbol_type', help='symbol type')
    parser.add_argument('dim', help='dim as tile_x_num,tile_y_num,tile_x_size,tile_y_size')
    parser.add_argument('pixel_size', type=int, help='pixel size')
    parser.add_argument('space_size', type=int, help='space size')
    args = parser.parse_args()

    if os.path.exists(args.save_image_dir_path):
        assert os.path.isdir(args.save_image_dir_path)
    else:
        os.mkdir(args.save_image_dir_path)

    assert os.path.isfile(args.target_file)

    symbol_type = symbol_codec.parse_symbol_type(args.symbol_type)
    dim = image_codec_types.parse_dim(args.dim)
    codec, part_byte_num, raw_bytes, part_num = part_image_utils.prepare_part_images(args.target_file, symbol_type, dim)
    print('\rpart {}/{}'.format(0, part_num - 1), end='', file=sys.stderr)
    for part_id, img in part_image_utils.generate_part_images(dim, args.pixel_size, args.space_size, codec, part_byte_num, raw_bytes, part_num):
        print('\rpart {}/{}'.format(part_id, part_num - 1), end='', file=sys.stderr)
        img_file_name = part_image_utils.get_part_image_file_name(part_num, part_id)
        img_file_path = os.path.join(args.save_image_dir_path, img_file_name)
        cv.imwrite(img_file_path, img)
    print(file=sys.stderr)
