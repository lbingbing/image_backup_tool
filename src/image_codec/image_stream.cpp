#include <iostream>
#include <fstream>

#include <boost/program_options.hpp>

#include "image_stream.h"
#include "base64.h"
#include "server_utils.h"

CameraImageStream::CameraImageStream(const std::string& url, float scale, int width, int height) {
    if (url.find("http") == 0) {
        m_cap.open(url);
    } else {
        m_cap.open(std::stoi(url));
    }
    if (m_cap.isOpened()) {
        if (scale) {
            m_cap.set(cv::CAP_PROP_FRAME_WIDTH, m_cap.get(cv::CAP_PROP_FRAME_WIDTH) * scale);
            m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_cap.get(cv::CAP_PROP_FRAME_HEIGHT) * scale);
        } else {
            m_cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
            m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        }
    }
}

CameraImageStream::~CameraImageStream() {
    if (m_cap.isOpened()) {
        m_cap.release();
    }
}

cv::Mat CameraImageStream::GetFrame() {
    cv::Mat frame;
    m_cap >> frame;
    return frame;
}

ThreadedImageStream::~ThreadedImageStream() {
    if (m_runnning) {
        m_runnning = false;
        m_thread.join();
    }
}

cv::Mat ThreadedImageStream::GetFrame() {
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this] { return !m_runnning || m_size; });
    cv::Mat frame;
    if (m_size) {
        frame = std::move(m_ring_buffer[m_rptr]);
        IncPtr(m_rptr);
        --m_size;
    }
    return frame;
}

void ThreadedImageStream::Start() {
    m_runnning = true;
    m_thread = std::thread(&ThreadedImageStream::Worker, this);
}

void ThreadedImageStream::Worker() {
    while (m_runnning) {
        Bytes data = FetchData();
        cv::Mat frame;
        if (!data.empty()) {
            frame = cv::imdecode(data, cv::IMREAD_COLOR);
        }
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_ring_buffer[m_wptr] = std::move(frame);
            IncPtr(m_wptr);
            if (m_size == m_ring_buffer.size()) {
                IncPtr(m_rptr);
            } else {
                ++m_size;
            }
            m_cv.notify_one();
        }
    }
}

void ThreadedImageStream::IncPtr(size_t& ptr) {
    ptr = (ptr + 1) % m_ring_buffer.size();
}

PipeImageStream::PipeImageStream(size_t buffer_size) : ThreadedImageStream(buffer_size) {
    Start();
}

Bytes PipeImageStream::FetchData() {
    try {
        uint64_t len = 0;
        uint64_t done1 = 0;
        while (done1 < sizeof(uint64_t)) {
            std::cin.read(reinterpret_cast<char*>(&len) + done1, sizeof(uint64_t) - done1);
            done1 += std::cin.gcount();
        }
        Bytes data(len);
        uint64_t done2 = 0;
        while (done2 < len) {
            std::cin.read(reinterpret_cast<char*>(data.data()) + done2, len - done2);
            done2 += std::cin.gcount();
        }
        return b64decode(data);
    } catch (std::exception& e) {
        return Bytes();
    }
}

SocketImageStream::SocketImageStream(const std::string& addr, int port, size_t buffer_size) : ThreadedImageStream(buffer_size) {
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(addr), port);
    try {
        m_socket.connect(endpoint);
        Start();
    } catch (std::exception& e) {
    }
}

Bytes SocketImageStream::FetchData() {
    try {
        uint64_t len = 0;
        uint64_t done1 = 0;
        while (done1 < sizeof(uint64_t)) {
            done1 += boost::asio::read(m_socket, boost::asio::buffer(reinterpret_cast<char*>(&len) + done1, sizeof(uint64_t) - done1));
        }
        Bytes data(len);
        uint64_t done2 = 0;
        while (done2 < len) {
            done2 += boost::asio::read(m_socket, boost::asio::buffer(data.data() + done2, len - done2));
        }
        return b64decode(data);
    } catch (std::exception& e) {
        return Bytes();
    }
}

std::unique_ptr<ImageStream> create_image_stream() {
    std::string stream_type("camera");
    std::string camera_url("0");
    float scale = 1;
    int width = 800;
    int height = 600;
    size_t buffer_size = 64;
    std::string server("127.0.0.1:80");
    std::ifstream cfg_file("image_stream.ini");
    boost::program_options::options_description desc;
    auto desc_handler = desc.add_options();
    desc_handler("DEFAULT.stream_type", boost::program_options::value<std::string>(&stream_type));
    desc_handler("DEFAULT.camera_url", boost::program_options::value<std::string>(&camera_url));
    desc_handler("DEFAULT.scale", boost::program_options::value<float>(&scale));
    desc_handler("DEFAULT.width", boost::program_options::value<int>(&width));
    desc_handler("DEFAULT.height", boost::program_options::value<int>(&height));
    desc_handler("DEFAULT.buffer_size", boost::program_options::value<size_t>(&buffer_size));
    desc_handler("DEFAULT.server", boost::program_options::value<std::string>(&server));
    boost::program_options::variables_map vm;
    store(parse_config_file(cfg_file, desc, false), vm);
    notify(vm);
    auto [ip, port] = parse_server_addr(server);
    std::unique_ptr<ImageStream> image_stream;
    if (stream_type == "camera") {
        image_stream = std::make_unique<CameraImageStream>(camera_url, scale, width, height);
    } else if (stream_type == "pipe") {
        image_stream = std::make_unique<PipeImageStream>(buffer_size);
    } else if (stream_type == "socket") {
        image_stream = std::make_unique<SocketImageStream>(ip, port, buffer_size);
    }
    return image_stream;
}
