#pragma once

#include <string>

#include <boost/program_options.hpp>

#include "image_codec_api.h"

IMAGE_CODEC_API void check_positional_options(const boost::program_options::positional_options_description& p_desc, const boost::program_options::variables_map& vm);
IMAGE_CODEC_API void check_is_file(const std::string& file_path);
IMAGE_CODEC_API void check_is_dir(const std::string& dir_path);
