#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "image_decode_task_status_client.h"

TaskStatusClient::TaskStatusClient(const std::string& ip, int port) : m_ip(ip), m_port(port) {
}

Bytes TaskStatusTcpClient::GetTaskStatus() {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(GetIp()), GetPort());
        socket.connect(endpoint);
        uint64_t task_byte_len = 0;
        uint64_t done1 = 0;
        while (done1 < sizeof(uint64_t)) {
            done1 += boost::asio::read(socket, boost::asio::buffer(reinterpret_cast<char*>(&task_byte_len) + done1, sizeof(uint64_t) - done1));
        }
        Bytes task_bytes(task_byte_len);
        if (task_byte_len) {
            uint64_t done2 = 0;
            while (done2 < task_byte_len) {
                done2 += boost::asio::read(socket, boost::asio::buffer(task_bytes.data() + done2, task_byte_len - done2));
            }
        }
        return task_bytes;
    }
    catch (std::exception& e) {
        //std::cerr << e.what() << "\n";
        return Bytes();
    }
}

Bytes TaskStatusHttpClient::GetTaskStatus() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::socket socket(ioc);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(GetIp()), GetPort());
        socket.connect(endpoint);
        boost::beast::http::request<boost::beast::http::string_body> req(boost::beast::http::verb::get, "/", 10);
        req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        boost::beast::http::write(socket, req);
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::vector_body<Byte>> res;
        boost::beast::http::read(socket, buffer, res);
        return res.body();
    }
    catch (std::exception& e) {
        //std::cerr << e.what() << "\n";
        return Bytes();
    }
}

std::unique_ptr<TaskStatusClient> create_task_status_client(ServerType server_type, const std::string& ip, int port) {
    std::unique_ptr<TaskStatusClient> task_status_client;
    if (server_type == ServerType::TCP) {
        task_status_client = std::make_unique<TaskStatusTcpClient>(ip, port);
    } else if (server_type == ServerType::HTTP) {
        task_status_client = std::make_unique<TaskStatusHttpClient>(ip, port);
    } else {
        throw std::invalid_argument("invalid server type '" + std::to_string(static_cast<int>(server_type)) + "'");
    }
    return task_status_client;
}
