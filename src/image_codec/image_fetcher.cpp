#include <regex>

#include "image_fetcher.h"
#include "base64.h"

CameraImageFetcher::CameraImageFetcher(const std::string& url, int scale) {
    if (url.find("http") == 0) {
        m_cap.open(url);
    } else {
        m_cap.open(std::stoi(url));
    }
    if (!m_cap.isOpened()) {
        throw std::logic_error("No video stream detected");
    }
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, m_cap.get(cv::CAP_PROP_FRAME_WIDTH) * scale);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_cap.get(cv::CAP_PROP_FRAME_HEIGHT) * scale);
}

CameraImageFetcher::~CameraImageFetcher() {
    m_cap.release();
}

cv::Mat CameraImageFetcher::GetFrame() {
    cv::Mat frame;
    m_cap >> frame;
    return frame;
}

ThreadedImageFetcher::~ThreadedImageFetcher() {
    Join();
}

cv::Mat ThreadedImageFetcher::GetFrame() {
    std::unique_lock<std::mutex> lock(m_mtx);
    m_cv.wait(lock, [this] { return m_stop || m_size; });
    cv::Mat frame;
    if (m_size) {
        frame = std::move(m_ring_buffer[m_rptr]);
        IncPtr(m_rptr);
        --m_size;
    }
    return frame;
}

void ThreadedImageFetcher::Start() {
    m_thread = std::thread(&ThreadedImageFetcher::Worker, this);
}

void ThreadedImageFetcher::Stop() {
    m_stop = true;
}

void ThreadedImageFetcher::Join() {
    Stop();
    m_thread.join();
}

void ThreadedImageFetcher::Worker() {
    uint64_t frame_id = 0;
    char len_buf[4] = { 0 };
    Bytes data;
    while (!m_stop) {
        Bytes data = FetchData();
        //std::cout << "fetch frame " << frame_id << "\n";
        cv::Mat frame = cv::imdecode(data, cv::IMREAD_COLOR);
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
        ++frame_id;
    }
}

void ThreadedImageFetcher::IncPtr(size_t& ptr) {
    ptr = (ptr + 1) % m_ring_buffer.size();
}

PipeImageFetcher::PipeImageFetcher(size_t buffer_size) : ThreadedImageFetcher(buffer_size) {
    Start();
}

Bytes PipeImageFetcher::FetchData() {
    char len_buf[4] = { 0 };
    std::cin.read(len_buf, 4);
    if (std::cin.gcount() == 0) {
        Stop();
        return {};
    } else if (std::cin.gcount() != 4) {
        std::ostringstream oss;
        oss << "fail to fetch length bytes, len=" << std::cin.gcount() << ", 4 expected";
        std::string msg = oss.str();
        std::cerr << msg << "\n";
        throw std::logic_error(msg);
    }
    uint32_t len = *reinterpret_cast<uint32_t*>(len_buf);
    Bytes data(len);
    std::cin.read(reinterpret_cast<char*>(data.data()), len);
    if (std::cin.gcount() != len) {
        std::ostringstream oss;
        oss << "fail to fetch data bytes, len=" << std::cin.gcount() << ", " << len << " expected";
        std::string msg = oss.str();
        std::cerr << msg << "\n";
        throw std::logic_error(msg);
    }
    return b64decode(data);
}

SocketImageFetcher::SocketImageFetcher(const std::string& addr, int port, size_t buffer_size) : ThreadedImageFetcher(buffer_size) {
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(addr), port);
    m_socket.connect(endpoint);

    Start();
}

Bytes SocketImageFetcher::FetchData() {
    char len_buf[4] = { 0 };
    boost::system::error_code error;
    auto len1 = boost::asio::read(m_socket, boost::asio::buffer(len_buf), error);
    if (len1 == 0) {
        Stop();
        return {};
    } else if (len1 != 4) {
        std::ostringstream oss;
        oss << "fail to fetch length bytes, len=" << len1 << ", 4 expected";
        std::string msg = oss.str();
        std::cerr << msg << "\n";
        throw std::logic_error(msg);
    }
    uint32_t len = *reinterpret_cast<uint32_t*>(len_buf);
    Bytes data(len);
    auto len2 = boost::asio::read(m_socket, boost::asio::buffer(data), error);
    if (len2 != len) {
        std::ostringstream oss;
        oss << "fail to fetch data bytes, len=" << len2 << ", " << len << " expected";
        std::string msg = oss.str();
        std::cerr << msg << "\n";
        throw std::logic_error(msg);
    }
    return b64decode(data);
}

std::unique_ptr<ImageFetcher> create_image_fetcher() {
    std::string type("camera");
    std::string camera_url("0");
    int scale = 1;
    size_t buffer_size = 16;
    std::string ip("127.0.0.1");
    int port = 8123;
    std::ifstream cfg_file("image_fetcher.ini");
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
    std::unique_ptr<ImageFetcher> image_fetcher;
    if (type == "camera") {
        image_fetcher = std::make_unique<CameraImageFetcher>(camera_url, scale);
    } else if (type == "pipe") {
        image_fetcher = std::make_unique<PipeImageFetcher>(buffer_size);
    } else if (type == "socket") {
        image_fetcher = std::make_unique<SocketImageFetcher>(ip, port, buffer_size);
    } else {
        std::string msg = "unsupported type '" + type + "'";
        std::cerr << msg << "\n";
        throw std::invalid_argument(msg);
    }
    return image_fetcher;
}
