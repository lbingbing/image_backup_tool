#pragma once

#include <vector>

#include "image_codec_api.h"
#include "pixel_codec.h"

Bytes b64encode(const Bytes& bytes);
Bytes b64decode(const Bytes& bytes);