import sys
import ctypes

import runtime_utils
import clib_utils

runtime_utils.set_env_vars()

clib = clib_utils.load_library('decode_image_stream_qt_l')
clib.run.argtypes = [ctypes.c_int, (ctypes.c_char_p * len(sys.argv))]
clib.run.restype = ctypes.c_int
clib.run(len(sys.argv), (ctypes.c_char_p * len(sys.argv))(b'decode_image_stream_qt', *[e.encode() for e in sys.argv[1:]]))
