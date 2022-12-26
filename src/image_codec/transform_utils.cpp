#include <sstream>
#include <fstream>
#include <exception>

#include "transform_utils.h"

void Transform::Load(const std::string& path) {
    std::ifstream f(path, std::ios_base::binary);
    f.read(reinterpret_cast<char*>(bbox.data()), sizeof(float) * bbox.size());
    f.read(reinterpret_cast<char*>(sphere.data()), sizeof(float) * sphere.size());
    f.read(reinterpret_cast<char*>(&filter_level), sizeof(int));
    f.read(reinterpret_cast<char*>(&binarization_threshold), sizeof(int));
    f.read(reinterpret_cast<char*>(pixelization_threshold.data()), sizeof(int) * pixelization_threshold.size());
}

void Transform::Save(const std::string& path) {
    std::ofstream f(path, std::ios_base::binary);
    f.write(reinterpret_cast<char*>(bbox.data()), sizeof(float) * bbox.size());
    f.write(reinterpret_cast<char*>(sphere.data()), sizeof(float) * sphere.size());
    f.write(reinterpret_cast<char*>(&filter_level), sizeof(int));
    f.write(reinterpret_cast<char*>(&binarization_threshold), sizeof(int));
    f.write(reinterpret_cast<char*>(pixelization_threshold.data()), sizeof(int) * pixelization_threshold.size());
}

template <typename T>
std::string vec_to_string(const std::vector<T>& vec) {
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) oss << ",";
        oss << vec[i];
    }
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Transform& transform) {
    os << "bbox: [" << vec_to_string(transform.bbox) << "]\n";
    os << "sphere: [" << vec_to_string(transform.sphere) << "]\n";
    os << "filter_level: " << transform.filter_level << "\n";
    os << "binarization_threshold: " << transform.binarization_threshold << "\n";
    os << "pixelization_threshold: [" << vec_to_string(transform.pixelization_threshold) << "]\n";
    return os;
}

std::vector<float> parse_bbox(const std::string& bbox_str) {
    std::vector<float> bbox;
    bool fail = false;
    try {
        bbox = parse_vec<float>(bbox_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || bbox.size() != 4 || bbox[0] < 0 || bbox[1] < 0 || bbox[2] > 1 || bbox[3] > 1) {
        throw invalid_image_codec_argument("invalid bbox '" + bbox_str + "'");
    }
    return bbox;
}

std::string get_bbox_str(const std::vector<float>& bbox) {
    return vec_to_string(bbox);
}

std::vector<int> get_bbox(const cv::Mat& img, const std::vector<float>& bbox) {
    return std::vector<int>({
        static_cast<int>(img.cols * bbox[0]),
        static_cast<int>(img.rows * bbox[1]),
        static_cast<int>(img.cols * bbox[2]),
        static_cast<int>(img.rows * bbox[3]),
    });
}

cv::Mat do_crop(const cv::Mat& img, const std::vector<int>& bbox) {
    return cv::Mat(img, cv::Rect(bbox[0], bbox[1], bbox[2] - bbox[0], bbox[3] - bbox[1]));
}

std::vector<float> parse_sphere(const std::string& sphere_str) {
    std::vector<float> sphere;
    bool fail = false;
    try {
        sphere = parse_vec<float>(sphere_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || sphere.size() != 4) {
        throw invalid_image_codec_argument("invalid sphere coeff '" + sphere_str + "'");
    }
    return sphere;
}

std::string get_sphere_str(const std::vector<float>& sphere) {
    return vec_to_string(sphere);
}

cv::Mat do_sphere(const cv::Mat& img, const std::vector<float>& sphere) {
    auto cols0 = img.cols / 2;
    auto rows0 = img.rows / 2;
    auto cols1 = img.cols - cols0;
    auto rows1 = img.rows - rows0;
    cv::Mat img_nw = img(cv::Rect(0, 0, cols0, rows0));
    cv::Mat img_sw = img(cv::Rect(0, rows0, cols0, rows1));
    cv::Mat img_se = img(cv::Rect(cols0, rows0, cols1, rows1));
    cv::Mat img_ne = img(cv::Rect(cols0, 0, cols1, rows0));
    std::vector<cv::Point2f> src_corners_nw{
        {0.f, 0.f},
        {-sphere[0] * cols0, static_cast<float>(rows0)},
        {static_cast<float>(cols0), static_cast<float>(rows0)},
        {static_cast<float>(cols0), -sphere[1] * rows0},
    };
    std::vector<cv::Point2f> src_corners_sw{
        {-sphere[0] * cols0, 0.f},
        {0.f, static_cast<float>(rows1)},
        {static_cast<float>(cols0), (1 + sphere[3]) * rows1},
        {static_cast<float>(cols0), 0.f},
    };
    std::vector<cv::Point2f> src_corners_se{
        {0.f, 0.f},
        {0.f, (1 + sphere[3]) * rows1},
        {static_cast<float>(cols1), static_cast<float>(rows1)},
        {(1 + sphere[2]) * cols1, 0.f},
    };
    std::vector<cv::Point2f> src_corners_ne{
        {0.f, -sphere[1] * rows0},
        {0.f, static_cast<float>(rows0)},
        {(1 + sphere[2]) * cols1, static_cast<float>(rows0)},
        {static_cast<float>(cols1), 0.f},
    };
    std::vector<cv::Point2f> dst_corners_nw{
        {0.f, 0.f},
        {0.f, static_cast<float>(rows0)},
        {static_cast<float>(cols0), static_cast<float>(rows0)},
        {static_cast<float>(cols0), 0.f},
    };
    std::vector<cv::Point2f> dst_corners_sw{
        {0.f, 0.f},
        {0.f, static_cast<float>(rows1)},
        {static_cast<float>(cols0), static_cast<float>(rows1)},
        {static_cast<float>(cols0), 0.f},
    };
    std::vector<cv::Point2f> dst_corners_se{
        {0.f, 0.f},
        {0.f, static_cast<float>(rows1)},
        {static_cast<float>(cols1), static_cast<float>(rows1)},
        {static_cast<float>(cols1), 0.f},
    };
    std::vector<cv::Point2f> dst_corners_ne{
        {0.f, 0.f},
        {0.f, static_cast<float>(rows0)},
        {static_cast<float>(cols1), static_cast<float>(rows0)},
        {static_cast<float>(cols1), 0.f},
    };
    cv::Mat mat_nw = cv::getPerspectiveTransform(src_corners_nw, dst_corners_nw);
    cv::Mat mat_sw = cv::getPerspectiveTransform(src_corners_sw, dst_corners_sw);
    cv::Mat mat_se = cv::getPerspectiveTransform(src_corners_se, dst_corners_se);
    cv::Mat mat_ne = cv::getPerspectiveTransform(src_corners_ne, dst_corners_ne);
    cv::Mat img_nw1;
    cv::Mat img_sw1;
    cv::Mat img_se1;
    cv::Mat img_ne1;
    cv::warpPerspective(img_nw, img_nw1, mat_nw, img_nw.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    cv::warpPerspective(img_sw, img_sw1, mat_sw, img_sw.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    cv::warpPerspective(img_se, img_se1, mat_se, img_se.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    cv::warpPerspective(img_ne, img_ne1, mat_ne, img_ne.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    cv::Mat img1(img.rows, img.cols, img.type());
    img_nw1.copyTo(img1(cv::Rect(0, 0, cols0, rows0)));
    img_sw1.copyTo(img1(cv::Rect(0, rows0, cols1, rows1)));
    img_se1.copyTo(img1(cv::Rect(cols0, rows0, cols1, rows1)));
    img_ne1.copyTo(img1(cv::Rect(cols0, 0, cols1, rows1)));
    return img1;
}

std::vector<float> parse_quad(const std::string& quad_str) {
    std::vector<float> quad;
    bool fail = false;
    try {
        quad = parse_vec<float>(quad_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || quad.size() != 8) {
        throw invalid_image_codec_argument("invalid quad coeff '" + quad_str + "'");
    }
    return quad;
}

std::string get_quad_str(const std::vector<float>& quad) {
    return vec_to_string(quad);
}

cv::Mat do_quad(const cv::Mat& img, const std::vector<float>& quad) {
    std::vector<cv::Point2f> src_corners{
        {0.f, 0.f},
        {0.f, static_cast<float>(img.rows)},
        {static_cast<float>(img.cols), static_cast<float>(img.rows)},
        {static_cast<float>(img.cols), 0.f},
    };
    std::vector<cv::Point2f> dst_corners{
        {quad[0] * img.cols, quad[1] * img.rows},
        {quad[2] * img.cols, (1 + quad[3]) * img.rows},
        {(1 + quad[4]) * img.cols, (1 + quad[5]) * img.rows},
        {(1 + quad[6]) * img.cols, quad[7] * img.rows},
    };
    cv::Mat mat = cv::getPerspectiveTransform(src_corners, dst_corners);
    cv::Mat img1;
    cv::warpPerspective(img, img1, mat, img.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    return img1;
}

std::pair<int, int> detect_non_white_corner_nw(const cv::Mat& img) {
    int row_num = img.rows;
    int col_num = img.cols;
    int i0 = 0;
    int j0 = 0;
    while (true) {
        int i = i0;
        int j = j0;
        while (true) {
            if (is_non_white(img, j, i)) return { j, i };
            if (i > 0) --i;
            else break;
            if (j < col_num - 1) ++j;
            else break;
        }
        if (i0 < row_num - 1) ++i0;
        else if (j0 < col_num - 1) ++j0;
        else break;
    }
    return { 0, 0 };
}

std::pair<int, int> detect_non_white_corner_ne(const cv::Mat& img) {
    int row_num = img.rows;
    int col_num = img.cols;
    int i0 = 0;
    int j0 = col_num - 1;
    while (true) {
        int i = i0;
        int j = j0;
        while (true) {
            if (is_non_white(img, j, i)) return { j, i };
            if (i < row_num - 1) ++i;
            else break;
            if (j < col_num - 1) ++j;
            else break;
        }
        if (j0 > 0) --j0;
        else if (i0 < row_num - 1) ++i0;
        else break;
    }
    return { 0, 0 };
}

std::pair<int, int> detect_non_white_corner_sw(const cv::Mat& img) {
    int row_num = img.rows;
    int col_num = img.cols;
    int i0 = row_num - 1;
    int j0 = 0;
    while (true) {
        int i = i0;
        int j = j0;
        while (true) {
            if (is_non_white(img, j, i)) return { j, i };
            if (i < row_num - 1) ++i;
            else break;
            if (j < col_num - 1) ++j;
            else break;
        }
        if (i0 > 0) --i0;
        else if (j0 < col_num - 1) ++j0;
        else break;
    }
    return { 0, 0 };
}

std::pair<int, int> detect_non_white_corner_se(const cv::Mat& img) {
    int row_num = img.rows;
    int col_num = img.cols;
    int i0 = row_num - 1;
    int j0 = col_num - 1;
    while (true) {
        int i = i0;
        int j = j0;
        while (true) {
            if (is_non_white(img, j, i)) return { j, i };
            if (i > 0) --i;
            else break;
            if (j < col_num - 1) ++j;
            else break;
        }
        if (j0 > 0) --j0;
        else if (i0 > 0) --i0;
        else break;
    }
    return { 0, 0 };
}

cv::Mat do_auto_quad(const cv::Mat& img, int binarization_threshold) {
    cv::Mat img_b = do_binarize(img, binarization_threshold);
    auto [x0, y0] = detect_non_white_corner_nw(img_b);
    auto [x1, y1] = detect_non_white_corner_sw(img_b);
    auto [x2, y2] = detect_non_white_corner_se(img_b);
    auto [x3, y3] = detect_non_white_corner_ne(img_b);
    std::vector<cv::Point2f> src_corners{
        {static_cast<float>(x0), static_cast<float>(y0)},
        {static_cast<float>(x1), static_cast<float>(y1)},
        {static_cast<float>(x2), static_cast<float>(y2)},
        {static_cast<float>(x3), static_cast<float>(y3)},
    };
    std::vector<cv::Point2f> dst_corners{
        {0.f, 0.f},
        {0.f, static_cast<float>(img.rows)},
        {static_cast<float>(img.cols), static_cast<float>(img.rows)},
        {static_cast<float>(img.cols), 0.f},
    };
    cv::Mat mat = cv::getPerspectiveTransform(src_corners, dst_corners);
    cv::Mat img1;
    cv::warpPerspective(img, img1, mat, img.size(), cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    return img1;
}

int parse_filter_level(const std::string& filter_level_str) {
    int filter_level = 0;
    bool fail = false;
    try {
        filter_level = std::stoi(filter_level_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || filter_level < 0) {
        throw invalid_image_codec_argument("invalid filter level '" + filter_level_str + "'");
    }
    return filter_level;
}

std::string get_filter_level_str(int filter_level) {
    return std::to_string(filter_level);
}

cv::Mat do_filter(const cv::Mat& img, int filter_level) {
    cv::Mat img1 = img;
    if (filter_level >= 4) {
        cv::medianBlur(img1, img1, 9);
    }
    if (filter_level >= 3) {
        cv::medianBlur(img1, img1, 7);
    }
    if (filter_level >= 2) {
        cv::medianBlur(img1, img1, 5);
    }
    if (filter_level >= 1) {
        cv::medianBlur(img1, img1, 3);
    }
    return img1;
}

int parse_binarization_threshold(const std::string& binarization_threshold_str) {
    int binarization_threshold = 0;
    bool fail = false;
    try {
        binarization_threshold = std::stoi(binarization_threshold_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || binarization_threshold < 0 || binarization_threshold > 255) {
        throw invalid_image_codec_argument("invalid binarization threshold '" + binarization_threshold_str + "'");
    }
    return binarization_threshold;
}

std::string get_binarization_threshold_str(int binarization_threshold) {
    return std::to_string(binarization_threshold);
}

cv::Mat do_binarize(const cv::Mat& img, int threshold) {
    cv::Mat img1;
    cv::cvtColor(img, img1, cv::COLOR_BGR2GRAY);
    cv::threshold(img1, img1, static_cast<double>(threshold), 255, cv::THRESH_BINARY);
    return img1;
}

std::vector<int> parse_pixelization_threshold(const std::string& pixelization_threshold_str) {
    std::vector<int> pixelization_threshold;
    bool fail = false;
    try {
        pixelization_threshold = parse_vec<int>(pixelization_threshold_str);
    }
    catch (const std::exception&) {
        fail = true;
    }
    if (fail || pixelization_threshold.size() != 3 ||
        pixelization_threshold[0] < 0 || pixelization_threshold[0] > 255 ||
        pixelization_threshold[1] < 0 || pixelization_threshold[1] > 255 ||
        pixelization_threshold[2] < 0 || pixelization_threshold[2] > 255) {
        throw invalid_image_codec_argument("invalid pixelization threshold '" + pixelization_threshold_str + "'");
    }
    return pixelization_threshold;
}

std::string get_pixelization_threshold_str(const std::vector<int>& pixelization_threshold) {
    return vec_to_string(pixelization_threshold);
}

cv::Mat do_pixelize(const cv::Mat& img, PixelType pixel_type, std::vector<int> threshold) {
    cv::Mat img_channels[3];
    cv::split(img, img_channels);
    cv::threshold(img_channels[0], img_channels[0], static_cast<double>(threshold[0]), 255, cv::THRESH_BINARY);
    cv::threshold(img_channels[1], img_channels[1], static_cast<double>(threshold[1]), 255, cv::THRESH_BINARY);
    cv::threshold(img_channels[2], img_channels[2], static_cast<double>(threshold[2]), 255, cv::THRESH_BINARY);
    cv::Mat img1;
    cv::merge(img_channels, 3, img1);
    if (pixel_type == PixelType::PIXEL4) {
        for (int y = 0; y < img1.rows; ++y) {
            for (int x = 0; x < img1.cols; ++x) {
                auto& c1 = img1.at<cv::Vec3b>(y, x);
                if (c1[0] == 255 && c1[1] == 0 && c1[2] == 255) {
                    const auto& c = img.at<cv::Vec3b>(y, x);
                    if (c[0] > c[2]) {
                        c1[0] = 255;
                        c1[2] = 0;
                    } else {
                        c1[0] = 0;
                        c1[2] = 255;
                    }
                }
            }
        }
    } else if (pixel_type == PixelType::PIXEL2) {
        for (int y = 0; y < img1.rows; ++y) {
            for (int x = 0; x < img1.cols; ++x) {
                auto& c1 = img1.at<cv::Vec3b>(y, x);
                if (static_cast<int>(c1[0]) + static_cast<int>(c1[1]) + static_cast<int>(c1[2]) <= 255) {
                    c1[0] = 0;
                    c1[1] = 0;
                    c1[2] = 0;
                } else {
                    c1[0] = 255;
                    c1[1] = 255;
                    c1[2] = 255;
                }
            }
        }
    }
    return img1;
}

void add_transform_options(boost::program_options::options_description_easy_init& desc_handler) {
    desc_handler("bbox", boost::program_options::value<std::string>(), "bbox");
    desc_handler("sphere", boost::program_options::value<std::string>(), "sphere coeff");
    desc_handler("filter_level", boost::program_options::value<std::string>(), "filter level");
    desc_handler("binarization_threshold", boost::program_options::value<std::string>(), "binarization threshold");
    desc_handler("pixelization_threshold", boost::program_options::value<std::string>(), "pixelization threshold");
}

Transform get_transform(const boost::program_options::variables_map& vm) {
    Transform transform;
    if (vm.count("bbox")) {
        transform.bbox = parse_bbox(vm["bbox"].as<std::string>());
    }
    if (vm.count("sphere")) {
        transform.sphere = parse_sphere(vm["sphere"].as<std::string>());
    }
    if (vm.count("filter_level")) {
        transform.filter_level = parse_filter_level(vm["filter_level"].as<std::string>());
    }
    if (vm.count("binarization_threshold")) {
        transform.binarization_threshold = parse_binarization_threshold(vm["binarization_threshold"].as<std::string>());
    }
    if (vm.count("pixelization_threshold")) {
        transform.pixelization_threshold = parse_pixelization_threshold(vm["pixelization_threshold"].as<std::string>());
    }
    return transform;
}

cv::Mat transform_image(const cv::Mat& img, const Transform& transform) {
    cv::Mat img1 = img;
    img1 = do_crop(img1, get_bbox(img1, transform.bbox));
    img1 = do_sphere(img1, transform.sphere);
    img1 = do_filter(img1, transform.filter_level);
    return img1;
}
