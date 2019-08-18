//
// Created by kangjj on 2019/8/13.
//

#ifndef INC_3_2_4_PLAYER_HAVACALLHEPLER_H
#define INC_3_2_4_PLAYER_HAVACALLHEPLER_H

#include <jni.h>
#include "macro.h"

class JavaCallHelper {

public:
    JavaCallHelper(JavaVM * javaVM_,JNIEnv *env_,jobject instance_);
    ~JavaCallHelper();
    void onPrepared(int threadMode);
//    void onError(int threadMode,int errorCode);
    void onError(int threadMode, int erroCode);

private:
    JavaVM *javaVM;
    JNIEnv *env;
    jobject instance;
    jmethodID jmd_prepared;
    jmethodID jmd_error;
};


#endif //INC_3_2_4_PLAYER_HAVACALLHEPLER_H
