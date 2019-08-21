//
// Created by kangjj on 2019/8/13.
//

#ifndef INC_3_2_4_PLAYER_VIDEOCHANNEL_H
#define INC_3_2_4_PLAYER_VIDEOCHANNEL_H

extern "C"{
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};

#include "BaseChannel.h"
#include "AudioChannel.h"

typedef void (*RenderCallback)(uint8_t *,int,int,int);

class VideoChannel : public BaseChannel{
public:
    VideoChannel(int id,AVCodecContext *codecContext,AVRational time_base,int fps);

    ~VideoChannel();

    void start();

    void stop();

    void video_decode();

    void video_play();

    void setRenderCallback(RenderCallback renderCallback);

    void setAudioChannel(AudioChannel* audioChannel);
private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback  renderCallback;
    int fps;
    AudioChannel *audioChannel = 0;
};


#endif //INC_3_2_4_PLAYER_VIDEOCHANNEL_H
