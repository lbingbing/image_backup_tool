import os
import shutil

from Quartz import CGWindowListCopyWindowInfo, kCGNullWindowID, kCGWindowListOptionAll

TMP_DIR_PATH = 'snapshot_windows.tmp_dir'

def get_window_numbers(app_name):
    window_numbers = []
    for window in CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID):
        #print(window)
        if window['kCGWindowOwnerName'] == app_name:
            window_numbers.append(window['kCGWindowNumber'])
    return window_numbers

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('app_name', help='app name')
    args = parser.parse_args()

    window_numbers = get_window_numbers(args.app_name)

    if not os.path.isdir(TMP_DIR_PATH):
        os.mkdir(TMP_DIR_PATH)

    for window_number in window_numbers:
        img_path = os.path.join(TMP_DIR_PATH, 'snapshot{}.jpg'.format(window_number))
        cmd = 'screencapture -l {} {}'.format(window_number, img_path)
        print(cmd)
        os.system(cmd)

    input('wait')
    print('cleaning')
    if os.path.isdir(TMP_DIR_PATH):
        shutil.rmtree(TMP_DIR_PATH)
