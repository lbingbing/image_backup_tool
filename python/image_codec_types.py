import enum

@enum.unique
class PixelColor(enum.Enum):
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

class InvalidImageCodecArgument(Exception):
    pass

def parse_dim(dim_str):
    return tuple(map(int, dim_str.split(',')))
