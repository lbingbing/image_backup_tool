#include <iostream>
#include <fstream>

#include "image_codec.h"

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << "test_decode_image_task_status_client target_file_path ip port";
        return 1;
    }

    std::string target_file_path(argv[1]);
    std::string ip(argv[2]);
    int port = std::stoi(argv[3]);
    TaskStatusClient task_status_client(ip, port);
    Bytes task_bytes = task_status_client.GetTaskStatus();
    if (task_bytes.empty()) {
        std::cout << "fail to get task status\n";
    } else {
        std::ofstream f(target_file_path, std::ios_base::binary);
        f.write(reinterpret_cast<char*>(task_bytes.data()), task_bytes.size());
    }
    return 0;
}
