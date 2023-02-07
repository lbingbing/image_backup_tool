#include <iostream>

#include "image_codec.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "test_decode_image_task_status_server port byte_num";
        return 1;
    }

    int port = std::stoi(argv[1]);
    int byte_num = std::stoi(argv[2]);
    TaskStatusServer task_status_server;
    task_status_server.UpdateTaskStatus(Bytes(byte_num, 0));
    task_status_server.Start(port);
    char c = 0;
    while (c != 'q') {
        std::cin >> c;
    }
    task_status_server.Stop();
    std::cout << "quit\n";
    return 0;
}
