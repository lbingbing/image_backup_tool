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

cmds = [
    'cmake -B build',
    'cmake --build build --config release -j 4',
    'cmake --install build --prefix .',
    'pytest test.py -v',
    ]

run(cmds)
