#pragma once

#ifdef IMAGE_CODEC_EXPORTS
#define IMAGE_CODEC_API __declspec(dllexport)
#else
#define IMAGE_CODEC_API __declspec(dllimport)
#endif