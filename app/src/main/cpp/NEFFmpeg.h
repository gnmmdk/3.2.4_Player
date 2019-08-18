//
// Created by kangjj on 2019/8/13.
//

#ifndef INC_3_2_4_PLAYER_NEFFMPEG_H
#define INC_3_2_4_PLAYER_NEFFMPEG_H


#include "JavaCallHelper.h"
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "macro.h"
#include <cstring>
#include <pthread.h>

extern "C"{
#include <libavformat/avformat.h>
};

class NEFFmpeg {
public:
    NEFFmpeg(JavaCallHelper *javaCallHelper, char *dataSource);

    ~NEFFmpeg();

    void prepare();
    void _prepare();

    void start();

    void _start();

    void setRenderCallback(RenderCallback renderCallback);

private:
    JavaCallHelper *javaCallHelper = 0;
    AudioChannel *audioChannel = 0;
    VideoChannel *videoChannel = 0;
    char *dataSource;
    pthread_t pid_prepare;
    pthread_t pid_start;
    bool isPlaying;
    AVFormatContext *formatContext = 0;

    RenderCallback renderCallback;

};


#endif //INC_3_2_4_PLAYER_NEFFMPEG_H
