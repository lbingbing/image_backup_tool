set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_executable(display_qt
    main.cpp
    Widget.cpp
)
target_compile_definitions(display_qt PRIVATE ${compile_flags})
target_include_directories(display_qt PRIVATE ${internal_include_dirs} ${external_include_dirs})
target_link_libraries(display_qt PRIVATE Qt6::Core Qt6::Widgets image_codec ${external_libs})
set_target_properties(display_qt PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

install(TARGETS display_qt DESTINATION ${release_dir})
