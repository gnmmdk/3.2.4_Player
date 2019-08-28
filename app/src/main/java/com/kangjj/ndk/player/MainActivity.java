package com.kangjj.ndk.player;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    private SurfaceView surfaceView;
    private NEPlayer player;
    private SeekBar seekBar;
    private boolean isTouch;
    private boolean isSeek;
    private static final String TAG = MainActivity.class.getSimpleName();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON,WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        surfaceView = findViewById(R.id.surfaceView);
        seekBar = findViewById(R.id.seekBar);

        player = new NEPlayer();
        player.setSurfaceView(surfaceView);
//        player.setDataSource(new File("/sdcard/","demo.mp4").getAbsolutePath());
        player.setDataSource(new File("/sdcard/","input.mp4").getAbsolutePath());
//        player.setDataSource(new File("/sdcard/","eat.mkv").getAbsolutePath());
        player.setOnPreparedListener(new NEPlayer.OnpreparedListener() {
            @Override
            public void onPrepared() {
                int duration = player.getDuration();
                if(duration!=0){
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            seekBar.setVisibility(View.VISIBLE);
                        }
                    });
                }
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(getApplicationContext(),"开始播放！", Toast.LENGTH_SHORT).show();
                    }
                });
                //播放，调用到native
                player.start();
            }
        });
        player.setOnProgressListener(new NEPlayer.OnProgressListener() {
            @Override
            public void onProgress(final int progress) {
                if(!isTouch){
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            int duration = player.getDuration();
                            if(duration!=0){
                                if(isSeek){
                                    isSeek = false;
                                    return;
                                }
                                seekBar.setProgress(progress*100/duration);
                            }
                        }
                    });
                }
            }
        });
        player.setOnPlayerErrorListener(new NEPlayer.OnPlayerErrorListener() {
            @Override
            public void onError(final int errorCode) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(MainActivity.this, "出错了，错误码：" + errorCode,
                                Toast.LENGTH_SHORT).show();
                    }
                });
            }
        });

        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {

            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                isTouch = true;
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                isSeek = true;
                isTouch = false;
                int seekBarProgress = seekBar.getProgress();
                int duration = player.getDuration();
                int playProgress = seekBarProgress * duration/100;
                //将播放进度传给底层ffmepg av_seek_frame
                player.seekTo(playProgress);
            }
        });
    }

    public void onPrepare(View view) {
        if(player!=null) {

            player.prepare();
        }
    }

    public void onRelease(View view) {
        if(player!=null) {
            player.release();
        }
    }

    public void onStop(View view) {
        if(player!=null) {
            player.stop();
        }
    }
}
