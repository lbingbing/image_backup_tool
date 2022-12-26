import struct
import collections

import numpy as np
import cv2

import image_codec_types
import pixel_codec
import transform_utils

class TileCalibration:
    def __init__(self):
        centers = None

class Calibration:
    def __init__(self):
        self.valid = False
        self.dim = None
        self.tiles = None

    def load(self, path):
        with open(path, 'rb') as f:
            self.valid = bool(struct.unpack('<i', f.read(4))[0])
            if valid:
                self.dim = struct.unpack('<IIII', f.read(16))
                tile_x_num, tile_y_num, tile_x_size, tile_y_size = self.dim
                tiles = [[TileCalibration() for j in range(tile_x_num)] for i in range(tile_y_num)]
                for tile_y_id in range(tile_y_num):
                    for tile_x_id in range(tile_x_num):
                        centers = [[(None, None) for j in range(tile_x_size)] for i in range(tile_y_size)]
                        tiles[tile_y_id][tile_x_id].centers = centers
                        for y in range(tile_y_size):
                            for x in range(tile_x_size):
                                cx = struct.unpack('<f', f.read(4))[0]
                                cy = struct.unpack('<f', f.read(4))[0]
                                centers[y][x] = (cx, cy)

    def save(self, path):
        with open(path, 'wb') as f:
            f.write(struct.pack('<i', int(self.valid)))
            if self.valid:
                f.write(struct.pack('<IIII', *self.dim))
                tile_x_num, tile_y_num, tile_x_size, tile_y_size = self.dim
                for tile_y_id in range(tile_y_num):
                    for tile_x_id in range(tile_x_num):
                        centers = tiles[tile_y_id][tile_x_id].centers
                        for y in range(tile_y_size):
                            for x in range(tile_x_size):
                                cx, cy = centers[y][x]
                                f.write(struct.pack('<ff', cx, cy))

def find_non_white_boundary_n(img):
    for i in range(img.shape[0]):
        for j in range(img.shape[1]):
            if transform_utils.is_non_white(img, j, i):
                return i
    return img.shape[0]

def find_non_white_boundary_s(img):
    for i in range(img.shape[0] - 1, -1, -1):
        for j in range(img.shape[1]):
            if transform_utils.is_non_white(img, j, i):
                return i
    return -1

def find_non_white_boundary_w(img):
    for j in range(img.shape[1]):
        for i in range(img.shape[0]):
            if transform_utils.is_non_white(img, j, i):
                return j
    return img.shape[1]

def find_non_white_boundary_e(img):
    for j in range(img.shape[1] - 1, -1, -1):
        for i in range(img.shape[0]):
            if transform_utils.is_non_white(img, j, i):
                return j
    return -1

def find_tile_bbox1(img):
    y0 = find_non_white_boundary_n(img)
    y1 = find_non_white_boundary_s(img)
    x0 = find_non_white_boundary_w(img)
    x1 = find_non_white_boundary_e(img)
    return [x0, y0, x1, y1]

def get_tile_bbox2(bbox1, tile_x_size, tile_y_size):
    x0 = bbox1[0]
    y0 = bbox1[1]
    x1 = bbox1[2]
    y1 = bbox1[3]
    unit_h = (y1 - y0) / (tile_y_size + 2)
    unit_w = (x1 - x0) / (tile_x_size + 2)
    y0 += round(unit_h)
    y1 -= round(unit_h)
    x0 += round(unit_w)
    x1 -= round(unit_w)
    return [x0, y0, x1, y1]

def get_spaces(img):
    space_xs = []
    x_space_start_x = -1
    for j in range(img.shape[1]):
        is_space = True
        for i in range(img.shape[0]):
            if transform_utils.is_non_white(img, j, i):
                is_space = False
                break
        if x_space_start_x >= 0:
            if not is_space:
                space_xs.append((x_space_start_x + j) // 2)
                x_space_start_x = -1
        else:
            if is_space:
                x_space_start_x = j
    if x_space_start_x >= 0:
        space_xs.append((x_space_start_x + img.shape[1]) // 2)
    space_ys = []
    y_space_start_y = -1
    for i in range(img.shape[0]):
        is_space = True
        for j in range(img.shape[1]):
            if transform_utils.is_non_white(img, j, i):
                is_space = False
                break
        if y_space_start_y >= 0:
            if not is_space:
                space_ys.append((y_space_start_y + i) // 2)
                y_space_start_y = -1
        else:
            if is_space:
                y_space_start_y = i
    if y_space_start_y >= 0:
        space_ys.append((y_space_start_y + img.shape[0]) // 2)
    return space_xs, space_ys

def get_tile_bboxes(img, tile_x_num, tile_y_num):
    space_xs, space_ys = get_spaces(img)
    if len(space_ys) != tile_y_num - 1 or len(space_xs) != tile_x_num - 1:
        return False, []
    tile_bboxs = [[None for j in range(tile_x_num)] for i in range(tile_y_num)]
    for tile_y_id in range(tile_y_num):
        for tile_x_id in range(tile_x_num):
            x0 = space_xs[tile_x_id - 1] if tile_x_id > 0 else 0
            y0 = space_ys[tile_y_id - 1] if tile_y_id > 0 else 0
            x1 = space_xs[tile_x_id] if tile_x_id < tile_x_num - 1 else img.shape[1]
            y1 = space_ys[tile_y_id] if tile_y_id < tile_y_num - 1 else img.shape[0]
            tile_bboxs[tile_y_id][tile_x_id] = (x0, y0, x1, y1)
    return True, tile_bboxs

def get_non_white_region_bbox(img, x, y):
    x0 = x
    y0 = y
    x1 = x
    y1 = y
    visited = set()
    q = collections.deque()
    q.append((x, y))
    while q:
        px, py = q.popleft()
        if px - 1 >= 0 and (px - 1, py) not in visited and transform_utils.is_non_white(img, px - 1, py):
            x0 = min(x0, px - 1)
            q.append((px - 1, py))
            visited.add((px - 1, py))
        if px + 1 < img.shape[1] and (px + 1, py) not in visited and transform_utils.is_non_white(img, px + 1, py):
            x1 = max(x1, px + 1)
            q.append((px + 1, py))
            visited.add((px + 1, py))
        if py - 1 >= 0 and (px, py - 1) not in visited and transform_utils.is_non_white(img, px, py - 1):
            y0 = min(y0, py - 1)
            q.append((px, py - 1))
            visited.add((px, py - 1))
        if py + 1 < img.shape[0] and (px, py + 1) not in visited and transform_utils.is_non_white(img, px, py + 1):
            y1 = max(y1, py + 1)
            q.append((px, py + 1))
            visited.add((px, py + 1))
    return x0, y0, x1 + 1, y1 + 1

def get_pixel(img, cx, cy):
    RADIUS = 1.5
    x0 = max(round(cx - RADIUS), 0)
    y0 = max(round(cy - RADIUS), 0)
    x1 = min(round(cx + RADIUS + 1), img.shape[1])
    y1 = min(round(cy + RADIUS + 1), img.shape[0])
    color_num = [0, 0, 0, 0, 0, 0, 0, 0, 0]
    for y in range(y0, y1):
        for x in range(x0, x1):
            if transform_utils.is_white(img, x, y):
                color_num[image_codec_types.PixelValue.WHITE.value] += 1
            elif transform_utils.is_black(img, x, y):
                color_num[image_codec_types.PixelValue.BLACK.value] += 1
            elif transform_utils.is_red(img, x, y):
                color_num[image_codec_types.PixelValue.RED.value] += 1
            elif transform_utils.is_blue(img, x, y):
                color_num[image_codec_types.PixelValue.BLUE.value] += 1
            elif transform_utils.is_green(img, x, y):
                color_num[image_codec_types.PixelValue.GREEN.value] += 1
            elif transform_utils.is_cyan(img, x, y):
                color_num[image_codec_types.PixelValue.CYAN.value] += 1
            elif transform_utils.is_magenta(img, x, y):
                color_num[image_codec_types.PixelValue.MAGENTA.value] += 1
            elif transform_utils.is_yellow(img, x, y):
                color_num[image_codec_types.PixelValue.YELLOW.value] += 1
            else:
                color_num[image_codec_types.PixelValue.UNKNOWN.value] += 1
    pixel = image_codec_types.PixelValue.UNKNOWN.value
    max_num = 0
    for i in range(image_codec_types.PixelValue.NUM.value):
        if color_num[i] > max_num:
            pixel = i
            max_num = color_num[i]
    return pixel

def get_tile_pixels(img, tile_x_size, tile_y_size):
    unit_w = img.shape[1] / tile_x_size
    unit_h = img.shape[0] / tile_y_size
    pixels = []
    for y in range(tile_y_size):
        for x in range(tile_x_size):
            pixel = image_codec_types.PixelValue.UNKNOWN.value
            if img.shape[1] and img.shape[0]:
                cx = (x + 0.5) * unit_w
                cy = (y + 0.5) * unit_h
                pixel = get_pixel(img, cx, cy)
            pixels.append(pixel)
    return pixels

def get_tile_pixels_by_calibration(img, centers):
    pixels = []
    for e in centers:
        for cx, cy in e:
            pixel = get_pixel(img, cx, cy)
            pixels.append(pixel)
    return pixels

def get_result_image(img, tile_x_size, tile_y_size, bbox1, bbox2, pixels):
    img1 = np.copy(img)
    unit_h = (bbox2[3] - bbox2[1]) / tile_y_size
    unit_w = (bbox2[2] - bbox2[0]) / tile_x_size
    if pixels:
        RADIUS = 2
        for i in range(tile_y_size):
            for j in range(tile_x_size):
                index = i * tile_x_size + j
                y = round(bbox2[1] + (i + 0.5) * unit_h)
                x = round(bbox2[0] + (j + 0.5) * unit_w)
                color = (0, 0, 0)
                marker = cv2.MARKER_TILTED_CROSS
                if pixels[index] == image_codec_types.PixelValue.WHITE.value:
                    color = (255, 255, 255)
                elif pixels[index] == image_codec_types.PixelValue.BLACK.value:
                    color = (0, 0, 0)
                elif pixels[index] == image_codec_types.PixelValue.RED.value:
                    color = (0, 0, 255)
                elif pixels[index] == image_codec_types.PixelValue.BLUE.value:
                    color = (255, 0, 0)
                elif pixels[index] == image_codec_types.PixelValue.GREEN.value:
                    color = (0, 255, 0)
                elif pixels[index] == image_codec_types.PixelValue.CYAN.value:
                    color = (255, 255, 0)
                elif pixels[index] == image_codec_types.PixelValue.MAGENTA.value:
                    color = (255, 0, 255)
                elif pixels[index] == image_codec_types.PixelValue.YELLOW.value:
                    color = (0, 255, 255)
                else:
                    marker = cv2.MARKER_CROSS
                cv2.drawMarker(img1, (x, y), color, marker, RADIUS * 2)
    for i in range(tile_y_size):
        x0 = round(bbox2[0])
        x1 = round(bbox2[0] + tile_x_size * unit_w)
        y = round(bbox2[1] + i * unit_h)
        cv2.line(img1, (x0, y), (x1, y), (0, 0, 255))
    for j in range(tile_x_size):
        x = round(bbox2[0] + j * unit_w)
        y0 = round(bbox2[1])
        y1 = round(bbox2[1] + tile_y_size * unit_h)
        cv2.line(img1, (x, y0), (x, y1), (0, 0, 255))
    cv2.rectangle(img1, (bbox1[0], bbox1[1]), (bbox1[2], bbox1[3]), (0, 0, 255))
    return img1

def get_result_image_by_calibration(img, dim, calibration, pixels):
    img1 = np.copy(img)
    tile_x_num, tile_y_num, tile_x_size, tile_y_size = dim
    RADIUS = 2
    for tile_y_id in range(tile_y_num):
        for tile_x_id in range(tile_x_num):
            for y in range(tile_y_size):
                for x in range(tile_x_size):
                    index = ((tile_y_id * tile_x_num + tile_x_id) * tile_y_size + y) * tile_x_size + x
                    cx, cy = calibration.tiles[tile_y_id][tile_x_id].centers[y][x]
                    color = (0, 0, 0)
                    marker = cv2.MARKER_TILTED_CROSS
                    if pixels[index] == image_codec_types.PixelValue.WHITE.value:
                        color = (255, 255, 255)
                    elif pixels[index] == image_codec_types.PixelValue.BLACK.value:
                        color = (0, 0, 0)
                    elif pixels[index] == image_codec_types.PixelValue.RED.value:
                        color = (0, 0, 255)
                    elif pixels[index] == image_codec_types.PixelValue.BLUE.value:
                        color = (255, 0, 0)
                    elif pixels[index] == image_codec_types.PixelValue.GREEN.value:
                        color = (0, 255, 0)
                    elif pixels[index] == image_codec_types.PixelValue.CYAN.value:
                        color = (255, 255, 0)
                    elif pixels[index] == image_codec_types.PixelValue.MAGENTA.value:
                        color = (255, 0, 255)
                    elif pixels[index] == image_codec_types.PixelValue.YELLOW.value:
                        color = (0, 255, 255)
                    else:
                        marker = cv2.MARKER_CROSS
                    cv2.drawMarker(img1, (cx, cy), color, marker, RADIUS * 2)
    return img1

class ImageDecoder:
    def __init__(self, pixel_type):
        self.pixel_codec = pixel_codec.create_pixel_codec(pixel_type)

    def calibrate(self, img, dim, transform, result_image):
        tile_x_num, tile_y_num, tile_x_size, tile_y_size = dim
        img1 = transform_utils.transform_image(img, transform)
        img1_b = transform_utils.do_binarize(img1, transform.binarization_threshold)
        calibration = Calibration()
        result_imgs = [[None for j in range(tile_x_num)] for i in range(tile_y_num)]
        centers = []
        start_x = 0
        start_y = [0 for i in range(tile_x_num * tile_x_size)]
        for y in range(tile_y_num * tile_y_size):
            for x in range(tile_x_num * tile_x_size):
                if start_x >= img1_b.shape[1] or start_y[x] >= img1_b.shape[0]:
                    if result_image:
                        result_imgs[0][0] = cv2.cvtColor(img1_b, cv2.COLOR_GRAY2BGR)
                    return img1, calibration, result_imgs
                img2_b = transform_utils.do_crop(img1_b, (start_x, start_y[x], img1.shape[1], img1.shape[0]))
                corner_x, corner_y = detect_non_white_corner_nw(img2_b)
                x0, y0, x1, y1 = get_non_white_region_bbox(img2_b, corner_x, corner_y)
                cx = (x0 + x1) / 2 + start_x
                cy = (y0 + y1) / 2 + start_y[x]
                centers.append(cx, cy)
                start_x += x1
                start_y[x] += y1
            start_x = 0
        calibration.valid = True
        calibration.dim = dim
        calibration.tiles = [[TileCalibration() for j in range(tile_x_num)] for i in range(tile_y_num)]
        result_img = np.copy(img1)
        RADIUS = 2
        for tile_y_id in range(tile_y_num):
            for tile_x_id in range(tile_x_num):
                calibration.tiles[tile_y_id][tile_x_id].centers = [[(None, None) for j in range(tile_x_size)] for i in range(tile_y_size)]
                for y in range(tile_y_size):
                    for x in range(tile_x_size):
                        index = ((tile_y_id * tile_y_size + y) * tile_x_num + tile_x_id) * tile_x_size + x
                        calibration.tiles[tile_y_id][tile_x_id].centers[y][x] = centers[index]
                        if result_image:
                            cv2.circle(result_img, (centers[index][0], centers[index][1]), RADIUS, (0, 0, 255), cv2.FILLED)
        if result_image:
            result_imgs[0][0] = result_img
        return img1, calibration, result_imgs

    def decode(self, img, dim, transform, calibration, result_image):
        tile_x_num, tile_y_num, tile_x_size, tile_y_size = dim
        img1 = transform_utils.transform_image(img, transform)
        pixels = []
        result_imgs = [[None for j in range(tile_x_num)] for i in range(tile_y_num)]
        if calibration.valid:
            img1_p = transform_utils.do_pixelize(img1, self.pixel_codec.get_pixel_type(), transform.pixelization_threshold)
            for tile_y_id in range(tile_y_num):
                for tile_x_id in range(tile_x_num):
                    tile_pixels = get_tile_pixels_by_calibration(img1_p, calibration.tiles[tile_y_id][tile_x_id].centers)
                    pixels += tile_pixels
            if result_image:
                result_imgs[0][0] = get_result_image_by_calibration(img1, dim, calibration, pixels)
        else:
            img1 = transform_utils.do_auto_quad(img1, transform.binarization_threshold)
            img1_b = transform_utils.do_binarize(img1, transform.binarization_threshold)
            tiling_success, tile_bboxes = get_tile_bboxes(img1_b, tile_x_num, tile_y_num)
            if not tiling_success:
                return False, 0, b'', pixels, img1, result_imgs
            for tile_y_id in range(tile_y_num):
                for tile_x_id in range(tile_x_num):
                    tile_img = transform_utils.do_crop(img1, tile_bboxes[tile_y_id][tile_x_id])
                    tile_img1 = transform_utils.do_auto_quad(tile_img, transform.binarization_threshold)
                    bbox1 = (0, 0, tile_img1.shape[1], tile_img1.shape[0])
                    bbox2 = get_tile_bbox2(bbox1, tile_x_size, tile_y_size)
                    tile_img2 = transform_utils.do_crop(tile_img1, bbox2)
                    tile_img2 = transform_utils.do_pixelize(tile_img2, self.pixel_codec.get_pixel_type(), transform.pixelization_threshold)
                    tile_pixels = get_tile_pixels(tile_img2, tile_x_size, tile_y_size)
                    pixels += tile_pixels
                    if result_image:
                        result_imgs[tile_y_id][tile_x_id] = get_result_image(tile_img1, tile_x_size, tile_y_size, bbox1, bbox2, tile_pixels)
        success, part_id, part_bytes = self.pixel_codec.decode(pixels)
        return success, part_id, part_bytes, pixels, img1, result_imgs
