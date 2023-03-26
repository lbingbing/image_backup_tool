#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>

#include "image_codec_api.h"
#include "image_codec_types.h"
#include "server_utils.h"

class TaskStatusServer {
public:
    IMAGE_CODEC_API virtual ~TaskStatusServer() {}
    IMAGE_CODEC_API void Start(int port);
    IMAGE_CODEC_API void Stop();
    IMAGE_CODEC_API void UpdateTaskStatus(const Bytes& task_bytes);

protected:
    int GetPort() const { return m_port; };
    Bytes GetTaskBytes() const;

private:
    void Worker();
    virtual void HandleRequest() = 0;
    virtual void RequestSelf() = 0;

    int m_port = 0;

    std::thread m_thread;
    std::atomic<bool> m_running = false;
    Bytes m_task_bytes;
    mutable std::mutex m_mtx;
};

class TaskStatusTcpServer : public TaskStatusServer {
private:
    void HandleRequest() override;
    void RequestSelf() override;
};

class TaskStatusHttpServer : public TaskStatusServer {
private:
    void HandleRequest() override;
    void RequestSelf() override;
};

IMAGE_CODEC_API std::unique_ptr<TaskStatusServer> create_task_status_server(ServerType server_type);
