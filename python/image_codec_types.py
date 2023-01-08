import enum

@enum.unique
class PixelType(enum.Enum):
    PIXEL2 = 0
    PIXEL4 = 1
    PIXEL8 = 2

@enum.unique
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
    return PixelType[pixel_type_str.upper()]

class InvalidImageCodecArgument(Exception):
    pass

def parse_dim(dim_str):
    return tuple(map(int, dim_str.split(',')))
