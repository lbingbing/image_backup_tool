#include <boost/asio.hpp>

#include "image_decode_task_status_client.h"

TaskStatusClient::TaskStatusClient(const std::string& ip, int port) : m_ip(ip), m_port(port) {
}

Bytes TaskStatusClient::GetTaskStatus() {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(m_ip), m_port);
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
