#pragma once

#include <string>
#include <tuple>

#include "image_codec_api.h"

enum class ServerType {
    NONE,
    TCP,
    HTTP,
};

IMAGE_CODEC_API ServerType parse_server_type(const std::string& server_type_str);
IMAGE_CODEC_API std::string get_server_type_str(ServerType server_type);
IMAGE_CODEC_API int parse_server_port(const std::string& server_port_str);
IMAGE_CODEC_API std::tuple<std::string, int> parse_server_addr(const std::string& server_addr_str);
