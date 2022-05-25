add_library(image_codec SHARED
    base64.cpp
    image_codec_types.cpp
    pixel_codec.cpp
    transform_utils.cpp
    decode_image.cpp
    decode_image_task.cpp
    decode_image_task_status_server.cpp
    image_fetcher.cpp
    decode_image_thread.cpp
)
target_compile_definitions(image_codec PRIVATE IMAGE_CODEC_EXPORTS ${compile_flags})
target_include_directories(image_codec PRIVATE ${external_include_dirs})
target_link_libraries(image_codec ${external_libs})

install(TARGETS image_codec DESTINATION bin)
install(FILES image_fetcher.ini DESTINATION bin)
