//
// Created by kangjj on 2019/8/13.
//

#include "JavaCallHelper.h"
JavaCallHelper::JavaCallHelper(JavaVM *javaVM_, JNIEnv *env_, jobject instance_) {
    this->javaVM = javaVM_;
    this->env = env_;
//    this->instance = instance_;//不能直接复制！
    //todo B.8.1 一旦涉及到jobject跨方法、线程、需要创建全局引用
    this->instance = env->NewGlobalRef(instance_);
    jclass clazz =env->GetObjectClass(instance);
    //todo B.8.2（）V 括号里面是传参数 V是返回值Void
    jmd_prepared = env->GetMethodID(clazz,"onPrepared","()V");
    jmd_error = env->GetMethodID(clazz,"onError","(I)V");
    jmd_progress = env->GetMethodID(clazz,"onProgress","(I)V");
}

JavaCallHelper::~JavaCallHelper() {
    javaVM = 0;
    env->DeleteGlobalRef(instance);
    instance = 0;
}

void JavaCallHelper::onPrepared(int threadMode) {
    if(threadMode == THREAD_MAIN){
        //主线程
        env->CallVoidMethod(instance,jmd_prepared);
    }else{
        JNIEnv * env_child;                     //todo B.8.3 子线程用自定义的JNIEnv
        javaVM->AttachCurrentThread(&env_child,0);//todo B.8.4 子线程需要包裹起来
        env_child->CallVoidMethod(instance,jmd_prepared);
        javaVM->DetachCurrentThread();//todo B.8.3 子线程需要包裹起来
    }
}

void JavaCallHelper::onError(int threadMode, int erroCode) {
    if(threadMode == THREAD_MAIN){
        env->CallVoidMethod(instance,jmd_error);
    }else{
        JNIEnv * env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(instance,jmd_error,erroCode);
        javaVM->DetachCurrentThread();
    }

}

void JavaCallHelper::onProgress(int threadMode, int progress) {
    if(threadMode == THREAD_MAIN){
        env->CallVoidMethod(instance,jmd_progress);
    }else{
        JNIEnv* env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(instance,jmd_progress,progress);
        javaVM->DetachCurrentThread();
    }

}







