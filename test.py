import os
import platform
import subprocess

def run(cmd, timeout=None):
    system_name = platform.system()
    if system_name == 'Linux':
        env = {
            'PATH': os.environ.get('PATH', '') + ':.',
            'LD_LIBRARY_PATH': os.environ.get('LD_LIBRARY_PATH', '') + ':.',
            }
    elif system_name == 'Darwin':
        env = {
            'PATH': os.environ.get('PATH', '') + ':.',
            'DYLD_LIBRARY_PATH': os.environ.get('DYLD_LIBRARY_PATH', '') + ':.',
            }
    else:
        env = None
    print(' '.join(cmd))
    res = subprocess.run(cmd, timeout=timeout, env=env)
    return res.returncode == 0

os.chdir('image_backup_tool_release')

def test_test_symbol_codec_p():
    assert run(['python', 'test_symbol_codec.py'])

def test_test_symbol_codec_c():
    assert run(['test_symbol_codec'])

def test_test_symbol_codec_python_c():
    assert run(['python', 'test_symbol_codec_python_c.py'])

def test_test_thread_safe_queue():
    assert run(['test_thread_safe_queue'])

def test_test_image_codec_p2p():
    assert run(['python', 'test_image_codec.py', 'p2p', 'test_target_file', '256', 'symbol2', '1,1,10,10', '10', '10', '9123'], timeout=60)

def test_test_image_codec_c2c():
    assert run(['python', 'test_image_codec.py', 'c2c', 'test_target_file', '256', 'symbol2', '1,1,10,10', '10', '10', '9123'], timeout=60)

def test_test_image_codec_p2c():
    assert run(['python', 'test_image_codec.py', 'p2c', 'test_target_file', '256', 'symbol2', '1,1,10,10', '10', '10', '9123'], timeout=60)

def test_test_image_codec_c2p():
    assert run(['python', 'test_image_codec.py', 'c2p', 'test_target_file', '256', 'symbol2', '1,1,10,10', '10', '10', '9123'], timeout=60)
