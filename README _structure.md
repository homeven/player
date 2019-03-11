主要分为类
DNFFmpeg.cpp
VideoChannel.cpp
AudioChannel.cpp

#1、VideoChannel采用的是ANativeWindow进行绘制：

ANativeWindow代表的是本地窗口，可以看成NDK提供Native版本的Surface。通过`ANativeWindow_fromSurface`获得ANativeWindow指针，`ANativeWindow_release`进行释放。类似Java，可以对它进行lock、unlockAndPost以及通过`ANativeWindow_Buffer`进行图像数据的修改。

```c++
#include <android/native_window_jni.h>
//先释放之前的显示窗口
if (window) {
	ANativeWindow_release(window);
	window = 0;
}
//创建新的窗口用于视频显示
window = ANativeWindow_fromSurface(env, surface);
//设置窗口属性
ANativeWindow_setBuffersGeometry(window, w,
                                     h,
                                     WINDOW_FORMAT_RGBA_8888);

ANativeWindow_Buffer window_buffer;
if (ANativeWindow_lock(window, &window_buffer, 0)) {
	ANativeWindow_release(window);
	window = 0;
	return;
}
//填充rgb数据给dst_data
uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
//......
ANativeWindow_unlockAndPost(window);
```

在NDK中使用ANativeWindow编译时需要链接NDK中的`libandroid.so`库

```cmake
#编译链接NDK/platforms/android-X/usr/lib/libandroid.so
target_link_libraries(XXX android )

#2、AudioChannel
     该类的音频播放采用的是 OpenSL ES
     Android的OpenSL ES库同样位于NDK的platforms文件夹内。关于OpenSL ES的使用可以进入ndk-sample查看native-audio工程:https://github.com/googlesamples/android-ndk/blob/master/native-audio/app/src/main/cpp/native-audio-jni.c


OpenSL ES的开发流程主要有如下7个步骤：
?   1、创建接口对象
?   2、设置混音器
?        3、创建播放器
?        4、设置播放回调函数
?        5、设置播放状态
?        6、启动回调函数
?   7、释放
1、创建引擎与接口
    SLresult result;
    // 创建引擎 SLObjectItf engineObject
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 初始化引擎 
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 获取引擎接口SLEngineItf engineInterface
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE,
                                           &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

2、设置混音器
    // 创建混音器SLObjectItf outputMixObject
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0,
                                              0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // 初始化混音器outputMixObject
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //不启用混响可以不用获取接口
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

3、创建播放器
 /**
     * 配置输入声音信息
     */
    //创建buffer缓冲类型的队列 2个队列
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,2};
    //pcm数据格式
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN};

    //数据源 将上述配置信息放到这个数据源中
    SLDataSource slDataSource = {&android_queue, &pcm};


    //设置混音器
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, NULL};
    //需要的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    //创建播放器
    (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &slDataSource,
                                       &audioSnk, 1,
                                       ids, req);
    //初始化播放器
    (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);

//    得到接口后调用  获取Player接口
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerInterface);


4、设置播放回调
//获取播放器队列接口
(*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                    &bqPlayerBufferQueue);
//设置回调
(*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

5、设置播放状态
// 设置播放状态
(*bqPlayerInterface)->SetPlayState(bqPlayerInterface, SL_PLAYSTATE_PLAYING);

6、启动回调函数
bqPlayerCallback(bqPlayerBufferQueue, this);

7、释放
 //设置停止状态
    if (bqPlayerInterface) {
        (*bqPlayerInterface)->SetPlayState(bqPlayerInterface, SL_PLAYSTATE_STOPPED);
        bqPlayerInterface = 0;
    }
    //销毁播放器
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bqPlayerBufferQueue = 0;
    }
    //销毁混音器
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
    //销毁引擎
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineInterface = 0;
    }




