package com.kangjj.ndk.player;

import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
/**
  * @Description:    todo A FFmepg框架搭建 :根据资料里面的“FFmpeg工程c++核心类图.png”搭建框架:NEFFmpeg、VideoChannel、AudioChannel、BaseChannel、JavaCallHelper
 *                   todo A CMakeList里面的tip
  * @Author:         jj.kang
  * @Email:          jj.kang@zkteco.com
  * @ProjectName:
  * @Package:        com.kangjj.ndk.player
  * @CreateDate:     2019/12/20 15:34
 */
public class NEPlayer {
    static{
        System.loadLibrary("kjjPlayer");
    }

    //准备过程错误码
    public static final int ERROR_CODE_FFMPEG_PREPARE = 1000;
    //播放过程错误码
    public static final int ERROR_CODE_FFMPEG_PLAY = 2000;
    //打不开视频
    public static final int FFMPEG_CAN_NOT_OPEN_URL = (ERROR_CODE_FFMPEG_PREPARE - 1);

    //找不到媒体流信息
    public static final int FFMPEG_CAN_NOT_FIND_STREAMS = (ERROR_CODE_FFMPEG_PREPARE - 2);

    //找不到解码器
    public static final int FFMPEG_FIND_DECODER_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 3);

    //无法根据解码器创建上下文
    public static final int FFMPEG_ALLOC_CODEC_CONTEXT_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 4);

    //根据流信息 配置上下文参数失败
    public static final int FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 5);

    //打开解码器失败
    public static final int FFMPEG_OPEN_DECODER_FAIL = (ERROR_CODE_FFMPEG_PREPARE - 6);

    //没有音视频
    public static final int FFMPEG_NOMEDIA = (ERROR_CODE_FFMPEG_PREPARE - 7);

    //读取媒体数据包失败
    public static final int FFMPEG_READ_PACKETS_FAIL = (ERROR_CODE_FFMPEG_PLAY - 1);

    //seek错误
    public static final int FFMPEG_SEEK_TO_FAIL = (ERROR_CODE_FFMPEG_PLAY - 2);

    //直播地址或媒体文件路径
    private String dataSource;
    private SurfaceHolder surfaceHolder;

    public void setDataSource(String dataSource) {
        this.dataSource = dataSource;
    }

    public void prepare(){
        prepareNative(dataSource);
    }

    public void start(){
        startNative();
    }

    public void setSurfaceView(SurfaceView surfaceView) {
        if(surfaceHolder!=null){
            surfaceHolder.removeCallback(mCallback);
        }
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(mCallback);
    }

    private SurfaceHolder.Callback mCallback = new SurfaceHolder.Callback() {
        @Override
        public void surfaceCreated(SurfaceHolder holder) {

        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            setSurfaceNative(holder.getSurface());
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {

        }
    };

    public void release() {
        surfaceHolder.removeCallback(mCallback);
        releaseNative();
    }


    public void stop() {
        stopNative();
    }


    public int getDuration() {
        return getDurationNative();
    }

    /**
     * 播放进度条跳转
     * @param playProgress
     */
    public void seekTo(final int playProgress) {
        new Thread(){
            @Override
            public void run() {
                seekToNative(playProgress);
            }
        }.start();
    }

    private native void seekToNative(int playProgress);

    private native int getDurationNative();


    private native void stopNative();

    private native void startNative();

    private native void prepareNative(String dataSource);

    private native void releaseNative();

    private native void setSurfaceNative(Surface surface);

    /****************************native回调******************************/
    public void setOnPreparedListener(OnpreparedListener onpreparedListener){
        this.mOnpreparedListener = onpreparedListener;
    }

    public void setOnProgressListener(OnProgressListener onProgressListener){
        this.mOnProgressListener = onProgressListener;
    }

    public void setOnPlayerErrorListener(OnPlayerErrorListener onPlayerErrorListener){
        this.mOnPlayerErrorListener = onPlayerErrorListener;
    }

    /**
     * native反射调用
     * 表示播放器准备好了 可以开始播放了
     */
    public void onPrepared(){
        if(mOnpreparedListener!=null){
            mOnpreparedListener.onPrepared();
        }
    }

    public void onProgress(int progress){
        if(null != mOnProgressListener){
            mOnProgressListener.onProgress(progress);
        }
    }

    /**
     * native反射调用
     * 表示出错
     * @param errorCode
     */
    public void onError(int errorCode){
        if(mOnPlayerErrorListener!=null){
            mOnPlayerErrorListener.onError(errorCode);
        }
    }

    interface OnpreparedListener{
        void onPrepared();
    }

    interface OnProgressListener{
        void onProgress(int progress);
    }

    interface OnPlayerErrorListener{
        void onError(int errCode);
    }

    private OnpreparedListener mOnpreparedListener;
    private OnProgressListener mOnProgressListener;
    private OnPlayerErrorListener mOnPlayerErrorListener;
    /****************************native回调******************************/
}
