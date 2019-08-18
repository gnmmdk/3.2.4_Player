cmake_minimum_required(VERSION 3.4.1)
include_directories(include)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}")

file(GLOB src_files *.cpp)
add_library(
        kjjPlayer
        SHARED
        ${src_files})
target_link_libraries(
        kjjPlayer
        -Wl,--start-group
        avcodec avfilter avformat avutil swresample swscale
        -Wl,--end-group
        log
        z
        android
        OpenSLES
)