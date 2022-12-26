import enum

class PixelType(enum.Enum):
    PIXEL2 = 0
    PIXEL4 = 1
    PIXEL8 = 2

class PixelValue(enum.Enum):
    WHITE   = 0
    BLACK   = 1
    RED     = 2
    BLUE    = 3
    GREEN   = 4
    CYAN    = 5
    MAGENTA = 6
    YELLOW  = 7
    UNKNOWN = 8
    NUM     = 9

def parse_pixel_type(pixel_type_str):
    if pixel_type_str == 'pixel2':
        return PixelType.PIXEL2
    elif pixel_type_str == 'pixel4':
        return PixelType.PIXEL4
    elif pixel_type_str == 'pixel8':
        return PixelType.PIXEL8
    else:
        assert 0, "invalid pixel type '{}'".format(pixel_type_str)

class InvalidImageCodecArgument(Exception):
    pass

def parse_dim(dim_str):
    return tuple(map(int, dim_str.split(',')))
