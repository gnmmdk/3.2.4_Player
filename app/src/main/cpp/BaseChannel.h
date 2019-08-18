//
// Created by kangjj on 2019/8/13.
//

#ifndef INC_3_2_4_PLAYER_BASECHANNEL_H
#define INC_3_2_4_PLAYER_BASECHANNEL_H


#include "safe_queue.h"

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
};

class BaseChannel{
public:
    BaseChannel(int id,AVCodecContext *codecContext):id(id),codecContext(codecContext){
        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }
    //虚函数是指一个类中你希望重载的成员函数，当你用一个基类指针或引用指向一个继承对象的时候，
//    调用一个虚函数时，实际调用的是继承类的版本。
    virtual ~BaseChannel(){
        packets.clear();
        frames.clear();
    }

    static void releaseAVPacket(AVPacket **packet){//这里的T表示AVPacket* 所以需要传入AVPacket**
        if(packet){
            av_packet_free(packet);
            *packet = 0;
        }
    }

    static void releaseAVFrame(AVFrame ** frame){
        if(frame){
            av_frame_free(frame);
            *frame = 0;
        }
    }
    //“=0”并不表示函数返回值为0，它只起形式上的作用，告诉编译系统“这是纯虚函数”
    //纯虚函数只有函数的名字而不具备函数的功能，不能被调用。它只是通知编译系统:
    // “在这里声明一个虚函数，留待派生类中定义”。在派生类中对此函数提供定义后，它才能具备函数的功能，可被调用。
    virtual void start() = 0;
    virtual void stop() = 0;

    int id;
    SafeQueue<AVPacket *> packets;
    SafeQueue<AVFrame *> frames;
    int isPlaying;
    AVCodecContext *codecContext;
};

#endif //INC_3_2_4_PLAYER_BASECHANNEL_H
