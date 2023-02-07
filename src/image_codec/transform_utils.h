#pragma once

#include <string>
#include <array>

#include <boost/program_options.hpp>
#include <opencv2/opencv.hpp>

#include "image_codec_api.h"
#include "symbol_codec.h"

struct Transform {
    using Bbox = std::array<float, 4>;
    using Sphere = std::array<float, 4>;
    using Quad = std::array<float, 8>;
    using PixelizationThreshold = std::array<int, 3>;

    Bbox bbox{0, 0, 1, 1};
    Sphere sphere{0, 0, 0, 0};
    int filter_level = 0;
    int binarization_threshold = 128;
    PixelizationThreshold pixelization_threshold{128, 128, 128};

    IMAGE_CODEC_API void Load(const std::string& path);
    IMAGE_CODEC_API void Save(const std::string& path);
};

IMAGE_CODEC_API std::ostream& operator<<(std::ostream& os, const Transform& transform);

inline bool is_white(const cv::Mat& img, int x, int y) {
    return (img.channels() == 1 && img.at<uchar>(y, x) == 255) || (img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 255, 255));
}

inline bool is_black(const cv::Mat& img, int x, int y) {
    return (img.channels() == 1 && img.at<uchar>(y, x) == 0) || (img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 0, 0));
}

inline bool is_red(const cv::Mat& img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 0, 255);
}

inline bool is_blue(const cv::Mat& img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 0, 0);
}

inline bool is_green(const cv::Mat& img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 255, 0);
}

inline bool is_cyan(const cv::Mat& img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 255, 0);
}

inline bool is_magenta(const cv::Mat& img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(255, 0, 255);
}

inline bool is_yellow(const cv::Mat& img, int x, int y) {
    return img.channels() == 3 && img.at<cv::Vec3b>(y, x) == cv::Vec3b(0, 255, 255);
}

inline bool is_non_white(const cv::Mat& img, int x, int y) {
    return (img.channels() == 1 && img.at<uchar>(y, x) != 255) || (img.channels() == 3 && img.at<cv::Vec3b>(y, x) != cv::Vec3b(255, 255, 255));
}

std::array<int, 2> detect_non_white_corner_nw(const cv::Mat& img);
std::array<int, 2> detect_non_white_corner_ne(const cv::Mat& img);
std::array<int, 2> detect_non_white_corner_sw(const cv::Mat& img);
std::array<int, 2> detect_non_white_corner_se(const cv::Mat& img);

IMAGE_CODEC_API Transform::Bbox parse_bbox(const std::string& bbox_str);
IMAGE_CODEC_API std::string get_bbox_str(const Transform::Bbox& bbox);
IMAGE_CODEC_API std::array<int, 4> get_bbox(const cv::Mat& img, const Transform::Bbox& bbox);
IMAGE_CODEC_API cv::Mat do_crop(const cv::Mat& img, const std::array<int, 4>& bbox);
IMAGE_CODEC_API Transform::Sphere parse_sphere(const std::string& sphere_str);
IMAGE_CODEC_API std::string get_sphere_str(const Transform::Sphere& sphere);
IMAGE_CODEC_API cv::Mat do_sphere(const cv::Mat& img, const Transform::Sphere& sphere);
IMAGE_CODEC_API Transform::Quad parse_quad(const std::string& quad_str);
IMAGE_CODEC_API std::string get_quad_str(const Transform::Quad& quad);
IMAGE_CODEC_API cv::Mat do_quad(const cv::Mat& img, const Transform::Quad& quad);
IMAGE_CODEC_API cv::Mat do_auto_quad(const cv::Mat& img, int binarization_threshold);
IMAGE_CODEC_API int parse_filter_level(const std::string& filter_level_str);
IMAGE_CODEC_API std::string get_filter_level_str(int filter_level);
IMAGE_CODEC_API cv::Mat do_filter(const cv::Mat& img, int filter_level);
IMAGE_CODEC_API int parse_binarization_threshold(const std::string& binarization_threshold_str);
IMAGE_CODEC_API std::string get_binarization_threshold_str(int binarization_threshold);
IMAGE_CODEC_API cv::Mat do_binarize(const cv::Mat& img, int threshold);
IMAGE_CODEC_API Transform::PixelizationThreshold parse_pixelization_threshold(const std::string& pixelization_threshold_str);
IMAGE_CODEC_API std::string get_pixelization_threshold_str(const Transform::PixelizationThreshold& pixelization_threshold);
IMAGE_CODEC_API cv::Mat do_pixelize(const cv::Mat& img, SymbolType symbol_type, Transform::PixelizationThreshold threshold);

IMAGE_CODEC_API void add_transform_options(boost::program_options::options_description_easy_init& desc_handler);
IMAGE_CODEC_API Transform get_transform(const boost::program_options::variables_map& vm);
IMAGE_CODEC_API cv::Mat transform_image(const cv::Mat& img, const Transform& transform);
