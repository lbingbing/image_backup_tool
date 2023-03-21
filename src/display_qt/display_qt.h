#pragma once

#if _WIN32
    #ifdef DISPLAY_QT_EXPORTS
    #define DISPLAY_QT_API __declspec(dllexport)
    #else
    #define DISPLAY_QT_API __declspec(dllimport)
    #endif
#else
    #define DISPLAY_QT_API
#endif

extern "C" {

DISPLAY_QT_API int run(int argc, char *argv[]);

}
