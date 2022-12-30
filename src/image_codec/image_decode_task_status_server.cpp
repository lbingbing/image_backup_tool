#include <sstream>
#include <boost/asio.hpp>

#include "image_decode_task_status_server.h"

int parse_task_status_server_port(const std::string& task_status_server_port_str) {
    int task_status_server_port = 0;
    bool fail = false;
    try {
        task_status_server_port = std::stoi(task_status_server_port_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || task_status_server_port < 8100 | task_status_server_port > 8200) {
        throw invalid_image_codec_argument("invalid port '" + task_status_server_port_str + "'");
    }
    return task_status_server_port;
}

TaskStatusServer::~TaskStatusServer() {
    Stop();
}

void TaskStatusServer::Stop() {
    if (m_running) {
        m_running = false;
        try {
            boost::asio::io_context io_context;
            boost::asio::ip::tcp::socket socket(io_context);
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), m_port);
            socket.connect(endpoint);
            uint64_t task_byte_len = 0;
            uint64_t done1 = 0;
            while (done1 < sizeof(uint64_t)) {
                done1 += boost::asio::read(socket, boost::asio::buffer(reinterpret_cast<char*>(&task_byte_len) + done1, sizeof(uint64_t) - done1));
            }
            if (task_byte_len) {
                Bytes task_bytes(task_byte_len);
                uint64_t done2 = 0;
                while (done2 < task_byte_len) {
                    done2 += boost::asio::read(socket, boost::asio::buffer(task_bytes.data() + done2, task_byte_len - done2));
                }
            }
        }
        catch (std::exception& e) {
            //std::cerr << e.what() << "\n";
        }
        m_thread.join();
    }
}

void TaskStatusServer::Start(int port) {
    m_port = port;
    m_running = true;
    m_need_update_task_status = true;
    m_thread = std::thread(&TaskStatusServer::Worker, this);
}

void TaskStatusServer::Worker() {
    while (m_running) {
        try {
            boost::asio::io_context io_context;
            boost::asio::ip::tcp::acceptor acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_port));
            boost::asio::ip::tcp::socket socket(io_context);
            acceptor.accept(socket);
            Bytes task_bytes;
            {
                std::lock_guard<std::mutex> lock(m_mtx);
                task_bytes = m_task_bytes;
            }
            uint64_t task_byte_len = task_bytes.size();
            uint64_t done1 = 0;
            while (done1 < sizeof(uint64_t)) {
                done1 += boost::asio::write(socket, boost::asio::buffer(reinterpret_cast<char*>(&task_byte_len) + done1, sizeof(uint64_t) - done1));
            }
            if (task_byte_len) {
                uint64_t done2 = 0;
                while (done2 < task_byte_len) {
                    done2 += boost::asio::write(socket, boost::asio::buffer(task_bytes.data() + done2, task_byte_len - done2));
                }
            }
        }
        catch (std::exception& e) {
            //std::cerr << e.what() << "\n";
        }
    }
}

void TaskStatusServer::UpdateTaskStatus(const Bytes& task_bytes) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_task_bytes = task_bytes;
    m_need_update_task_status = false;
}