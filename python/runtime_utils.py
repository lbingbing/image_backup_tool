import os
import sys
import platform

def set_env_vars():
    system_name = platform.system()
    if system_name == 'Windows':
        os.environ['QT_QPA_PLATFORM_PLUGIN_PATH'] = './platforms'
    elif system_name == 'Linux':
        os.environ['PATH'] = os.environ.get('PATH', '') + ':.'
    elif system_name == 'Darwin':
        os.environ['PATH'] = os.environ.get('PATH', '') + ':.'
    else:
        print('unknown system {}'.format(system_name))
        sys.exit(1)
