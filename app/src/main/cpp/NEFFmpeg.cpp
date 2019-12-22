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
    pthread_mutex_init(&seekMutex,0);
}

NEFFmpeg::~NEFFmpeg() {
    DELETE(dataSource);
    DELETE(javaCallHelper);
    pthread_mutex_destroy(&seekMutex);
}
// todo A.3 为了获取到地址dataSourceffmpeg->_prepare();
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
    ffmpeg->isPlaying = 0 ;//todo F.2.2 这里结束 _start()方法里面会调用到audioChannel->stop videoChannel->stop
    //解决了：要保证_prepare方法执行完再释放的问题。
    //todo F.2.3 放到join去停止
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

// todo B 内容：FFmpeg播放器视频播放（无声）  1、JNI反射Java方法 2、FFmpeg的准备流程
void NEFFmpeg::_prepare() {
    //0.5 AVFormatContext **ps
    formatContext = avformat_alloc_context();
    AVDictionary *dictionary = 0;
    av_dict_set(&dictionary, "timeout", "10000000", 0);//设置超时时间为10秒，这里的单位是微秒
    //todo B.1.打开媒体 avformat_open_input
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
    //todo B.2 查找媒体中的流信息 avformat_find_stream_info
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
        //todo B.3_1 获取流媒体（音频或者视频）
        AVStream *stream = formatContext->streams[i];
        //todo B.3_2获取编解码这段流的参数
        AVCodecParameters *codecParameters = stream->codecpar;
        //todo B.3_3 通过参数中的id（编解码的方式），来查找当前流的解码器
        AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
        if (!codec) {
            //LOGE("查找当前流的解码器失败");
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }
            return;
        }
        //todo B.4 创建解码器上下文 avcodec_alloc_context3
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if(!codecContext){
            //LOGE("创建解码器上下文失败");
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
        }
        // todo B.5 设置解码器上下文参数 avcodec_parameters_to_context
        ret = avcodec_parameters_to_context(codecContext, codecParameters);
        if (ret < 0) {
            //LOGE("设置解码器上下文的参数失败:%s", av_err2str(ret));
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }
        //todo B.6 打开解码器 avcodec_open2
        ret = avcodec_open2(codecContext, codec, 0);
        if (ret) {
            //LOGE("打开解码器失败:%s", av_err2str(ret));
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }
        //todo E 音视频同步
        //todo E.1 time_base传递过去用于音视频同步
        AVRational time_base = stream->time_base;
        //todo B.7 判断流类型（音频还是视频？）
        if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            //音频
            audioChannel = new AudioChannel(i,codecContext,time_base,javaCallHelper);
        } else if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            //视频
            AVRational frame_rate = stream-> avg_frame_rate;//todo D 控制视频的播放速度 1
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
    //todo B.8 准备好了，反射通知java层，反射的知识点！
    if(javaCallHelper){
        javaCallHelper->onPrepared(THREAD_CHILD);
    }

}

/*
 * todo A.2  播放准备 放在子线程 pthread_create第四个函数传this进来为了获取地址
 */
void NEFFmpeg::prepare() {
    //pthread_create :创建子线程
    pthread_create(&pid_prepare, 0, task_prepare, this);
}
// todo C 视频解码与原生绘制播放
void NEFFmpeg::start() {
    isPlaying = 1;
    if(videoChannel){
        //todo E.3 设置AudioChannel用于音视频同步
        videoChannel->setAudioChannel(audioChannel);
        //todo 下方的_start()方法里面 生产数据videoChannel->packets.push(packet); 这里就要消费数据
        videoChannel->start();
    }
    if (audioChannel) {
        audioChannel->start();
    }
    pthread_create(&pid_start,0,task_start,this);
}
/**
 * todo B.10 获取到AVPacket，然后放到队列中  视频解码有两个生产者、消费者，这里是第一个。生产者（videoChannel->packets.push(packet);）-消费者（video_decode）
 */
void NEFFmpeg::_start() {
    while (isPlaying) {
        /**
         *  // todo D 内存泄漏点1
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
        // todo B.10_1 给AVPacket分配内存空间
        AVPacket *packet = av_packet_alloc();
        // todo B.10_2 从媒体中读取音/视频报
        int ret = av_read_frame(formatContext, packet);
        if (!ret) {
            //ret为0 表示成功
            //todo B.10_3 要判断流类型，是视频还是音频,然后分别添加到队列中
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
            if(videoChannel->packets.empty() && videoChannel->frames.empty()
                    && audioChannel->packets.empty() && audioChannel->frames.empty()){
                av_packet_free(&packet);//播放完了
                break;
            }
        } else {
            //LOGE("读取音视频数据包失败");
            LOGE("读取音视频数据包失败");
            if (javaCallHelper) {
                javaCallHelper->onError(THREAD_CHILD, ERROR_READ_PACKETS_FAIL);
            }
            av_packet_free(&packet);
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
    //todo F.2.1 创建子线程去停止
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

void NEFFmpeg::seekTo(int playProgress) {
    if(playProgress<0 || playProgress > duration){
        return;
    }
    if(!audioChannel && !videoChannel){
        return;
    }
    if(!formatContext){
        return;
    }
    pthread_mutex_lock(&seekMutex);
    //1 上下文 2 流索引，（-1：表示选择的是默认流） 3要seek到的时间戳，4 seek的方式
    // AVSEEK_FLAG_BACKWAR 表示seek到请求的时间戳之前的最靠近的一个关键帧
    // AVSEEK_FLAG_BYTE 基于字节位置seek
    // AVSEEK_FLAG_ANY  任意帧（可能不是关键帧,会花屏）
    //AVSEEK_FLAG_FRAME 基于帧数seek
    int ret = av_seek_frame(formatContext,-1,playProgress * AV_TIME_BASE,AVSEEK_FLAG_BACKWARD);
    if(ret<0){
        if(javaCallHelper){
            javaCallHelper->onError(THREAD_CHILD,ERROR_SEEK_TO);
        }
        return;
    }
    if(audioChannel){
        audioChannel->packets.setWork(0);
        audioChannel->frames.setWork(0);
        audioChannel->packets.clear();
        audioChannel->frames.clear();
        audioChannel->packets.setWork(1);
        audioChannel->frames.setWork(1);
    }

    if(videoChannel){
        videoChannel->packets.setWork(0);
        videoChannel->frames.setWork(0);
        videoChannel->packets.clear();
        videoChannel->frames.clear();
        videoChannel->packets.setWork(1);
        videoChannel->frames.setWork(1);
    }
    pthread_mutex_unlock(&seekMutex);
}
