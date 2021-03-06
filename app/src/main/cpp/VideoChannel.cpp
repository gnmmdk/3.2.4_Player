//
// Created by kangjj on 2019/8/13.
//


#include "VideoChannel.h"

/**
 * 丢包AVPacket
 * @param q
 */
void dropAVPakcet(queue<AVPacket *> &q){
    while(!q.empty()){
        AVPacket *avPacket = q.front();
        //I帧、B帧、P帧
        //不能丢I帧，AV_PKT_FLAG_KEY:I帧（关键帧）
        if(avPacket->flags != AV_PKT_FLAG_KEY){
            BaseChannel::releaseAVPacket(&avPacket);
            q.pop();
        }else{
            break;
        }
    }
}

/**
 * 丢包AVFrame
 * @param q
 */
void dropAVFrame(queue<AVFrame *> &q){
    if(!q.empty()){
        AVFrame * avFrame = q.front();
        BaseChannel::releaseAVFrame(&avFrame);
        q.pop();
    }
}
VideoChannel::VideoChannel(int id,AVCodecContext* codecContext,AVRational time_base,int fps):BaseChannel(id,codecContext,time_base) {
    this->fps = fps;
    packets.setSyncHandle(dropAVPakcet);
    frames.setSyncHandle(dropAVFrame);
}

VideoChannel::~VideoChannel() {}

void* task_video_decode(void* args){
    VideoChannel * videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->video_decode();
    return 0;
}

void * task_video_play(void* args){
    VideoChannel* videoChannel = static_cast<VideoChannel *>(args);
    videoChannel->video_play();
    return 0;
}

void VideoChannel::start() {
    //设置队列状态
    isPlaying = 1;
    //设置队列为工作状态
    packets.setWork(1);
    frames.setWork(1);
    pthread_create(&pid_video_decode,0,task_video_decode,this);
    pthread_create(&pid_video_play,0,task_video_play,this);
}

void VideoChannel::stop() {

}

/**
 * 真正的视频解码
 */
void VideoChannel::video_decode() {
    AVPacket* packet=0;
    while(isPlaying){
        //8 获取AVPacket
        int ret = packets.pop(packet);
        if(!isPlaying){
            //如果停止播放了，跳出循环 释放packet
            break;
        }
        if(!ret){
            //取数据包失败
            continue;
        }
        // 9 拿到视频数据包（编码压缩了的），需要把数据包给解码器进行解码
        ret = avcodec_send_packet(codecContext,packet);
        if(ret){
            //往解码器发送数据包失败，跳出循环
            break;
        }
        releaseAVPacket(&packet);
        AVFrame* frame =av_frame_alloc();
        //10 从解码器中获取音/视频原始数据包
        ret = avcodec_receive_frame(codecContext, frame);
        if(ret == AVERROR(EAGAIN)){
            continue;
        }else if(ret){//ret != 0
            break;
        }
        /**
         * 内存泄露点2
         * 控制 frames 队列
         */
        while(isPlaying && frames.size()>100){
            av_usleep(10*1000);
            continue;
        }
        //ret==0 数据收发正常，成功获取到解码后的视频原始数据包AVFrame，格式是yuv
        frames.push(frame);
    }
    releaseAVPacket(&packet);

}
/**
 * 真正的播放视频
 */
void VideoChannel::video_play() {
    AVFrame * frame = 0;
    //要对原始数据进行转换 yuv>grba
    //()优先级高，首先说明p是一个指针，指向一个整型的一维数组，这个一维数组的长度是n，
    // 也可以说是p的步长。也就是说执行p+1时，p要跨过n个整型数据的长度。
    // 数组指针也称指向一维数组的指针，亦称行指针。
    //uint8_t (*dst_dat)[4];
    //指针数组 是多个指针变量，以数组形式存在内存当中，占有多个指针的存储空间
    uint8_t *dst_data[4];
    int dst_linesize[4];
    SwsContext* sws_context =sws_getContext(codecContext->width,codecContext->height,
            codecContext->pix_fmt,
            codecContext->width,codecContext->height,AV_PIX_FMT_RGBA,
            SWS_BILINEAR,NULL,NULL,NULL) ;
    //  给dst_data 和 dst_linesize 申请内存
    av_image_alloc(dst_data,dst_linesize,
            codecContext->width,codecContext->height,AV_PIX_FMT_RGBA,1);
    //根据fps（传入流的的平均帧率来控制每一帧的延时时间）
    //sleep : fps > time(单位是秒)
    double delay_time_per_frame = 1.0/fps;
    while(isPlaying){
        int ret = frames.pop(frame);
        if(!isPlaying){
            break;
        }
        if(!ret){
            //取数据包失败
            continue;
        }
        //取到yuv原始数据包，下面要进行数据转换        dst_data:AV_PIX_FMT_RGBA格式里的数据
        sws_scale(sws_context,frame->data,
                frame->linesize,0,codecContext->height,dst_data,dst_linesize);
        //进行休眠 每一帧还有自己的额外延时时间 （怎么获取额外延时时间公式? 点进去frame->repeat_pict看注释）
        double extra_delay = frame->repeat_pict / (2* fps);
        double real_delay = extra_delay+ delay_time_per_frame;

        //根据音频的播放时间来判断
        //获取视频的播放时间
        double video_time = frame->best_effort_timestamp * av_q2d(time_base);

        //音视频同步：永远都是你追我赶的状态。
        if(!audioChannel){
            //没有音频（类似GIF）
            av_usleep(real_delay * 1000000);
        } else{
            double audioTime = audioChannel->audio_time;
            //音视频时间差
            double time_diff = video_time-audioTime;
            if(time_diff >0 ){
                LOGE("视频比音频快:%lf",time_diff);
                //视频比音频快：等音频（sleep）
                //自然状态下，time_diff的值不会很大
                //但是，seek后time_diff的值可能会很大，导致视频休眠天就
                //av_usleep((real_delay + time_diff) * 1000000);//TODO seek后测试
                if(time_diff >1){
                    av_usleep((real_delay * 2 ) * 1000000);
                }else{
                    av_usleep((real_delay + time_diff) * 1000000);
                }
            }else if(time_diff<0){
                LOGE("音频比视频快: %lf", fabs(time_diff));//fabs绝对值
                //音频比视频快：追音频（尝试丢视频包）
                //视频包：packets和frames
                if(fabs(time_diff)>=0.05){
                    //时间差如果大于0.05，有明显的延迟感
                    //丢包：要操作队列中数据！一定要小心！
//                    packets.sync();
                    frames.sync();
                    continue;
                }
//                av_usleep(real_delay  * 1000000);
            }else{
                LOGE("音视频完美同步！");
//                av_usleep(real_delay  * 1000000);
            }
        }


        //渲染，回调回去 > native-lib
        //渲染一副图像需要什么信息：（宽高》图像的尺寸）（图像的内容（数据）怎么画）
        //需要：1 data 2 linesize 3 width 4 height
        renderCallback(dst_data[0],dst_linesize[0],codecContext->width,codecContext->height);
        releaseAVFrame(&frame);
    }//end while
    releaseAVFrame(&frame);
    isPlaying = 0 ;
    av_freep(&dst_data[0]);
    sws_freeContext(sws_context);
}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void VideoChannel::setAudioChannel(AudioChannel *audioChannel) {
    this->audioChannel = audioChannel;
}


