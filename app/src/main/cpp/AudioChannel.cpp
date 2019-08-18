//
// Created by kangjj on 2019/8/13.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int id,AVCodecContext *codecContext) : BaseChannel(id,codecContext) {}

AudioChannel::~AudioChannel() {

}

void AudioChannel::start() {
    //设置队列状态
}

void AudioChannel::stop() {

}
