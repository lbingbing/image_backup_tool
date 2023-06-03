set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_library(decode_image_stream_qt_l SHARED
    Widget.cpp
    decode_image_stream_qt.cpp
)
target_compile_definitions(decode_image_stream_qt_l PRIVATE DECODE_IMAGE_STREAM_QT_EXPORTS ${compile_flags})
target_include_directories(decode_image_stream_qt_l PRIVATE ${internal_include_dirs} ${external_include_dirs})
target_link_libraries(decode_image_stream_qt_l PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui image_codec ${external_libs})
set_property(TARGET decode_image_stream_qt_l PROPERTY INSTALL_RPATH .)

add_executable(decode_image_stream_qt
    main.cpp
)
target_compile_definitions(decode_image_stream_qt PRIVATE ${compile_flags})
target_link_libraries(decode_image_stream_qt PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui decode_image_stream_qt_l)
set_target_properties(decode_image_stream_qt PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
set_property(TARGET decode_image_stream_qt PROPERTY INSTALL_RPATH .)

install(TARGETS decode_image_stream_qt_l DESTINATION ${release_dir})
install(TARGETS decode_image_stream_qt DESTINATION ${release_dir})
