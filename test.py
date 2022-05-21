import os

os.chdir('bin')

def test_test_pixel_codec():
    assert(os.system('test_pixel_codec') == 0)

def test_test_thread_safe_queue():
    assert(os.system('test_thread_safe_queue') == 0)
