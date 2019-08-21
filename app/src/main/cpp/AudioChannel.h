//
// Created by kangjj on 2019/8/13.
//

#ifndef INC_3_2_4_PLAYER_AUDIOCHANNEL_H
#define INC_3_2_4_PLAYER_AUDIOCHANNEL_H

#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
extern "C"{
#include <libswresample/swresample.h>
};

class AudioChannel : public BaseChannel {
public:
    AudioChannel(int id,AVCodecContext *codecContext,AVRational time_base);

    ~AudioChannel();

    void start();

    void stop();

    void audio_decode();

    void audio_play();

    int getPCM();

    uint8_t *out_buffers = 0;
    int out_channels;
    int out_sampleSize;
    int out_sampleRate;
    int out_buffers_size;

private:
    SwrContext* swr_context = 0;
    pthread_t pid_audio_decode;
    pthread_t pid_audio_play;
    //引擎
    SLObjectItf engineObject;
    //引擎接口
    SLEngineItf engineInterface;
    //混音器
    SLObjectItf outputMixObject;
    //播放器
    SLObjectItf bqPlayerObject;
    //播放器接口
    SLPlayItf bqPlayerPlay;
    //播放器队列接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
};


#endif //INC_3_2_4_PLAYER_AUDIOCHANNEL_H
