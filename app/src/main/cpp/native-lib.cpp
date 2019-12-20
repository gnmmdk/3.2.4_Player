#include <jni.h>
#include <string>
#include "JavaCallHelper.h"
#include "NEFFmpeg.h"
#include <android/native_window_jni.h>

extern "C"{

#include "include/libavutil/avutil.h"
}
JavaVM *javaVM = 0;
JavaCallHelper * javaCallHelper= 0 ;
NEFFmpeg* ffmpeg = 0;
ANativeWindow* window =0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//静态初始化mutex
//extern "C" JNIEXPORT jstring JNICALL
//Java_com_kangjj_ndk_player_MainActivity_stringFromJNI(
//        JNIEnv *env,
//        jobject /* this */) {
//    std::string hello = "Hello from C++";
//    return env->NewStringUTF(av_version_info());
//}
jint JNI_OnLoad(JavaVM *vm,void * reserved){
    javaVM = vm;
    return JNI_VERSION_1_4;
}

void readerFrame(uint8_t * src_data,int src_linesize,int width,int height){
    pthread_mutex_lock(&mutex);
    if(!window){
        pthread_mutex_unlock(&mutex);
        return;
    }
    //设置窗口属性
    ANativeWindow_setBuffersGeometry(window,width,height,WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer window_buffer;
    if(ANativeWindow_lock(window, &window_buffer,0)){
        ANativeWindow_release(window);
        window = 0;
        pthread_mutex_unlock(&mutex);
        return;
    }
    //把buffer中的数据进行赋值 //填充rgb数据给dst_data
    uint8_t * dst_data = static_cast<uint8_t *>(window_buffer.bits);
    int dst_linesize = window_buffer.stride * 4; //ARGB
    //逐行拷贝
    for (int i = 0; i < window_buffer.height; ++i) {
        memcpy(dst_data + i* dst_linesize, src_data + i * src_linesize,dst_linesize);
    }
    ANativeWindow_unlockAndPost(window);
    pthread_mutex_unlock(&mutex);
}
//todo A.1 传递地址进来进行，FFmpeg进行准备动作以及设置RenderCallback
extern "C"
JNIEXPORT void JNICALL
Java_com_kangjj_ndk_player_NEPlayer_prepareNative(JNIEnv *env, jobject instance,
                                                  jstring dataSource_) {
    const char *dataSource = env->GetStringUTFChars(dataSource_, 0);

    javaCallHelper = new JavaCallHelper(javaVM,env,instance);
    ffmpeg = new NEFFmpeg(javaCallHelper, const_cast<char *>(dataSource));
    ffmpeg->setRenderCallback(readerFrame);
    ffmpeg->prepare();

    env->ReleaseStringUTFChars(dataSource_, dataSource);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_kangjj_ndk_player_NEPlayer_startNative(JNIEnv *env, jobject instance) {
    if(ffmpeg){
        ffmpeg->start();
    }

}
extern "C"
JNIEXPORT void JNICALL
Java_com_kangjj_ndk_player_NEPlayer_setSurfaceNative(JNIEnv *env, jobject instance,
                                                     jobject surface) {
    pthread_mutex_lock(&mutex);

    if(window){
        ANativeWindow_release(window);
        window = 0;
    }
    //创建新的窗口窗口用于视频显示
    window = ANativeWindow_fromSurface(env,surface);

    pthread_mutex_unlock(&mutex);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_kangjj_ndk_player_NEPlayer_releaseNative(JNIEnv *env, jobject instance) {

    pthread_mutex_lock(&mutex);
    if(window){
        ANativeWindow_release(window);
        window = 0 ;
    }
    pthread_mutex_unlock(&mutex);
    DELETE(ffmpeg);

}
extern "C"
JNIEXPORT void JNICALL
Java_com_kangjj_ndk_player_NEPlayer_stopNative(JNIEnv *env, jobject instance) {
    if(ffmpeg){
        ffmpeg->stop();
    }

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_kangjj_ndk_player_NEPlayer_getDurationNative(JNIEnv *env, jobject instance) {
    if(ffmpeg){
        return ffmpeg->getDuration();
    }
    return 0;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_kangjj_ndk_player_NEPlayer_seekToNative(JNIEnv *env, jobject instance, jint playProgress) {

    if(ffmpeg){
        ffmpeg->seekTo(playProgress);
    }

}