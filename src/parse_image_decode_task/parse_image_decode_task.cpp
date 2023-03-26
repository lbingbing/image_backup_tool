#include <iostream>
#include <exception>
#include <filesystem>

#include <boost/program_options.hpp>

#include "image_codec.h"

int main(int argc, char** argv) {
    try {
        std::string file_path;
        uint32_t show_undone_part_num = 0;
        boost::program_options::options_description desc("usage");
        auto desc_handler = desc.add_options();
        desc_handler("help", "help message");
        desc_handler("file_path", boost::program_options::value<std::string>(&file_path), "file path");
        desc_handler("show_undone_part_num", boost::program_options::value<uint32_t>(&show_undone_part_num), "show undone part num");
        boost::program_options::positional_options_description p_desc;
        p_desc.add("file_path", 1);
        p_desc.add("show_undone_part_num", 1);
        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).positional(p_desc).run(), vm);
        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << "\n";
            return 1;
        }

        check_positional_options(p_desc, vm);

        Task task(file_path);
        check_is_file(task.TaskPath());
        task.Load();
        task.Print(show_undone_part_num);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
