import struct

import numpy as np
import cv2 as cv

import image_codec_types
import symbol_codec

class Transform:
    def __init__(self):
        self.bbox = [0, 0, 1, 1]
        self.sphere = [0, 0, 0, 0]
        self.filter_level = 0
        self.binarization_threshold = 128
        self.pixelization_threshold = [128, 128, 128]

    def clone(self):
        transform = Transform()
        transform.binarization_threshold = self.binarization_threshold
        transform.filter_level = self.filter_level
        transform.bbox = self.bbox
        transform.sphere = self.sphere
        return transform

    def load(self, path):
        with open(path, 'rb') as f:
            self.bbox = list(struct.unpack('<ffff', f.read(16)))
            self.sphere = list(struct.unpack('<ffff', f.read(16)))
            self.filter_level = struct.unpack('<i', f.read(4))[0]
            self.binarization_threshold = struct.unpack('<i', f.read(4))[0]
            self.pixelization_threshold = list(struct.unpack('<iii', f.read(12)))

    def save(self, path):
        with open(path, 'wb') as f:
            f.write(struct.pack('<ffff', *self.bbox))
            f.write(struct.pack('<ffff', *self.sphere))
            f.write(struct.pack('<i', self.filter_level))
            f.write(struct.pack('<i', self.binarization_threshold))
            f.write(struct.pack('<iii', *self.pixelization_threshold))

def is_white(img, x, y):
    return (len(img.shape) == 2 and img[y][x] == 255) or (len(img.shape) == 3 and img[y][x][0] == 255 and img[y][x][1] == 255 and img[y][x][2] == 255)

def is_black(img, x, y):
    return (len(img.shape) == 2 and img[y][x] == 0) or (len(img.shape) == 3 and img[y][x][0] == 0 and img[y][x][1] == 0 and img[y][x][2] == 0)

def is_red(img, x, y):
    return len(img.shape) == 3 and img[y][x][0] == 0 and img[y][x][1] == 0 and img[y][x][2] == 255

def is_blue(img, x, y):
    return len(img.shape) == 3 and img[y][x][0] == 255 and img[y][x][1] == 0 and img[y][x][2] == 0

def is_green(img, x, y):
    return len(img.shape) == 3 and img[y][x][0] == 0 and img[y][x][1] == 255 and img[y][x][2] == 0

def is_cyan(img, x, y):
    return len(img.shape) == 3 and img[y][x][0] == 255 and img[y][x][1] == 255 and img[y][x][2] == 0

def is_magenta(img, x, y):
    return len(img.shape) == 3 and img[y][x][0] == 255 and img[y][x][1] == 0 and img[y][x][2] == 255

def is_yellow(img, x, y):
    return len(img.shape) == 3 and img[y][x][0] == 0 and img[y][x][1] == 255 and img[y][x][2] == 255

def is_non_white(img, x, y):
    return (len(img.shape) == 2 and img[y][x] != 255) or (len(img.shape) == 3 and (img[y][x][0] != 255 or img[y][x][1] != 255 or img[y][x][2] != 255))

def parse_bbox(bbox_str):
    bbox = []
    fail = False
    try:
        bbox = tuple(map(float, bbox_str.split(',')))
    except Exception:
        fail = True
    if fail or len(bbox) != 4 or bbox[0] < 0 or bbox[1] < 0 or bbox[2] > 1 or bbox[3] > 1:
        raise image_codec_types.InvalidImageCodecArgument('invalid bbox \'{}\''.format(bbox_str))
    return bbox

def get_bbox_str(bbox):
    return ','.join(map(str, bbox))

def get_bbox(img, bbox):
    return (
        int(img.shape[1] * bbox[0]),
        int(img.shape[0] * bbox[1]),
        int(img.shape[1] * bbox[2]),
        int(img.shape[0] * bbox[3]),
        )

def do_crop(img, bbox):
    return img[bbox[1]:bbox[3],bbox[0]:bbox[2]]

def parse_sphere(sphere_str):
    sphere = []
    fail = False
    try:
        sphere = tuple(map(float, sphere_str.replace('n', '-').split(',')))
    except Exception:
        fail = True
    if fail or len(sphere) != 4:
        raise image_codec_types.InvalidImageCodecArgument('invalid sphere coeff \'{}\''.format(sphere_str))
    return sphere

def get_sphere_str(sphere):
    return ','.join(map(str, sphere)).replace('-', 'n')

def do_sphere(img, sphere):
    cols0 = img.shape[1] // 2
    rows0 = img.shape[0] // 2
    cols1 = img.shape[1] - cols0
    rows1 = img.shape[0] - rows0
    img_nw = img[:rows0,:cols0]
    img_sw = img[rows0:,:cols0]
    img_se = img[rows0:,cols0:]
    img_ne = img[:rows0,cols0:]
    src_corners_nw = np.float32(
        (
        (0, 0),
        (-sphere[0] * cols0, rows0),
        (cols0, rows0),
        (cols0, -sphere[1] * rows0),
        )
        )
    src_corners_sw = np.float32(
        (
        (-sphere[0] * cols0, 0),
        (0, rows1),
        (cols0, (1 + sphere[3]) * rows1),
        (cols0, 0),
        )
        )
    src_corners_se = np.float32(
        (
        (0, 0),
        (0, (1 + sphere[3]) * rows1),
        (cols1, rows1),
        ((1 + sphere[2]) * cols1, 0),
        )
        )
    src_corners_ne = np.float32(
        (
        (0, -sphere[1] * rows0),
        (0, rows0),
        ((1 + sphere[2]) * cols1, rows0),
        (cols1, 0),
        )
        )
    dst_corners_nw = np.float32(
        (
        (0, 0),
        (0, rows0),
        (cols0, rows0),
        (cols0, 0),
        )
        )
    dst_corners_sw = np.float32(
        (
        (0, 0),
        (0, rows1),
        (cols0, rows1),
        (cols0, 0),
        )
        )
    dst_corners_se = np.float32(
        (
        (0, 0),
        (0, rows1),
        (cols1, rows1),
        (cols1, 0),
        )
        )
    dst_corners_ne = np.float32(
        (
        (0, 0),
        (0, rows0),
        (cols1, rows0),
        (cols1, 0),
        )
        )
    mat_nw = cv.getPerspectiveTransform(src_corners_nw, dst_corners_nw)
    mat_sw = cv.getPerspectiveTransform(src_corners_sw, dst_corners_sw)
    mat_se = cv.getPerspectiveTransform(src_corners_se, dst_corners_se)
    mat_ne = cv.getPerspectiveTransform(src_corners_ne, dst_corners_ne)
    img_nw1 = cv.warpPerspective(img_nw, mat_nw, (img_nw.shape[1], img_nw.shape[0]), flags=cv.INTER_NEAREST, borderMode=cv.BORDER_CONSTANT, borderValue=(255, 255, 255))
    img_sw1 = cv.warpPerspective(img_sw, mat_sw, (img_sw.shape[1], img_sw.shape[0]), flags=cv.INTER_NEAREST, borderMode=cv.BORDER_CONSTANT, borderValue=(255, 255, 255))
    img_se1 = cv.warpPerspective(img_se, mat_se, (img_se.shape[1], img_se.shape[0]), flags=cv.INTER_NEAREST, borderMode=cv.BORDER_CONSTANT, borderValue=(255, 255, 255))
    img_ne1 = cv.warpPerspective(img_ne, mat_ne, (img_ne.shape[1], img_ne.shape[0]), flags=cv.INTER_NEAREST, borderMode=cv.BORDER_CONSTANT, borderValue=(255, 255, 255))
    img1 = np.empty_like(img)
    img1[:rows0,:cols0] = img_nw1
    img1[rows0:,:cols0] = img_sw1
    img1[rows0:,cols0:] = img_se1
    img1[:rows0,cols0:] = img_ne1
    return img1

def parse_quad(quad_str):
    quad = []
    fail = False
    try:
        quad = list(map(float, quad_str.replace('n', '-').split(',')))
    except Exception:
        fail = True
    if fail or len(quad) != 8:
        raise image_codec_types.InvalidImageCodecArgument('invalid quad coeff \'{}\''.format(quad_str))
    return quad

def get_quad_str(quad):
    return ','.join(map(str, quad)).replace('-', 'n')

def do_quad(img, quad):
    src_corners = np.float32(
        (
        (0, 0),
        (0, img.shape[0]),
        (img.shape[1], img.shape[0]),
        (img.shape[1], 0),
        )
        )
    dst_corners = np.float32(
        (
        (quad[0] * img.shape[1], quad[1] * img.shape[0]),
        (quad[2] * img.shape[1], (1 + quad[3]) * img.shape[0]),
        ((1 + quad[4]) * img.shape[1], (1 + quad[5]) * img.shape[0]),
        ((1 + quad[6]) * img.shape[1], quad[7] * img.shape[0]),
        )
        )
    mat = cv.getPerspectiveTransform(src_corners, dst_corners)
    img1 = cv.warpPerspective(img, mat, (img.shape[1], img.shape[0]), flags=cv.INTER_NEAREST, borderMode=cv.BORDER_CONSTANT, borderValue=(255, 255, 255))
    return img1

def detect_non_white_corner_nw(img):
    row_num = img.shape[0]
    col_num = img.shape[1]
    i0 = 0
    j0 = 0
    while True:
        i = i0
        j = j0
        while True:
            if is_non_white(img, j, i):
                return j, i
            if i > 0:
                i -= 1
            else:
                break
            if j < col_num - 1:
                j += 1
            else:
                break
        if i0 < row_num - 1:
            i0 += 1
        elif j0 < col_num - 1:
            j0 += 1
        else:
            break
    return 0, 0

def detect_non_white_corner_ne(img):
    row_num = img.shape[0]
    col_num = img.shape[1]
    i0 = 0
    j0 = col_num - 1
    while True:
        i = i0
        j = j0
        while True:
            if is_non_white(img, j, i):
                return j, i
            if i < row_num - 1:
                i += 1
            else:
                break
            if j < col_num - 1:
                j += 1
            else:
                break
        if j0 > 0:
            j0 -= 1
        elif i0 < row_num - 1:
            i0 += 1
        else:
            break
    return 0, 0

def detect_non_white_corner_sw(img):
    row_num = img.shape[0]
    col_num = img.shape[1]
    i0 = row_num - 1
    j0 = 0
    while True:
        i = i0
        j = j0
        while True:
            if is_non_white(img, j, i):
                return j, i
            if i < row_num - 1:
                i += 1
            else:
                break
            if j < col_num - 1:
                j += 1
            else:
                break
        if i0 > 0:
            i0 -= 1
        elif j0 < col_num - 1:
            j0 += 1
        else:
            break
    return 0, 0

def detect_non_white_corner_se(img):
    row_num = img.shape[0]
    col_num = img.shape[1]
    i0 = row_num - 1
    j0 = col_num - 1
    while True:
        i = i0
        j = j0
        while True:
            if is_non_white(img, j, i):
                return j, i
            if i > 0:
                i -= 1
            else:
                break
            if j < col_num - 1:
                j += 1
            else:
                break
        if j0 > 0:
            j0 -= 1
        elif i0 > 0:
            i0 -= 1
        else:
            break
    return 0, 0

def do_auto_quad(img, binarization_threshold):
    img_b = do_binarize(img, binarization_threshold)
    x0, y0 = detect_non_white_corner_nw(img_b)
    x1, y1 = detect_non_white_corner_sw(img_b)
    x2, y2 = detect_non_white_corner_se(img_b)
    x3, y3 = detect_non_white_corner_ne(img_b)
    src_corners = np.float32(
        (
        (x0, y0),
        (x1, y1),
        (x2, y2),
        (x3, y3),
        )
        )
    dst_corners = np.float32(
        (
        (0, 0),
        (0, img.shape[0]),
        (img.shape[1], img.shape[0]),
        (img.shape[1], 0),
        )
        )
    mat = cv.getPerspectiveTransform(src_corners, dst_corners)
    img1 = cv.warpPerspective(img, mat, (img.shape[1], img.shape[0]), flags=cv.INTER_NEAREST, borderMode=cv.BORDER_CONSTANT, borderValue=(255, 255, 255))
    return img1

def parse_filter_level(filter_level_str):
    filter_level = 0
    fail = False
    try:
        filter_level = int(filter_level_str)
    except Exception:
        fail = True
    if fail or filter_level < 0:
        raise image_codec_types.InvalidImageCodecArgument('invalid filter level \'{}\''.format(filter_level_str))
    return filter_level

def get_filter_level_str(filter_level):
    return str(filter_level)

def do_filter(img, filter_level):
    img1 = img
    if filter_level >= 4:
        img1 = cv.medianBlur(img1, 9)
    if filter_level >= 3:
        img1 = cv.medianBlur(img1, 7)
    if filter_level >= 2:
        img1 = cv.medianBlur(img1, 5)
    if filter_level >= 1:
        img1 = cv.medianBlur(img1, 3)
    return img1

def parse_binarization_threshold(binarization_threshold_str):
    binarization_threshold = 0
    fail = False
    try:
        binarization_threshold = int(binarization_threshold_str)
    except Exception:
        fail = True
    if fail or binarization_threshold < 0 or binarization_threshold > 255:
        raise image_codec_types.InvalidImageCodecArgument('invalid binarization threshold \'{}\''.format(binarization_threshold_str))
    return binarization_threshold

def get_binarization_threshold_str(binarization_threshold):
    return str(binarization_threshold)

def do_binarize(img, threshold):
    img1 = cv.cvtColor(img, cv.COLOR_BGR2GRAY)
    ret, img1 = cv.threshold(img1, threshold, 255, cv.THRESH_BINARY)
    return img1

def parse_pixelization_threshold(pixelization_threshold_str):
    pixelization_threshold = []
    fail = False
    try:
        pixelization_threshold = tuple(map(float, pixelization_threshold_str.split(',')))
    except Exception:
        fail = True
    if fail or len(pixelization_threshold) != 3 or \
        pixelization_threshold[0] < 0 or pixelization_threshold[0] > 255 or \
        pixelization_threshold[1] < 0 or pixelization_threshold[1] > 255 or \
        pixelization_threshold[2] < 0 or pixelization_threshold[2] > 255:
        raise image_codec_types.InvalidImageCodecArgument('invalid pixelization threshold \'{}\''.format(pixelization_threshold_str))
    return pixelization_threshold

def get_pixelization_threshold_str(pixelization_threshold):
    return ','.join(map(str, pixelization_threshold))

def do_pixelize(img, symbol_type, threshold):
    img1 = np.empty_like(img)
    ret, slice0 = cv.threshold(img[:,:,0], threshold[0], 255, cv.THRESH_BINARY)
    ret, slice1 = cv.threshold(img[:,:,1], threshold[1], 255, cv.THRESH_BINARY)
    ret, slice2 = cv.threshold(img[:,:,2], threshold[2], 255, cv.THRESH_BINARY)
    img1[:,:,0] = slice0
    img1[:,:,1] = slice1
    img1[:,:,2] = slice2
    if symbol_type == symbol_codec.SymbolType.SYMBOL2:
        for y in range(img1.shape[0]):
            for x in range(img1.shape[1]):
                c1 = img1[y][x]
                if c1[0] == 255 and c1[1] == 0 and c1[2] == 255:
                    c = img[y][x]
                    if c[0] > c[2]:
                        c1[0] = 255
                        c1[2] = 0
                    else:
                        c1[0] = 0
                        c1[2] = 255
    elif symbol_type == symbol_codec.SymbolType.SYMBOL1:
        for y in range(img1.shape[0]):
            for x in range(img1.shape[1]):
                c1 = img1[y][x]
                if int(c1[0]) + int(c1[1]) + int(c1[2]) <= 255:
                    c1[0] = 0
                    c1[1] = 0
                    c1[2] = 0
                else:
                    c1[0] = 255
                    c1[1] = 255
                    c1[2] = 255
    return img1

def add_transform_arguments(parser):
    parser.add_argument('--bbox', help='bbox')
    parser.add_argument('--sphere', help='sphere coeff')
    parser.add_argument('--filter_level', help='filter level')
    parser.add_argument('--binarization_threshold', help='binarization threshold')
    parser.add_argument('--pixelization_threshold', help='pixelization threshold')

def get_transform(args):
    transform = Transform()
    if args.bbox:
        transform.bbox = parse_bbox(args.bbox)
    if args.sphere:
        transform.sphere = parse_sphere(args.sphere)
    if args.filter_level:
        transform.filter_level = parse_filter_level(args.filter_level)
    if args.binarization_threshold:
        transform.binarization_threshold = parse_binarization_threshold(args.binarization_threshold)
    if args.pixelization_threshold:
        transform.pixelization_threshold = parse_pixelization_threshold(args.pixelization_threshold)
    return transform

def transform_image(img, transform):
    img1 = img
    img1 = do_crop(img1, get_bbox(img1, transform.bbox))
    img1 = do_sphere(img1, transform.sphere)
    img1 = do_filter(img1, transform.filter_level)
    return img1

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('image_file_path', help='image file')
    add_transform_arguments(parser)
    args = parser.parse_args()

    transform = get_transform(args)

    img = cv.imread(args.image_file_path)
    img = transform_image(img, transform)
    cv.imshow('image', img)
