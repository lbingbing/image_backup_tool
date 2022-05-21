#pragma once

#include <fstream>
#include <boost/asio.hpp>
#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "pixel_codec.h"

class ImageFetcher {
public:
    virtual ~ImageFetcher() {}
    virtual cv::Mat GetFrame() = 0;
};

class CameraImageFetcher : public ImageFetcher {
public:
    CameraImageFetcher(const std::string& url, int scale = 1);
    ~CameraImageFetcher();
    cv::Mat GetFrame() override;

private:
    cv::VideoCapture m_cap;
};

class ThreadedImageFetcher : public ImageFetcher {
public:
    ThreadedImageFetcher(size_t buffer_size) : m_ring_buffer(buffer_size) {}
    ~ThreadedImageFetcher();
    cv::Mat GetFrame() override;

protected:
    void Start();
    void Stop();
    virtual Bytes FetchData() = 0;

private:
    void Join();
    void Worker();
    void IncPtr(size_t& ptr);

    std::vector<cv::Mat> m_ring_buffer;
    size_t m_size = 0;
    size_t m_wptr = 0;
    size_t m_rptr = 0;
    std::thread m_thread;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::atomic<bool> m_stop = false;
};

class PipeImageFetcher : public ThreadedImageFetcher {
public:
    PipeImageFetcher(size_t buffer_size);

protected:
    Bytes FetchData() override;
};

class SocketImageFetcher : public ThreadedImageFetcher {
public:
    SocketImageFetcher(const std::string& addr, int port, size_t buffer_size);

protected:
    Bytes FetchData() override;

private:
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket m_socket{ io_context };
};

IMAGE_CODEC_API std::unique_ptr<ImageFetcher> create_image_fetcher();