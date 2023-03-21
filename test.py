import os
import platform
import subprocess

os.chdir('image_backup_tool_release')

def run(cmd, timeout=None):
    if platform.system() in ('Linux', 'Darwin'):
        os.environ['PATH'] = os.environ.get('PATH', '') + ':.'
    print(' '.join(cmd))
    res = subprocess.run(cmd, timeout=timeout)
    return res.returncode == 0

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
