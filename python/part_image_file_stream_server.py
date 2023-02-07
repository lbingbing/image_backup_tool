import os

import cv2 as cv

import part_image_utils

def gen_images(image_dir_path):
    img_files = [(e, os.path.join(image_dir_path, e)) for e in os.listdir(image_dir_path) if e.endswith('.png')]
    img_files = [(name, path) for name, path in img_files if os.path.isfile(path)]
    img_files.sort()
    for name, path in img_files:
        img = cv.imread(path)
        yield name, img

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('image_dir_path', help='image dir path')
    parser.add_argument('port', type=int, help='server port')
    args = parser.parse_args()

    assert os.path.isdir(args.image_dir_path)
    part_image_utils.start_image_stream_server(gen_images(args.image_dir_path), args.port)
