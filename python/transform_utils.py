import PIL.Image
import PIL.ImageFilter
import PIL.ImageDraw

class Transform:
    def __init__(self):
        self.binarization_threshold = 128
        self.filter_level = 0
        self.bbox = None
        self.sphere = None
        self.quad = None

def do_binarize(img, threshold):
    img = img.convert('L')
    return img.point([1 if i > threshold else 0 for i in range(256)], '1')

def do_filter(img, filter_level):
    if filter_level >= 4:
        img = img.filter(PIL.ImageFilter.MedianFilter(size=9))
    if filter_level >= 3:
        img = img.filter(PIL.ImageFilter.MedianFilter(size=7))
    if filter_level >= 2:
        img = img.filter(PIL.ImageFilter.MedianFilter(size=5))
    if filter_level >= 1:
        img = img.filter(PIL.ImageFilter.MedianFilter(size=3))
    return img

def parse_bbox(bbox_str):
    return list(map(float, bbox_str.split(',')))

def get_bbox(img, bbox):
    return [bbox[0] * img.size[0], bbox[1] * img.size[1], bbox[2] * img.size[0],bbox[3] * img.size[1]]

def draw_bbox(img, bbox):
    img1 = img.copy()
    draw = PIL.ImageDraw.Draw(img1)
    draw.rectangle(get_bbox(img1, bbox), outline='#f00')
    return img1

def parse_sphere(sphere_str):
    sphere = list(map(float, sphere_str.replace('n', '-').split(',')))
    assert len(sphere) == 4, 'invalid sphere'
    return sphere

def do_sphere(img, sphere):
    img = img.copy()
    bbox_lu = (0, 0, img.size[0] // 2, img.size[1] // 2)
    bbox_ld = (0, img.size[1] // 2, img.size[0] // 2, img.size[1])
    bbox_ru = (img.size[0] // 2, 0, img.size[0], img.size[1] // 2)
    bbox_rd = (img.size[0] // 2, img.size[1] // 2, img.size[0], img.size[1])
    img_lu = img.crop(bbox_lu)
    img_ld = img.crop(bbox_ld)
    img_ru = img.crop(bbox_ru)
    img_rd = img.crop(bbox_rd)
    transform_data_lu = (0, 0, -sphere[0] * img_lu.size[0], img_lu.size[1], img_lu.size[0], img_lu.size[1], img_lu.size[0], -sphere[1] * img_lu.size[1])
    transform_data_ld = (-sphere[0] * img_ld.size[0], 0, 0, img_ld.size[1], img_ld.size[0], (1 + sphere[3]) * img_ld.size[1], img_ld.size[0], 0)
    transform_data_ru = (0, -sphere[1] * img_ru.size[1], 0, img_ru.size[1], (1 + sphere[2]) * img_ru.size[0], img_ru.size[1], img_ru.size[0], 0)
    transform_data_rd = (0, 0, 0, (1 + sphere[3]) * img_rd.size[1], img_rd.size[0], img_rd.size[1], (1 + sphere[2]) * img_rd.size[0], 0)
    img_lu1 = img_lu.transform(img_lu.size, PIL.Image.QUAD, transform_data_lu, PIL.Image.BICUBIC, fillcolor='#fff')
    img_ld1 = img_ld.transform(img_ld.size, PIL.Image.QUAD, transform_data_ld, PIL.Image.BICUBIC, fillcolor='#fff')
    img_ru1 = img_ru.transform(img_ru.size, PIL.Image.QUAD, transform_data_ru, PIL.Image.BICUBIC, fillcolor='#fff')
    img_rd1 = img_rd.transform(img_rd.size, PIL.Image.QUAD, transform_data_rd, PIL.Image.BICUBIC, fillcolor='#fff')
    img.paste(img_lu1, bbox_lu)
    img.paste(img_ld1, bbox_ld)
    img.paste(img_ru1, bbox_ru)
    img.paste(img_rd1, bbox_rd)
    return img

def parse_quad(quad_str):
    quad = list(map(float, quad_str.replace('n', '-').split(',')))
    assert len(quad) == 8, 'invalid quad'
    return quad

def do_quad(img, quad):
    x0, y0 = quad[0] * img.size[0], quad[1] * img.size[1]
    x1, y1 = quad[2] * img.size[0], (1 + quad[3]) * img.size[1]
    x2, y2 = (1 + quad[4]) * img.size[0], (1 + quad[5]) * img.size[1]
    x3, y3 = (1 + quad[6]) * img.size[0], quad[7] * img.size[1]
    transform_data = (x0, y0, x1, y1, x2, y2, x3, y3)
    return img.transform(img.size, PIL.Image.QUAD, transform_data, PIL.Image.BICUBIC, fillcolor='#fff')

def add_transform_arguments(parser):
    parser.add_argument('--quad', help='quad coeff')
    parser.add_argument('--sphere', help='sphere coeff')
    parser.add_argument('--binarization_threshold', type=int, help='binarization threshold')
    parser.add_argument('--filter_level', type=int, help='filter level')
    parser.add_argument('--bbox', help='crop bbox')

def get_transform(args):
    transform = Transform()
    if args.quad:
        transform.quad = parse_ladder(args.quad)
    if args.sphere:
        transform.sphere = parse_sphere(args.sphere)
    if args.binarization_threshold:
        transform.binarization_threshold = args.binarization_threshold
    if args.filter_level:
        transform.filter_level = args.filter_level
    if args.bbox:
        transform.bbox = parse_bbox(args.bbox)
    return transform

def transform_image(img, transform):
    img1 = img
    img1 = transform_utils.do_binarize(img1, transform.binarization_threshold)
    img1 = transform_utils.do_filter(img1, transform.filter_level)
    if transform.bbox:
        img1 = img1.crop(transform_utils.get_bbox(img1, transform.bbox))
    if transform.sphere:
        img1 = transform_utils.do_sphere(img1, transform.sphere)
    if transform.quad:
        img1 = transform_utils.do_quad(img1, transform.quad)
    return img1

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('image_file_path', help='image file')
    add_transform_arguments(parser)
    args = parser.parse_args()

    transform = get_transform(args)

    img = PIL.Image.open(args.image_file_path)
    img = transform_image(img, transform)
    img.show()
