#include <iostream>
#include <string>
#include <exception>
#include <filesystem>

#include "image_codec.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cout << "parse_image_decode_task file_path show_undone_part_num\n";
        return 1;
    }
    try {
        Task task(argv[1]);
        if (!std::filesystem::is_regular_file(task.TaskPath())) {
            throw std::invalid_argument("task file '" + task.TaskPath() + "' not found");
        }
        task.Load();
        task.Print(std::stoll(argv[2]));
    }
    catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    return 0;
}
