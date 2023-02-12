set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_executable(decode_image_stream_qt
    main.cpp
    Widget.cpp
)
target_compile_definitions(decode_image_stream_qt PRIVATE ${compile_flags})
target_include_directories(decode_image_stream_qt PRIVATE ${internal_include_dirs} ${external_include_dirs})
target_link_libraries(decode_image_stream_qt PRIVATE Qt6::Core Qt6::Widgets image_codec ${external_libs})
set_target_properties(decode_image_stream_qt PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

install(TARGETS decode_image_stream_qt DESTINATION ${release_dir})
