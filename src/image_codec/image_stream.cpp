#include <iostream>
#include <fstream>
#include <regex>

#include "image_stream.h"
#include "base64.h"

CameraImageStream::CameraImageStream(const std::string& url, int scale) {
    if (url.find("http") == 0) {
        m_cap.open(url);
    } else {
        m_cap.open(std::stoi(url));
    }
    if (m_cap.isOpened()) {
        m_cap.set(cv::CAP_PROP_FRAME_WIDTH, m_cap.get(cv::CAP_PROP_FRAME_WIDTH) * scale);
        m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_cap.get(cv::CAP_PROP_FRAME_HEIGHT) * scale);
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
    std::string type("camera");
    std::string camera_url("0");
    int scale = 1;
    size_t buffer_size = 64;
    std::string ip("127.0.0.1");
    int port = 8123;
    std::ifstream cfg_file("image_stream.ini");
    std::regex type_regex("^type=(\\S+)$");
    std::regex camera_regex("^camera=(\\S+)$");
    std::regex scale_regex("^scale=(\\S+)$");
    std::regex buffer_size_regex("^buffer_size=(\\d+)$");
    std::regex ip_regex("^ip=(\\S+)$");
    std::regex port_regex("^port=(\\d+)$");
    std::regex blank_regex("^\\s*(#.*)?$");
    std::smatch match;
    std::string line;
    while (std::getline(cfg_file, line)) {
        if (std::regex_match(line, match, type_regex)) {
            type = match[1].str();
        } else if (std::regex_match(line, match, camera_regex)) {
            camera_url = match[1].str();
        } else if (std::regex_match(line, match, scale_regex)) {
            scale = std::stoi(match[1].str());
        } else if (std::regex_match(line, match, buffer_size_regex)) {
            buffer_size = std::stoull(match[1].str());
        } else if (std::regex_match(line, match, ip_regex)) {
            ip = match[1].str();
        } else if (std::regex_match(line, match, port_regex)) {
            port = std::stoi(match[1].str());
        } else if (std::regex_match(line, match, blank_regex)) {
        } else {
            std::string msg = "unsupported config '" + line + "'";
            std::cerr << msg << "\n";
            throw std::invalid_argument(msg);
        }
    }
    std::unique_ptr<ImageStream> image_stream;
    if (type == "camera") {
        image_stream = std::make_unique<CameraImageStream>(camera_url, scale);
    } else if (type == "pipe") {
        image_stream = std::make_unique<PipeImageStream>(buffer_size);
    } else if (type == "socket") {
        image_stream = std::make_unique<SocketImageStream>(ip, port, buffer_size);
    } else {
        std::string msg = "unsupported type '" + type + "'";
        std::cerr << msg << "\n";
        throw std::invalid_argument(msg);
    }
    return image_stream;
}
