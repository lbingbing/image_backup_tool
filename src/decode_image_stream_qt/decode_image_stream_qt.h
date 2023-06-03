#pragma once

#if _WIN32
    #ifdef DECODE_IMAGE_STREAM_QT_EXPORTS
    #define DECODE_IMAGE_STREAM_QT_API __declspec(dllexport)
    #else
    #define DECODE_IMAGE_STREAM_QT_API __declspec(dllimport)
    #endif
#else
    #define DECODE_IMAGE_STREAM_QT_API
#endif

extern "C" {

DECODE_IMAGE_STREAM_QT_API int run(int argc, char *argv[]);

}
