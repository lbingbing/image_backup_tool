#pragma once

#include <string>

#include "image_codec_api.h"
#include "image_codec_types.h"

class TaskStatusClient {
public:
    IMAGE_CODEC_API TaskStatusClient(const std::string& ip, int port);
    IMAGE_CODEC_API Bytes GetTaskStatus();

private:
    std::string m_ip;
    int m_port = 0;
};
