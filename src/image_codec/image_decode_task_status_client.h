#pragma once

#include <string>

#include "image_codec_api.h"
#include "image_codec_types.h"
#include "server_utils.h"
#include "image_decode_task_status_server.h"

class TaskStatusClient {
public:
    IMAGE_CODEC_API TaskStatusClient(const std::string& ip, int port);
    IMAGE_CODEC_API virtual ~TaskStatusClient() {}
    IMAGE_CODEC_API virtual Bytes GetTaskStatus() = 0;

protected:
    std::string GetIp() const { return m_ip; }
    int GetPort() const { return m_port; }

private:
    std::string m_ip;
    int m_port = 0;
};

class TaskStatusTcpClient : public TaskStatusClient {
public:
    using TaskStatusClient::TaskStatusClient;
    IMAGE_CODEC_API Bytes GetTaskStatus() override;
};

class TaskStatusHttpClient : public TaskStatusClient {
public:
    using TaskStatusClient::TaskStatusClient;
    IMAGE_CODEC_API Bytes GetTaskStatus() override;
};

IMAGE_CODEC_API std::unique_ptr<TaskStatusClient> create_task_status_client(ServerType server_type, const std::string& ip, int port);
