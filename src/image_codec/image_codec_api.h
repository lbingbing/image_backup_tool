#pragma once

#if _WIN32
    #ifdef IMAGE_CODEC_EXPORTS
    #define IMAGE_CODEC_API __declspec(dllexport)
    #else
    #define IMAGE_CODEC_API __declspec(dllimport)
    #endif
#elif __APPLE__
    #define IMAGE_CODEC_API
#endif
