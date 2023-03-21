import ctypes

import runtime_utils
import clib_utils

runtime_utils.set_env_vars()

clib = clib_utils.load_library('display_qt_l')
clib.run.argtypes = [ctypes.c_int, (ctypes.c_char_p * 1)]
clib.run.restype = ctypes.c_int
clib.run(1, (ctypes.c_char_p * 1)(b'display_qt'))
