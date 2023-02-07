#pragma once

#include <boost/asio.hpp>
#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "image_codec_types.h"

class ImageStream {
public:
    IMAGE_CODEC_API virtual ~ImageStream() {}
    IMAGE_CODEC_API virtual cv::Mat GetFrame() = 0;
};

class CameraImageStream : public ImageStream {
public:
    IMAGE_CODEC_API CameraImageStream(const std::string& url, int scale = 1);
    IMAGE_CODEC_API ~CameraImageStream();
    IMAGE_CODEC_API cv::Mat GetFrame() override;

private:
    cv::VideoCapture m_cap;
};

class ThreadedImageStream : public ImageStream {
public:
    IMAGE_CODEC_API ThreadedImageStream(size_t buffer_size) : m_ring_buffer(buffer_size) {}
    IMAGE_CODEC_API virtual ~ThreadedImageStream();
    IMAGE_CODEC_API cv::Mat GetFrame() override;

protected:
    IMAGE_CODEC_API void Start();
    IMAGE_CODEC_API virtual Bytes FetchData() = 0;

private:
    void Worker();
    void IncPtr(size_t& ptr);

    std::vector<cv::Mat> m_ring_buffer;
    size_t m_size = 0;
    size_t m_wptr = 0;
    size_t m_rptr = 0;
    std::thread m_thread;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::atomic<bool> m_runnning = false;
};

class PipeImageStream : public ThreadedImageStream {
public:
    IMAGE_CODEC_API PipeImageStream(size_t buffer_size);

protected:
    IMAGE_CODEC_API Bytes FetchData() override;
};

class SocketImageStream : public ThreadedImageStream {
public:
    IMAGE_CODEC_API SocketImageStream(const std::string& addr, int port, size_t buffer_size);

protected:
    IMAGE_CODEC_API Bytes FetchData() override;

private:
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::socket m_socket{io_context};
};

IMAGE_CODEC_API std::unique_ptr<ImageStream> create_image_stream();
