#include <iostream>
#include <fstream>
#include <exception>

#include <boost/program_options.hpp>

#include "image_codec.h"

int main(int argc, char** argv) {
    try {
        std::string task_file;
        std::string server_type_str;
        std::string ip;
        int port = 0;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("task_file", boost::program_options::value<std::string>(&task_file), "task file");
        desc_handler("server_type", boost::program_options::value<std::string>(&server_type_str), "server type");
        desc_handler("ip", boost::program_options::value<std::string>(&ip), "ip");
        desc_handler("port", boost::program_options::value<int>(&port), "port");
        boost::program_options::positional_options_description p_desc;
        p_desc.add("task_file", 1);
        p_desc.add("server_type", 1);
        p_desc.add("ip", 1);
        p_desc.add("port", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        check_positional_options(p_desc, vm);

        auto task_status_client = create_task_status_client(parse_server_type(server_type_str), ip, port);
        Bytes task_bytes = task_status_client->GetTaskStatus();
        if (task_bytes.empty()) {
            std::cout << "fail to get task status\n";
        } else {
            std::ofstream f(task_file, std::ios_base::binary);
            f.write(reinterpret_cast<char*>(task_bytes.data()), task_bytes.size());
            std::cout << "success\n";
        }
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
