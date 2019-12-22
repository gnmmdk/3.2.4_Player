//
// Created by PC on 2019/8/15.
//

#ifndef INC_3_2_4_PLAYER_SAFE_QUEUE_H
#define INC_3_2_4_PLAYER_SAFE_QUEUE_H


#include <queue>//为什么BaseChannel导入save_channel.h queue就可以用了
#include <pthread.h>

using namespace std;
//todo  理解这个类
template <typename T>//todo  类似java的泛型
class SafeQueue{
    typedef void(*ReleaseCallback)(T*);     //todo 回掉接口 类似java的指针
    typedef void(*SyncHandle)(queue<T> &);
public:
    SafeQueue(){
        pthread_mutex_init(&mutex,0);//todo 锁 动态初始化
        pthread_cond_init(&cond,0);//todo 锁
    }
    ~SafeQueue(){
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }
    /**
     * 入队
     * @param value
     */
    void push(T value){
        pthread_mutex_lock(&mutex);
        if(work){
            q.push(value);
            pthread_cond_signal(&cond);
        }else{
            //非工作状态
            if(releaseCallback){
                releaseCallback(&value);
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    /**
     * 出队
     * @param value
     * @return
     */
    int pop(T &value){
        int ret = 0;
        pthread_mutex_lock(&mutex);
        while(work && q.empty()){
            //工作状态，说明确实需要pop，但是队列为空，需要等待
            pthread_cond_wait(&cond,&mutex);
        }
        if(!q.empty()){
            value = q.front();
            //弹出
            q.pop();
            ret = 1;
        }
        pthread_mutex_unlock(&mutex);
        return ret;
    }
    /**
     * 设置队列的工作状态
     * @param work
     */
    void setWork(int work){
        pthread_mutex_lock(&mutex);
        this->work = work;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
    int empty(){
        return q.empty();
    }

    int size(){
        return q.size();
    }
    void clear(){
        pthread_mutex_lock(&mutex);
        unsigned int size = q.size();
        for (int i = 0; i < size; ++i) {
            //取出队首元素
            T value = q.front();
            if(releaseCallback){
                releaseCallback(&value);
            }
            q.pop();
        }
        pthread_mutex_unlock(&mutex);
    }

    void setReleaseCallback(ReleaseCallback releaseCallback){
        this->releaseCallback = releaseCallback;
    }

    void setSyncHandle(SyncHandle syncHandle1){
        this->syncHandle = syncHandle1;
    }
    /**
         * 同步操作
         * 丢包
         */
    void sync(){
        pthread_mutex_lock(&mutex);
        syncHandle(q);
        pthread_mutex_unlock(&mutex);
    }

private:
    queue<T> q;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int work;//标记队列是否在工作
    ReleaseCallback releaseCallback;
    SyncHandle syncHandle;
};
#endif //INC_3_2_4_PLAYER_SAFE_QUEUE_H
