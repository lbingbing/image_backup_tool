#include <iostream>
#include <exception>
#include <chrono>

#include <boost/program_options.hpp>

#include "image_codec.h"

int main(int argc, char** argv) {
    try {
        std::string server_type_str;
        int port = 0;
        int task_byte_num = 0;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("server_type", boost::program_options::value<std::string>(&server_type_str), "server type");
        desc_handler("port", boost::program_options::value<int>(&port), "port");
        desc_handler("task_byte_num", boost::program_options::value<int>(&task_byte_num), "task byte num");
        boost::program_options::positional_options_description p_desc;
        p_desc.add("server_client_type", 1);
        p_desc.add("port", 1);
        p_desc.add("task_byte_num", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        check_positional_options(p_desc, vm);

        auto server_type = parse_server_type(server_type_str);

        Bytes task_bytes1;
        for (int i = 0; i < task_byte_num; ++i) {
            task_bytes1.push_back(static_cast<uint8_t>(i & 0xff));
        }

        auto task_status_server = create_task_status_server(server_type);
        task_status_server->UpdateTaskStatus(task_bytes1);
        task_status_server->Start(port);

        auto task_status_client = create_task_status_client(server_type, "127.0.0.1", port);
        bool success = false;
        for (int i = 0; i < 3; ++i) {
            Bytes task_bytes2 = task_status_client->GetTaskStatus();
            if (task_bytes1 == task_bytes2) {
                success = true;
                break;
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        task_status_server->Stop();

        if (success) {
            std::cout << "pass\n";
        } else {
            std::cout << "fail\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
