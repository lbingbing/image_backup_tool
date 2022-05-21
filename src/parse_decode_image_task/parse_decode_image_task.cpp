#include <iostream>
#include <string>

#include "image_codec.h"

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cout << "parse_decode_image_task file_path show_undone_part_num";
        return 1;
    }
    Task task(argv[1]);
    task.Load();
    task.Print(std::stoll(argv[2]));

    return 0;
}