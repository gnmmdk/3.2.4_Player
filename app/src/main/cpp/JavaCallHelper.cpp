//
// Created by kangjj on 2019/8/13.
//

#include "JavaCallHelper.h"
JavaCallHelper::JavaCallHelper(JavaVM *javaVM_, JNIEnv *env_, jobject instance_) {
    this->javaVM = javaVM_;
    this->env = env_;
    //一旦涉及到jobject跨方法、线程、需要创建全局引用
//    this->instance = instance_;//不能直接复制！
    this->instance = env->NewGlobalRef(instance_);
    jclass clazz =env->GetObjectClass(instance);
    //（）V 括号里面是传参数 V是返回值Void
    jmd_prepared = env->GetMethodID(clazz,"onPrepared","()V");
    jmd_error = env->GetMethodID(clazz,"onError","(I)V");
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
        JNIEnv * env_child;
        javaVM->AttachCurrentThread(&env_child,0);
        env_child->CallVoidMethod(instance,jmd_prepared);
        javaVM->DetachCurrentThread();
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







