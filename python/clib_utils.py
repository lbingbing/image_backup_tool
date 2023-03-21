import sys
import platform
import ctypes

def load_library(library_name):
    system_name = platform.system()
    if system_name == 'Windows':
        return ctypes.cdll.LoadLibrary('./{}.dll'.format(library_name))
    elif system_name == 'Linux':
        return ctypes.cdll.LoadLibrary('./lib{}.so'.format(library_name))
    elif system_name == 'Darwin':
        return ctypes.cdll.LoadLibrary('./lib{}.dylib'.format(library_name))
    else:
        print('unknown system {}'.format(system_name))
        sys.exit(1)
