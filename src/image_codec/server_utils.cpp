#include <map>
#include <regex>

#include "server_utils.h"
#include "image_codec_types.h"

namespace {

std::map<std::string, ServerType> server_type_str_to_server_type_mapping({
    {"none", ServerType::NONE},
    {"tcp",  ServerType::TCP},
    {"http", ServerType::HTTP},
});

std::map<ServerType, std::string> server_type_to_server_type_str_mapping({
    {ServerType::NONE, "none"},
    {ServerType::TCP,  "tcp"},
    {ServerType::HTTP, "http"},
});

}

ServerType parse_server_type(const std::string& server_type_str) {
    if (server_type_str_to_server_type_mapping.find(server_type_str) == server_type_str_to_server_type_mapping.end()) {
        throw std::invalid_argument("invalid server type '" + server_type_str + "'");
    }
    return server_type_str_to_server_type_mapping[server_type_str];
}

std::string get_server_type_str(ServerType server_type) {
    if (server_type_to_server_type_str_mapping.find(server_type) == server_type_to_server_type_str_mapping.end()) {
        throw std::invalid_argument("invalid server type " + std::to_string(static_cast<int>(server_type)));
    }
    return server_type_to_server_type_str_mapping[server_type];
}

int parse_server_port(const std::string& server_port_str) {
    int server_port = 0;
    bool fail = false;
    try {
        server_port = std::stoi(server_port_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || server_port < 0 || server_port > 100000) {
        throw invalid_image_codec_argument("invalid server port '" + server_port_str + "'");
    }
    return server_port;
}

std::tuple<std::string, int> parse_server_addr(const std::string& server_addr_str) {
    std::regex pattern{"(\\d+\\.\\d+\\.\\d+\\.\\d+):(\\d+)"};
    std::smatch match;
    bool success = std::regex_match(server_addr_str, match, pattern);
    if (success) {
        auto ip = match[1].str();
        auto port_str = match[2].str();
        auto port = parse_server_port(port_str);
        return std::make_tuple(ip, port);
    } else {
        throw invalid_image_codec_argument("invalid server addr '" + server_addr_str + "'");
        return std::make_tuple("", 0);
    }
}
