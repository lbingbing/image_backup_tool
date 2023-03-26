#include <regex>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "image_decode_task_status_server.h"

void TaskStatusServer::Start(int port) {
    m_port = port;
    m_running = true;
    m_thread = std::thread(&TaskStatusServer::Worker, this);
}

void TaskStatusServer::Stop() {
    if (m_running) {
        m_running = false;
        RequestSelf();
        m_thread.join();
    }
}

void TaskStatusServer::UpdateTaskStatus(const Bytes& task_bytes) {
    std::lock_guard<std::mutex> lock(m_mtx);
    m_task_bytes = task_bytes;
}

void TaskStatusServer::Worker() {
    while (m_running) {
        HandleRequest();
    }
}

Bytes TaskStatusServer::GetTaskBytes() const {
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_task_bytes;
}

void TaskStatusTcpServer::HandleRequest() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::acceptor acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), GetPort()));
        boost::asio::ip::tcp::socket socket(ioc);
        acceptor.accept(socket);
        Bytes task_bytes = GetTaskBytes();
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

void TaskStatusTcpServer::RequestSelf() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::socket socket(ioc);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), GetPort());
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
}

void TaskStatusHttpServer::HandleRequest() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::acceptor acceptor(ioc, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), GetPort()));
        boost::asio::ip::tcp::socket socket(ioc);
        acceptor.accept(socket);
        Bytes task_bytes = GetTaskBytes();
        boost::beast::flat_buffer buffer;
        boost::beast::http::request<boost::beast::http::string_body> req;
        boost::beast::http::read(socket, buffer, req);
        boost::beast::http::vector_body<Byte>::value_type body(task_bytes.begin(), task_bytes.end());
        boost::beast::http::response<boost::beast::http::vector_body<Byte>> res(std::piecewise_construct, std::make_tuple(std::move(body)), std::make_tuple(boost::beast::http::status::ok, req.version()));
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.prepare_payload();
        boost::beast::http::serializer<false, boost::beast::http::vector_body<Byte>, boost::beast::http::fields> sr(res);
        boost::beast::http::write(socket, sr);
    }
    catch (std::exception& e) {
        //std::cerr << e.what() << "\n";
    }
}

void TaskStatusHttpServer::RequestSelf() {
    try {
        boost::asio::io_context ioc;
        boost::asio::ip::tcp::socket socket(ioc);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address("127.0.0.1"), GetPort());
        socket.connect(endpoint);
        boost::beast::http::request<boost::beast::http::string_body> req(boost::beast::http::verb::get, "/", 10);
        req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        boost::beast::http::write(socket, req);
        boost::beast::flat_buffer buffer;
        boost::beast::http::response<boost::beast::http::vector_body<Byte>> res;
        boost::beast::http::read(socket, buffer, res);
    }
    catch (std::exception& e) {
        //std::cerr << e.what() << "\n";
    }
}

std::unique_ptr<TaskStatusServer> create_task_status_server(ServerType server_type) {
    std::unique_ptr<TaskStatusServer> task_status_server;
    if (server_type == ServerType::TCP) {
        task_status_server = std::make_unique<TaskStatusTcpServer>();
    } else if (server_type == ServerType::HTTP) {
        task_status_server = std::make_unique<TaskStatusHttpServer>();
    } else {
        throw std::invalid_argument("invalid server type '" + std::to_string(static_cast<int>(server_type)) + "'");
    }
    return task_status_server;
}
