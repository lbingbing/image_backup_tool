cmake_minimum_required(VERSION 3.21)

project(pixel_image_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(boost_dir "D:/Libs/boost_1_78_0")
set(opencv_dir "D:/Libs/opencv-4.5.5-x64-Release")
set(qt_dir "D:/Qt/6.2.3/msvc2019_64")

set(external_include_dirs
    ${boost_dir}
    ${opencv_dir}/include
)

set(external_libs
    ${boost_dir}/stage/lib/libboost_program_options-vc143-mt-x64-1_78.lib
    ${opencv_dir}/x64/vc17/lib/opencv_core455.lib
    ${opencv_dir}/x64/vc17/lib/opencv_highgui455.lib
    ${opencv_dir}/x64/vc17/lib/opencv_imgcodecs455.lib
    ${opencv_dir}/x64/vc17/lib/opencv_imgproc455.lib
    ${opencv_dir}/x64/vc17/lib/opencv_videoio455.lib
)

set(external_dlls
    ${opencv_dir}/x64/vc17/bin/opencv_core455.dll
    ${opencv_dir}/x64/vc17/bin/opencv_highgui455.dll
    ${opencv_dir}/x64/vc17/bin/opencv_imgcodecs455.dll
    ${opencv_dir}/x64/vc17/bin/opencv_imgproc455.dll
    ${opencv_dir}/x64/vc17/bin/opencv_videoio455.dll
    ${opencv_dir}/x64/vc17/bin/opencv_videoio_ffmpeg455_64.dll
    ${qt_dir}/bin/Qt6Core.dll
    ${qt_dir}/bin/Qt6Gui.dll
    ${qt_dir}/bin/Qt6Widgets.dll
)

set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${qt_dir})

set(internal_include_dirs ${CMAKE_SOURCE_DIR}/src/image_codec)

set(compile_flags -D_WIN32_WINNT=0x0601)

function(add_exe dir name)
    add_executable(${name} ${dir}/${name}.cpp)
    target_compile_definitions(${name} PRIVATE ${compile_flags})
    target_include_directories(${name} PRIVATE ${internal_include_dirs} ${external_include_dirs})
    target_link_libraries(${name} PRIVATE image_codec ${external_libs})

    install(TARGETS ${name} DESTINATION bin)
endfunction()

add_subdirectory(src/color_calibration)
add_subdirectory(src/decode_image_camera)
add_subdirectory(src/decode_image_camera_qt)
add_subdirectory(src/image_codec)
add_subdirectory(src/parse_decode_image_task)
add_subdirectory(src/test_calibration)
add_subdirectory(src/test_decode_image)
add_subdirectory(src/test_image_fetcher)
add_subdirectory(src/test_pixel_codec)
add_subdirectory(src/test_thread_safe_queue)
add_subdirectory(src/test_transform_utils)

install(FILES ${external_dlls} DESTINATION bin)
install(DIRECTORY ${qt_dir}/plugins/platforms DESTINATION bin)
