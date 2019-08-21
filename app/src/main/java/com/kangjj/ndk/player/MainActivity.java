package com.kangjj.ndk.player;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.SurfaceView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    private SurfaceView surfaceView;
    private NEPlayer player;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        surfaceView = findViewById(R.id.surfaceView);
        player = new NEPlayer();
        player.setSurfaceView(surfaceView);
        player.setDataSource(new File("/sdcard/","demo.mp4").getAbsolutePath());
//        player.setDataSource(new File("/sdcard/","input.mp4").getAbsolutePath());
        player.setOnPreparedListener(new NEPlayer.OnpreparedListener() {
            @Override
            public void onPrepared() {
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
    }

    @Override
    protected void onResume() {
        super.onResume();
        if(player!=null) {
            player.prepare();
        }
    }
}
