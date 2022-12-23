#include <iostream>
#include <exception>
#include <opencv2/opencv.hpp>

#include "image_codec.h"

int main() {
    bool quit = false;
    while (!quit) {
        auto image_stream = create_image_stream();
        cv::namedWindow("Video Player");
        cv::Mat frame;
        while (true) {
            frame = image_stream->GetFrame();
            if (frame.empty()) break;
            cv::imshow("Video Player", frame);
            char c = static_cast<char>(cv::waitKey(25));
            if (c == 27) {
                quit = true;
                break;
            } else if (c == 's') {
                cv::imwrite("z.bmp", frame);
            }
        }
    }
    return 0;
}
