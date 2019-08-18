//
// Created by kangjj on 2019/8/13.
//

#ifndef INC_3_2_4_PLAYER_AUDIOCHANNEL_H
#define INC_3_2_4_PLAYER_AUDIOCHANNEL_H


#include "BaseChannel.h"

class AudioChannel : public BaseChannel {
public:
    AudioChannel(int id,AVCodecContext *codecContext);

    ~AudioChannel();

    void start();

    void stop();
};


#endif //INC_3_2_4_PLAYER_AUDIOCHANNEL_H
