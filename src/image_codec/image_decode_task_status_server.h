#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "image_codec_api.h"
#include "image_codec_types.h"

IMAGE_CODEC_API int parse_task_status_server_port(const std::string& task_status_server_port_str);

class TaskStatusServer {
public:
    IMAGE_CODEC_API ~TaskStatusServer();
    IMAGE_CODEC_API void Start(int port);
    IMAGE_CODEC_API void Stop();
    IMAGE_CODEC_API void UpdateTaskStatus(const Bytes& task_status_bytes);
    IMAGE_CODEC_API bool IsRunning() const { return m_running; }
    IMAGE_CODEC_API bool NeedUpdateTaskStatus() { return m_need_update_task_status; }

private:
    void Worker();

    int m_port = 0;

    std::thread m_thread;
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_need_update_task_status = false;
    Bytes m_task_status_bytes;
    std::mutex m_mtx;
};
