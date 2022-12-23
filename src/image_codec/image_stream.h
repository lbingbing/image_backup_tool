#pragma once

#include <fstream>
#include <boost/asio.hpp>
#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "pixel_codec.h"

class ImageStream {
public:
    virtual ~ImageStream() {}
    virtual cv::Mat GetFrame() = 0;
};

class CameraImageStream : public ImageStream {
public:
    CameraImageStream(const std::string& url, int scale = 1);
    ~CameraImageStream();
    cv::Mat GetFrame() override;

private:
    cv::VideoCapture m_cap;
};

class ThreadedImageStream : public ImageStream {
public:
    ThreadedImageStream(size_t buffer_size) : m_ring_buffer(buffer_size) {}
    virtual ~ThreadedImageStream();
    cv::Mat GetFrame() override;

protected:
    void Start();
    void Stop();
    bool IsStopped() const;
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

class PipeImageStream : public ThreadedImageStream {
public:
    PipeImageStream(size_t buffer_size);

protected:
    Bytes FetchData() override;
};

class SocketImageStream : public ThreadedImageStream {
public:
    SocketImageStream(const std::string& addr, int port, size_t buffer_size);

protected:
    Bytes FetchData() override;

private:
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket m_socket{io_context};
};

IMAGE_CODEC_API std::unique_ptr<ImageStream> create_image_stream();
