package com.dongnao.player;

import android.content.res.Configuration;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.widget.Toast;

import com.trello.rxlifecycle2.components.support.RxAppCompatActivity;

/**
 * @author Lance
 * @date 2018/9/7
 */
public class PlayActivity extends RxAppCompatActivity {
    private DNPlayer dnPlayer;
    public String url;


    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager
                .LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_play);
        SurfaceView surfaceView = findViewById(R.id.surfaceView);
        dnPlayer = new DNPlayer();
        dnPlayer.setSurfaceView(surfaceView);
        dnPlayer.setOnPrepareListener(new DNPlayer.OnPrepareListener() {

            @Override
            public void onPrepare() {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        Toast.makeText(PlayActivity.this, "开始播放", Toast.LENGTH_SHORT).show();
                    }
                });
                dnPlayer.start();
            }
        });

        url = getIntent().getStringExtra("url");
        dnPlayer.setDataSource(url);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager
                    .LayoutParams.FLAG_FULLSCREEN);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
        setContentView(R.layout.activity_play);
        SurfaceView surfaceView = findViewById(R.id.surfaceView);
        dnPlayer.setSurfaceView(surfaceView);
    }

    @Override
    protected void onResume() {
        super.onResume();
        dnPlayer.prepare();
    }

    @Override
    protected void onStop() {
        super.onStop();
        dnPlayer.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        dnPlayer.release();
    }
}
