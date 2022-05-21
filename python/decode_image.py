import PIL.Image
import PIL.ImageDraw
import struct

import transform_utils
import pixel_codec

def parse_dim(dim):
    return map(int, dim.split(','))

def get_bbox(img, row_num, col_num):
    w, h = img.size
    data = list(img.getdata())
    pixels = [[0 if e else 1 for e in data[i*w:(i+1)*w]] for i in range(h)]
    row_occupied = ''.join(['1' if any(e) else '0' for e in pixels])
    col_occupied = ''.join(['1' if any(e) else '0' for e in zip(*pixels)])
    try:
        y0 = row_occupied.index('1')
        y1 = row_occupied.rindex('1') + 1
        x0 = col_occupied.index('1')
        x1 = col_occupied.rindex('1') + 1
    except ValueError:
        y0, y1, x0, x1 = 0, 0, 0, 0
    bbox1 = (x0, y0, x1, y1)
    element_h = (y1 - y0) / (row_num + 2)
    element_w = (x1 - x0) / (col_num + 2)
    y0 += element_h
    y1 -= element_h
    x0 += element_w
    x1 -= element_w
    bbox2 = (x0, y0, x1, y1)
    return bbox1, bbox2

def get_pixels(img_b, row_num, col_num):
    w, h = img_b.size
    element_w = w / col_num
    element_h = h / row_num
    pixels = []
    for row_id in range(row_num):
        for col_id in range(col_num):
            if w and h:
                x0 = (col_id + 0.5) * element_w - 1
                y0 = (row_id + 0.5) * element_h - 1
                x1 = (col_id + 0.5) * element_w + 1
                y1 = (row_id + 0.5) * element_h + 1
                element_img = img_b.crop((x0, y0, x1, y1))
                element_data = [0 if e else 1 for e in element_img.getdata()]
                #b = 1 if sum(element_data) / len(element_data) >= 0.5 else 0
                b = 1 if sum(element_data) else 0
            else:
                b = 0
            pixels.append(b)
    return pixels

def get_result_image(img, row_num, col_num, bbox1, bbox2, pixels=None):
    img = img.copy()
    draw = PIL.ImageDraw.Draw(img)
    element_h = (bbox2[3] - bbox2[1]) / row_num
    element_w = (bbox2[2] - bbox2[0]) / col_num
    if pixels:
        r = 4
        valid_length = len(pixels) // 8 * 8
        for i in range(row_num):
            for j in range(col_num):
                index = i * col_num + j
                y = bbox2[1] + (i + 0.5) * element_h
                x = bbox2[0] + (j + 0.5) * element_w
                marker_bbox = (x-r, y-r, x+r, y+r)
                if index < valid_length:
                    if pixels[index]:
                        draw.ellipse(marker_bbox, fill='#f00')
                    else:
                        draw.ellipse(marker_bbox, outline='#f00')
                else:
                    draw.ellipse(marker_bbox, outline='#00f')
    for i in range(row_num + 1):
        x0 = bbox2[0]
        x1 = bbox2[0] + col_num * element_w
        y = bbox2[1] + i * element_h
        draw.line((x0, y, x1, y), fill='#f00')
    for j in range(col_num + 1):
        x = bbox2[0] + j * element_w
        y0 = bbox2[1]
        y1 = bbox2[1] + row_num * element_h
        draw.line((x, y0, x, y1), fill='#f00')
    img = transform_utils.draw_bbox(img, bbox1)
    return img

def decode_image(img, row_num, col_num, transform, result_image=False):
    img_b = transform_image(img, transform)
    bbox1, bbox2 = get_bbox(img_b, row_num, col_num)
    pixels = get_pixels(img_b.crop(bbox2), row_num, col_num)
    if result_image:
        result_img = get_result_image(img_b.convert('RGB'), row_num, col_num, bbox1, bbox2, pixels)
    else:
        result_img = None
    success, part_id, part_bytes = pixel_codec.decode(pixels)
    return success, part_bytes, part_bytes, result_img

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('image_file_path', help='image file')
    parser.add_argument('dim', help='dim as row_num,col_num')
    transform_utils.add_transform_arguments(parser)
    parser.add_argument('--debug', action='store_true', help='debug')
    args = parser.parse_args()

    row_num, col_num = parse_dim(args.dim)
    transform = get_transform(args)
    with PIL.Image.open(args.image_file_path) as img:
        success, part_id, part_bytes = decode_image_to_bytes(img, row_num, col_num, transform, args.debug)
    print(success, part_id if success else None)
