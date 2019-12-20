cmake_minimum_required(VERSION 3.4.1)
include_directories(include)

#add_library(           #todo A 静态库一个个导入 如果动态库采用这种方式会有版本兼容性问题
#        avcodec
#        STATIC             #静态库
#        IMPORTED)          #导入方式
#set_target_properties(     #设置目标库的属性
#        avcodec            #库名
#        PROPERTIES                 #属性 作用？
#        IMPORTED_LOCATION          #属性 作用？
#        ${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}/libavcodec.a  #路径
#)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}") #导入静态库

file(GLOB src_files *.cpp)
add_library(
        kjjPlayer
        SHARED
        ${src_files})
target_link_libraries(
        kjjPlayer
        # todo A 链接静态库 这种方式 要注意顺序 顺序错误的话 使用到api会报错
        #        avcodec avfilter avformat avutil swresample swscale
        #        avformat avcodec avfilter  avutil swresample swscale
        # todo A 链接静态库 这种方式 任意顺序
        -Wl,--start-group
        avcodec avfilter avformat avutil swresample swscale
        -Wl,--end-group
        log
        z       #todo A 编译ffmpeg库的时候 会有External libraries:zlib 通过cmd查看libz库里面有什么：
                #D:\android-ndk-r16b\platforms\android-26\arch-arm\usr\lib>D:\android-ndk-r16b\to
                #olchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin\arm-linux-android
                #eabi-nm.exe libz.so
        android
        OpenSLES
)