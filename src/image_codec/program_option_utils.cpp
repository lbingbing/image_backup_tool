#include <exception>
#include <filesystem>

#include "program_option_utils.h"

void check_positional_options(const boost::program_options::positional_options_description& p_desc, const boost::program_options::variables_map& vm) {
    for (unsigned i = 0; i < p_desc.max_total_count(); ++i) {
        auto arg = p_desc.name_for_position(i);
        if (!vm.count(arg)) {
            throw std::invalid_argument(arg+" not specified");
        }
    }
}

void check_is_file(const std::string& file_path) {
    if (!std::filesystem::is_regular_file(file_path)) {
        throw std::invalid_argument("file '" + file_path + "' is not found");
    }
}

void check_is_dir(const std::string& dir_path) {
    if (!std::filesystem::is_directory(dir_path)) {
        throw std::invalid_argument("directory '" + dir_path + "' is not found");
    }
}
