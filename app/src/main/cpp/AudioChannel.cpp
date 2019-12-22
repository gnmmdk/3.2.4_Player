//
// Created by kangjj on 2019/8/13.
//


#include "AudioChannel.h"
//todo D 音频解码与播放
AudioChannel::AudioChannel(int id,AVCodecContext *codecContext,AVRational time_base,JavaCallHelper* javaCallHelper) : BaseChannel(id,codecContext,time_base,javaCallHelper) {
    //缓冲区大小
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);//通道数
    out_sampleSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sampleRate = 44100;
    //2(通道数）*2（16bits = 2 字节）* 44100（采样率）
    out_buffers_size = out_channels * out_sampleSize * out_sampleRate;
    //todo D.3.1 申请out_buffers数组的空间
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));
    memset(out_buffers,0,out_buffers_size);
    swr_context = swr_alloc_set_opts(0,AV_CH_LAYOUT_STEREO,AV_SAMPLE_FMT_S16,
                                                 out_sampleRate,codecContext->channel_layout,
                                                 codecContext->sample_fmt,codecContext->sample_rate,
                                                 0,0);
    //todo D.3.2 初始化重采样上下文
    swr_init(swr_context);
}

AudioChannel::~AudioChannel() {
    if(swr_context){
        swr_free(&swr_context);
        swr_context = 0;
    }
    DELETE(out_buffers);
}
void* task_audio_decode(void* args){
    AudioChannel* audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_decode();
    return 0;
};
void* task_audio_play(void* args){
    AudioChannel* audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_play();
    return 0;
};
void AudioChannel::start() {
    isPlaying = 1;
    //设置队列为工作状态
    packets.setWork(1);
    frames.setWork(1);
    //解码
    pthread_create(&pid_audio_decode,0,task_audio_decode,this);
    //播放
    pthread_create(&pid_audio_play,0,task_audio_play,this);
}

void AudioChannel::stop() {
    isPlaying = 0 ;
    //TODO javaCallHelper
    packets.setWork(0);
    frames.setWork(0);
    pthread_join(pid_audio_decode,0);
    pthread_join(pid_audio_play,0);
    if(swr_context){
        swr_free(&swr_context);
        swr_context = 0;
    }

    /**
     * 7、释放
     */
     //7.1 设置播放器状态为停止状态
    if (bqPlayerPlay) {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
        bqPlayerPlay= 0;
    }
     //7.2 销毁播放器
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bqPlayerBufferQueue = 0;
    }
     //7.3 销毁混音器
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
     //7.4 销毁引擎
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineInterface = 0;
    }
}
/**
 * todo D.1 音频解码            与视频一样
 */
void AudioChannel::audio_decode() {
    AVPacket* packet = 0 ;
    while(isPlaying){
        // todo D.1.1 获取AVPacket
        int ret = packets.pop(packet);
        if(!isPlaying){
            //如果停止播放了，跳出循环 释放packet
            break;
        }
        if(!ret){
            //取数据包失败
            continue;
        }
        // todo D.1.2 拿到音频数据包（编码压缩了的），需要把数据包给解码器进行解码
        ret = avcodec_send_packet(codecContext,packet);
        if(ret){
            //往解码器发送数据包失败，跳出循环
            break;
        }
        releaseAVPacket(&packet);
        AVFrame * frame = av_frame_alloc();
        // todo D.1.3 从解码器中获取音/视频原始数据包
        ret = avcodec_receive_frame(codecContext,frame);
        if(ret == AVERROR(EAGAIN)){
            //重来
            continue;
        } else if(ret != 0){
            break;
        }
        /**
    * todo D 内存泄漏点2
    * 控制 frames 队列
    */
        while(isPlaying && frames.size() > 100){
            av_usleep(10*1000);
            continue;
        }
        // todo D.1.4 放入到Frame队列中，是PCM数据，后续需要重采样
        frames.push(frame);//PCM数据
    }
    releaseAVPacket(&packet);
}

//4.3 创建回调函数
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    AudioChannel* audioChannel = static_cast<AudioChannel *>(context);
    int pcm_size = audioChannel->getPCM();
    //todo D.3.4 入队 就能播放了
    if(pcm_size > 0){
        (*bq)->Enqueue(bq,audioChannel->out_buffers,pcm_size);
    }
}
//todo D.2 OpenSLES播放PCM
void AudioChannel::audio_play() {
    //todo D.2.1、创建引擎并获取引擎接口
    SLresult result;
    // 1.1 创建引擎对象：SLObjectItf engineObject
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 1.2 初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 1.3 获取引擎接口 SLEngineItf engineInterface
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //todo D.2.2、设置混音器
    // 2.1 创建混音器：SLObjectItf outputMixObject
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0,
                                                 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 2.2 初始化混音器
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //不启用混响可以不用获取混音器接口
    // 获得混音器接口
    //result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
    //                                         &outputMixEnvironmentalReverb);
    //if (SL_RESULT_SUCCESS == result) {
    //设置混响 ： 默认。
    //SL_I3DL2_ENVIRONMENT_PRESET_ROOM: 室内
    //SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM : 礼堂 等
    //const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    //(*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
    //       outputMixEnvironmentalReverb, &settings);
    //}
    //todo D.2.3、创建播放器
    //3.1 配置输入声音信息
    //创建buffer缓冲类型的队列 2个队列
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                       2};
    //pcm数据格式
    //SL_DATAFORMAT_PCM：数据格式为pcm格式
    //2：双声道
    //SL_SAMPLINGRATE_44_1：采样率为44100
    //SL_PCMSAMPLEFORMAT_FIXED_16：采样格式为16bit
    //SL_PCMSAMPLEFORMAT_FIXED_16：数据大小为16bit
    //SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT：左右声道（双声道）
    //SL_BYTEORDER_LITTLEENDIAN：小端模式
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};

    //数据源 将上述配置信息放到这个数据源中
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    //3.2 配置音轨（输出）
    //设置混音器
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};
    //需要的接口 操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    //3.3 创建播放器
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &audioSrc, &audioSnk, 1, ids, req);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //3.4 初始化播放器：SLObjectItf bqPlayerObject
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //3.5 获取播放器接口：SLPlayItf bqPlayerPlay
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //todo D.2.4、设置播放回调函数
    //4.1 获取播放器队列接口：SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);

    //4.2 设置回调 void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);
    //todo D.2.5、设置播放器状态为播放状态
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    //todo D.2.6、手动激活回调函数
    bqPlayerCallback(bqPlayerBufferQueue, this);
}
//todo D.3 获取PCM数据 需要重采样
int AudioChannel::getPCM() {
    int pcm_data_size = 0;
    AVFrame* frame = 0 ;
    //todo 内存泄漏点
//    SwrContext *swrContext = swr_alloc_set_opts(0, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
//                                                out_sampleRate, codecContext->channel_layout,
//                                                codecContext->sample_fmt, codecContext->sample_rate,
//                                                0, 0);
//    //初始化重采样上下文
//    swr_init(swrContext);

    while(isPlaying){
        int ret = frames.pop(frame);
        if(!isPlaying){
            break;
        }
        if(!ret){
            //取数据包失败
            continue;
        }
//        LOGE("音频播放中");
        //swr_get_delay 下一个输入数据与下下个输入数据之间的时间间隔
        int64_t delay = swr_get_delay(swr_context,frame->sample_rate);
        // a * b / c
        //AV_ROUND_UP:向上取整
        int64_t out_max_samples = av_rescale_rnd(
                frame->nb_samples + delay,
                frame->sample_rate,
                out_sampleRate,
                AV_ROUND_UP);
        //todo D.3.3 重采样
        //pcm数据在frame中
        //这里获得的解码后pcm格式音频原始数据，有可能与创建的播放器中设置的pcm格式不一样
        //todo 假设输入10个数据(48000)，有可能这次转换只转换了8个(41000)，还剩2个数据（delay）
        //上下文
        //输出缓冲区
        //输出缓冲区能容纳的最大数据量
        //输入数据
        //输入数据量
        int out_samples = swr_convert(
                swr_context,
                &out_buffers,
                out_max_samples,
                (const uint8_t **)(frame->data),
                frame->nb_samples);
        // 获取swr_convert转换后 out_samples个 *2 （16位）*2（双声道）
        pcm_data_size = out_samples * out_sampleSize * out_channels;

        //todo E.2 获取音视频时间  时间戳*时间单位    audio_time需要被VideoChannel获取
        audio_time = frame->best_effort_timestamp * av_q2d(time_base);
        if(javaCallHelper){//TODO TEST  两边都有onProgress，所以可以屏蔽一个？有可能会没有音频，所以最好放在VideoChannel？
            javaCallHelper->onProgress(THREAD_CHILD,audio_time);
        }
        break;
    }
    releaseAVFrame(&frame);
    return pcm_data_size;
}
