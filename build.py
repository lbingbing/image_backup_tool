import os

def run(cmd):
    if isinstance(cmd, str):
        print(cmd)
        os.system(cmd)
    elif isinstance(cmd, list):
        for c in cmd:
            print(c)
            ret = os.system(c)
            if ret:
                break
    else:
        assert 0

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument('--debug', action='store_true', help='debug build')
    args = parser.parse_args()

    config = 'debug' if args.debug else 'release'
    run([
        'cmake -B build',
        'cmake --build build --config {} -j 4'.format(config),
        'cmake --install build --config {} --prefix image_backup_tool_release'.format(config),
        ])
