set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_library(display_qt_l SHARED
    Widget.cpp
    display_qt.cpp
)
target_compile_definitions(display_qt_l PRIVATE DISPLAY_QT_EXPORTS ${compile_flags})
target_include_directories(display_qt_l PRIVATE ${internal_include_dirs} ${external_include_dirs})
target_link_libraries(display_qt_l PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui image_codec ${external_libs})
set_property(TARGET display_qt_l PROPERTY INSTALL_RPATH .)

add_executable(display_qt
    main.cpp
)
target_compile_definitions(display_qt PRIVATE ${compile_flags})
target_link_libraries(display_qt PRIVATE Qt6::Core Qt6::Widgets Qt6::Gui display_qt_l)
set_target_properties(display_qt PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
set_property(TARGET display_qt PROPERTY INSTALL_RPATH .)

install(TARGETS display_qt_l DESTINATION ${release_dir})
install(TARGETS display_qt DESTINATION ${release_dir})
