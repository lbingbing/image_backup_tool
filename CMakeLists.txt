cmake_minimum_required(VERSION 3.21)

project(image_backup_tool)

set(CMAKE_VERBOSE_MAKEFILE OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(boost_dir "")
set(opencv_dir "")
set(qt_dir "")

set(CMAKE_PREFIX_PATH
    ${qt_dir}
    ${CMAKE_PREFIX_PATH}
)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS program_options PATHS ${boost_dir} NO_DEFAULT_PATH)
set(OpenCV_STATIC ON)
find_package(OpenCV REQUIRED PATHS ${opencv_dir} NO_DEFAULT_PATH)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets PATHS ${qt_dir} NO_DEFAULT_PATH)

set(external_include_dirs
    ${Boost_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

set(external_libs
    ${Boost_LIBRARIES}
    ${OpenCV_LIBS}
)

if(WIN32)
    set(external_binaries
        "${qt_dir}/bin/Qt6Core.dll"
        "${qt_dir}/bin/Qt6Gui.dll"
        "${qt_dir}/bin/Qt6Widgets.dll"
    )
endif()

set(internal_include_dirs ${CMAKE_SOURCE_DIR}/src/image_codec)

if(WIN32)
    set(compile_flags -D_WIN32_WINNT=0x0601)
elseif(APPLE)
    set(compile_flags "")
endif()

set(release_dir .)

function(add_exe dir name)
    add_executable(${name} ${dir}/${name}.cpp)
    target_compile_definitions(${name} PRIVATE ${compile_flags})
    target_include_directories(${name} PRIVATE ${internal_include_dirs} ${external_include_dirs})
    target_link_libraries(${name} PRIVATE image_codec ${external_libs})

    install(TARGETS ${name} DESTINATION ${release_dir})
endfunction()

add_subdirectory(python)
add_subdirectory(src/color_calibration)
add_subdirectory(src/decode_image)
add_subdirectory(src/decode_image_stream)
add_subdirectory(src/decode_image_stream_qt)
add_subdirectory(src/display_qt)
add_subdirectory(src/image_codec)
add_subdirectory(src/parse_image_decode_task)
add_subdirectory(src/part_image_file_gen)
add_subdirectory(src/part_image_file_stream_server)
add_subdirectory(src/part_image_stream_server)
add_subdirectory(src/test_calibration)
add_subdirectory(src/test_image_decode_task_status_client)
add_subdirectory(src/test_image_decode_task_status_server)
add_subdirectory(src/test_image_stream)
add_subdirectory(src/test_symbol_codec)
add_subdirectory(src/test_thread_safe_queue)
add_subdirectory(src/test_transform_utils)

if(WIN32)
    install(FILES ${external_binaries} DESTINATION ${release_dir})
    install(DIRECTORY ${qt_dir}/plugins/platforms DESTINATION ${release_dir})
endif()
