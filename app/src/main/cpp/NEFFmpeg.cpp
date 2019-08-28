//
// Created by kangjj on 2019/8/13.
//
#include "NEFFmpeg.h"

NEFFmpeg::NEFFmpeg(JavaCallHelper *javaCallHelper, char *dataSource) {
    this->javaCallHelper = javaCallHelper;
    //这里dataSource是从Java传过来的字符串，通过jni接口转变成了c++字符串。
//在jni方法中被释放掉了，导致dataSource变成悬空指针的问题（指向一块已经释放了的内存）
//    this->dataSource = dataSource;?

//内存拷贝，自己管理它的内存，strlen获取字符串长度，strcpy:拷贝字符串
//java:"hello" c字符串以\0 结尾:"hello\0"
    this->dataSource = new char[strlen(dataSource) + 1];
    strcpy(this->dataSource, dataSource);
}

NEFFmpeg::~NEFFmpeg() {
    DELETE(dataSource);
    DELETE(javaCallHelper);
}

void *task_prepare(void *args) {
    //打开输入 这里的args对应到pthread_create(&pid_prepare,0,task_prepare,this);的第四个参数（this）
    NEFFmpeg *ffmpeg = static_cast<NEFFmpeg *>(args);
    ffmpeg->_prepare();
    return 0;//一定一定一定要返回0！！！
}

void* task_start(void* args){
    NEFFmpeg * ffmpeg = static_cast<NEFFmpeg *>(args);
    ffmpeg->_start();
    return 0;
}

//要访问ffmpeg的私有属性，还可以设置为友元函数
void * task_stop(void* args){
    NEFFmpeg* ffmpeg = static_cast<NEFFmpeg *>(args);
    if(!ffmpeg->isPlaying){
        return 0 ;
    }
    ffmpeg->isPlaying = 0 ;
    //解决了：要保证_prepare方法执行完再释放的问题。
    // 假如是直播，这里可能阻塞住 int ret = avformat_open_input(&formatContext, dataSource, 0, &dictionary);
    pthread_join(ffmpeg->pid_prepare,0);
    //需要等待该线程循环结束：1、formatContext在线程有使用到 2、videoChannel->stop 和audioChannel->stop 需要执行完
    pthread_join(ffmpeg->pid_start,0);
    //要保证_prepare方法执行完再释放,所以上方用了pthread_join
    if(ffmpeg->formatContext){
        avformat_close_input(&ffmpeg->formatContext);
        avformat_free_context(ffmpeg->formatContext);
        ffmpeg->formatContext = 0 ;
    }
//    if(ffmpeg->videoChannel){ //这里不需要。isplaying=0的时候会start循环结束，会被调用。
//        ffmpeg->videoChannel->stop();
//    }
//    if(ffmpeg->audioChannel){
//        ffmpeg->audioChannel->stop();
//    }
    DELETE(ffmpeg->videoChannel);
    DELETE(ffmpeg->audioChannel);
    DELETE(ffmpeg);
    return 0;
}

void NEFFmpeg::_prepare() {
    //0.5 AVFormatContext **ps
    formatContext = avformat_alloc_context();
    AVDictionary *dictionary = 0;
    av_dict_set(&dictionary, "timeout", "10000000", 0);//设置超时时间为10秒，这里的单位是微秒
    //1.打开媒体
    int ret = avformat_open_input(&formatContext, dataSource, 0, &dictionary);
    av_dict_free(&dictionary);
    if (ret) {
        //LOGE("打印媒体失败：%s", av_err2str(ret));
//        javaCallHelper->sendErrorJavaCallHelper(ERROR_RET0,"dakai");
        
        if(javaCallHelper){
            javaCallHelper->onError(THREAD_CHILD, ERROR_CAN_NOT_OPEN_URL);
        }
        return;
    }
    //2 查找媒体中的流信息
    ret = avformat_find_stream_info(formatContext, 0);
    if (ret < 0) {
        //LOGE("查找媒体中的流信息失败:%s", av_err2str(ret));
        if (javaCallHelper) {
            javaCallHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }
    duration = formatContext->duration / AV_TIME_BASE;
    //这里的i是后面的packet->stream_index
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        //3_1 获取流媒体（音频或者视频）
        AVStream *stream = formatContext->streams[i];
        //3_2获取编解码这段流的参数
        AVCodecParameters *codecParameters = stream->codecpar;
        //3_3 通过参数中的id（编解码的方式），来查找当前流的解码器
        AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
        if (!codec) {
            //LOGE("查找当前流的解码器失败");
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }
            return;
        }
        //4 创建解码器上下文
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if(!codecContext){
            //LOGE("创建解码器上下文失败");
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
        }
        // 5 设置解码器上下文参数
        ret = avcodec_parameters_to_context(codecContext, codecParameters);
        if (ret < 0) {
            //LOGE("设置解码器上下文的参数失败:%s", av_err2str(ret));
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }
        //6 打开解码器
        ret = avcodec_open2(codecContext, codec, 0);
        if (ret) {
            //LOGE("打开解码器失败:%s", av_err2str(ret));
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }
        AVRational time_base = stream->time_base;
        //判断流类型（音频还是视频？）
        if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            //音频
            audioChannel = new AudioChannel(i,codecContext,time_base,javaCallHelper);
        } else if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            //视频
            AVRational frame_rate = stream-> avg_frame_rate;//TODO  看视频
            int fps = av_q2d(frame_rate);
            videoChannel = new VideoChannel(i,codecContext,time_base,javaCallHelper,fps);
            videoChannel->setRenderCallback(renderCallback);
        }
    }//end for
    if (!audioChannel && !videoChannel) {
        //LOGE("没有音视频");
        if (javaCallHelper) {
            javaCallHelper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        }
        return;
    }
    //准备好了，反射通知java
    if(javaCallHelper){
        javaCallHelper->onPrepared(THREAD_CHILD);
    }

}

/*
 * 播放准备
 */
void NEFFmpeg::prepare() {
    //pthread_create :创建子线程
    pthread_create(&pid_prepare, 0, task_prepare, this);
}

void NEFFmpeg::start() {
    isPlaying = 1;
    if(videoChannel){
        videoChannel->setAudioChannel(audioChannel);
        videoChannel->start();
    }
    if (audioChannel) {
        audioChannel->start();
    }
    pthread_create(&pid_start,0,task_start,this);
}
/**
 * 真正执行解码播放
 */
void NEFFmpeg::_start() {
    while (isPlaying) {
        /**
         * 内存泄漏点1
         * 控制packets队列
         */
        if (videoChannel && videoChannel->packets.size() > 100) {
            av_usleep(10 * 1000);     //睡眠十毫秒
            continue;
        }
        if (audioChannel && audioChannel->packets.size() > 100) {
            av_usleep(10 * 1000);
            continue;
        }
        //7_1 给AVPacket分配内存空间
        AVPacket *packet = av_packet_alloc();
        //7_2 从媒体中读取音/视频报
        int ret = av_read_frame(formatContext, packet);
        if (!ret) {
            //ret为0 表示成功
            //要判断流类型，是视频还是音频
            if (videoChannel && packet->stream_index == videoChannel->id) {
                //往视频编码数据包队列中添加数据
                videoChannel->packets.push(packet);
            } else if (audioChannel && packet->stream_index == audioChannel->id) {
                //往音频编码数据包队列中添加数据
                audioChannel->packets.push(packet);
            }
        } else if (ret == AVERROR_EOF) {
            //表示读完了
            //要考虑读完了，是否播放完的情况
        } else {
            //LOGE("读取音视频数据包失败");
            LOGE("读取音视频数据包失败");
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, ERROR_READ_PACKETS_FAIL);
            }
            break;
        }
    }//end while
    isPlaying = 0;
    if (videoChannel) {
        videoChannel->stop();
    }
    if(audioChannel) {
        audioChannel->stop();
    }
}

void NEFFmpeg::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void NEFFmpeg::stop() {
    javaCallHelper = 0 ;//防止prepare阻塞中停止了，还是会回调给java
    //要保证_prepare方法（子线程中）执行完再释放（在主线程）
    //pthread_join ：这里调用了后会阻塞主线程，可能引发ANR 所以开个线程去释放
//    isPlaying = 0;
//    pthread_join(pid_prepare, 0);//解决了：要保证_prepare方法（子线程中）执行完再释放（在主线程）的问题
    //在主线程会引发ANR，那么到子线程中去释放
    pthread_create(&pid_stop,0,task_stop,this);
}

int NEFFmpeg::getDuration() const{
    return duration;
}
