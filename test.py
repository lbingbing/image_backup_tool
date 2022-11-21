import os

os.chdir('bin')

def test_test_pixel_codec_p():
    assert(os.system('python test_pixel_codec.py') == 0)

def test_test_pixel_codec_c():
    assert(os.system('test_pixel_codec') == 0)

def test_test_pixel_codec_python_c():
    assert(os.system('python test_pixel_codec_python_c.py') == 0)

def test_test_thread_safe_queue():
    assert(os.system('test_thread_safe_queue') == 0)
