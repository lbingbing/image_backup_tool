#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <exception>

#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "pixel_codec.h"

struct Transform {
    std::vector<float> bbox{ 0, 0, 1, 1 };
    std::vector<float> sphere{ 0, 0, 0, 0 };
    int filter_level = 0;
    int binarization_threshold = 128;
    std::vector<int> pixelization_threshold{ 128, 128, 128 };

    IMAGE_CODEC_API void Load(const std::string& path);
    IMAGE_CODEC_API void Save(const std::string& path);
};

IMAGE_CODEC_API std::ostream& operator<<(std::ostream& os, const Transform& transform);

inline bool is_white(const cv::Mat img, int x, int y) {
    return (img.channels() == 1 && img.at<uchar>(y, x) == 255) || (img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 255, 255));
}

inline bool is_black(const cv::Mat img, int x, int y) {
    return (img.channels() == 1 && img.at<uchar>(y, x) == 0) || (img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 0, 0));
}

inline bool is_red(const cv::Mat img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 0, 255);
}

inline bool is_blue(const cv::Mat img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 0, 0);
}

inline bool is_green(const cv::Mat img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 255, 0);
}

inline bool is_cyan(const cv::Mat img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 255, 0);
}

inline bool is_magenta(const cv::Mat img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 0, 255);
}

inline bool is_yellow(const cv::Mat img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 255, 255);
}

inline bool is_non_white(const cv::Mat img, int x, int y) {
    return (img.channels() == 1 && img.at<uchar>(y, x) != 255) || (img.channels() == 3 && img.at<cv::Vec3b>(y, x) != cv::Vec3b(255, 255, 255));
}

std::pair<int, int> detect_non_white_corner_nw(const cv::Mat& img);
std::pair<int, int> detect_non_white_corner_ne(const cv::Mat& img);
std::pair<int, int> detect_non_white_corner_sw(const cv::Mat& img);
std::pair<int, int> detect_non_white_corner_se(const cv::Mat& img);

class invalid_transform_argument : public std::invalid_argument {
public:
    invalid_transform_argument(const std::string& what_arg) : std::invalid_argument(what_arg) {}
};

template <typename T>
const char* TypeNameStr = "unknown";
template <>
const char* TypeNameStr<int> = "int";
template <>
const char* TypeNameStr<float> = "float";

template <typename T> inline T stox(const std::string& s);
template <> inline int stox(const std::string& s) { return std::stoi(s); }
template <> inline float stox(const std::string& s) { return std::stof(s); }

template <typename T>
inline std::vector<T> parse_vec(const std::string& str) {
    std::vector<T> vec;
    size_t pos = 0;
    while (true) {
        auto pos1 = str.find(",", pos);
        auto s = str.substr(pos, pos1 == std::string::npos ? std::string::npos : pos1 - pos);
        if (s[0] == 'n') s.replace(0, 1, "-");
        try {
            vec.push_back(stox<T>(s));
        }
        catch (const std::exception&) {
            throw invalid_transform_argument("invalid " + std::string(TypeNameStr<T>) +"s '" + str + "'");
        }
        if (pos1 == std::string::npos) break;
        pos = pos1 + 1;
    }
    return vec;
}

IMAGE_CODEC_API std::vector<float> parse_bbox(const std::string& bbox_str);
IMAGE_CODEC_API std::string get_bbox_str(const std::vector<float>& bbox);
IMAGE_CODEC_API std::vector<int> get_bbox(const cv::Mat& img, const std::vector<float>& bbox);
IMAGE_CODEC_API cv::Mat do_crop(const cv::Mat& img, const std::vector<int>& bbox);
IMAGE_CODEC_API std::vector<float> parse_sphere(const std::string& sphere_str);
IMAGE_CODEC_API std::string get_sphere_str(const std::vector<float>& sphere);
IMAGE_CODEC_API cv::Mat do_sphere(const cv::Mat& img, const std::vector<float>& sphere);
IMAGE_CODEC_API std::vector<float> parse_quad(const std::string& quad_str);
IMAGE_CODEC_API std::string get_quad_str(const std::vector<float>& quad);
IMAGE_CODEC_API cv::Mat do_quad(const cv::Mat& img, const std::vector<float>& quad);
IMAGE_CODEC_API cv::Mat do_auto_quad(const cv::Mat& img, int binarization_threshold);
IMAGE_CODEC_API int parse_filter_level(const std::string& filter_level_str);
IMAGE_CODEC_API std::string get_filter_level_str(int filter_level);
IMAGE_CODEC_API cv::Mat do_filter(const cv::Mat& img, int filter_level);
IMAGE_CODEC_API int parse_binarization_threshold(const std::string& binarization_threshold_str);
IMAGE_CODEC_API std::string get_binarization_threshold_str(int binarization_threshold);
IMAGE_CODEC_API cv::Mat do_binarize(const cv::Mat& img, int threshold);
IMAGE_CODEC_API std::vector<int> parse_pixelization_threshold(const std::string& pixelization_threshold_str);
IMAGE_CODEC_API std::string get_pixelization_threshold_str(const std::vector<int>& pixelization_threshold);
IMAGE_CODEC_API cv::Mat do_pixelize(const cv::Mat& img, PixelType pixel_type, std::vector<int> threshold);
IMAGE_CODEC_API void add_transform_options(boost::program_options::options_description_easy_init& desc_handler);
IMAGE_CODEC_API Transform get_transform(const boost::program_options::variables_map& vm);
IMAGE_CODEC_API cv::Mat transform_image(const cv::Mat& img, const Transform& transform);
