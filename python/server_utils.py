import enum
import re

import image_codec_types

@enum.unique
class ServerType(enum.Enum):
    NONE = 0
    TCP = 1
    HTTP = 2

def parse_server_type(server_type_str):
    return ServerType[server_type_str.upper()]

def parse_server_port(server_port_str):
    server_port = 0
    fail = False
    try:
        server_port = int(server_port_str)
    except Exception:
        fail = True
    if fail or server_port < 0 or server_port > 100000:
        raise image_codec_types.InvalidImageCodecArgument('invalid server port \'{}\''.format(server_port_str))
    return server_port

def parse_server_addr(server_addr_str):
    pattern = r'(\d+\.\d+\.\d+\.\d+):(\d+)'
    m = re.match(pattern, server_addr_str)
    if m:
        ip = m.group(1)
        port_str = m.group(2)
        port = parse_server_port(port_str)
        return ip, port
    else:
        raise image_codec_types.InvalidImageCodecArgument('invalid server addr \'{}\''.format(server_addr_str))
