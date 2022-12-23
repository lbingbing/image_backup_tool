add_library(image_codec SHARED
    base64.cpp
    image_codec_types.cpp
    pixel_codec.cpp
    pixel_codec_capi.cpp
    transform_utils.cpp
    image_decoder.cpp
    image_decode_task.cpp
    image_decode_task_status_server.cpp
    image_stream.cpp
    image_decode_worker.cpp
)
target_compile_definitions(image_codec PRIVATE IMAGE_CODEC_EXPORTS ${compile_flags})
target_include_directories(image_codec PRIVATE ${external_include_dirs})
target_link_libraries(image_codec ${external_libs})

install(TARGETS image_codec DESTINATION bin)
install(FILES image_stream.ini DESTINATION bin)
