��Ҫ��Ϊ��
DNFFmpeg.cpp
VideoChannel.cpp
AudioChannel.cpp

#1��VideoChannel���õ���ANativeWindow���л��ƣ�

ANativeWindow������Ǳ��ش��ڣ����Կ���NDK�ṩNative�汾��Surface��ͨ��`ANativeWindow_fromSurface`���ANativeWindowָ�룬`ANativeWindow_release`�����ͷš�����Java�����Զ�������lock��unlockAndPost�Լ�ͨ��`ANativeWindow_Buffer`����ͼ�����ݵ��޸ġ�

```c++
#include <android/native_window_jni.h>
//���ͷ�֮ǰ����ʾ����
if (window) {
	ANativeWindow_release(window);
	window = 0;
}
//�����µĴ���������Ƶ��ʾ
window = ANativeWindow_fromSurface(env, surface);
//���ô�������
ANativeWindow_setBuffersGeometry(window, w,
                                     h,
                                     WINDOW_FORMAT_RGBA_8888);

ANativeWindow_Buffer window_buffer;
if (ANativeWindow_lock(window, &window_buffer, 0)) {
	ANativeWindow_release(window);
	window = 0;
	return;
}
//���rgb���ݸ�dst_data
uint8_t *dst_data = static_cast<uint8_t *>(window_buffer.bits);
//......
ANativeWindow_unlockAndPost(window);
```

��NDK��ʹ��ANativeWindow����ʱ��Ҫ����NDK�е�`libandroid.so`��

```cmake
#��������NDK/platforms/android-X/usr/lib/libandroid.so
target_link_libraries(XXX android )

#2��AudioChannel
     �������Ƶ���Ų��õ��� OpenSL ES
     Android��OpenSL ES��ͬ��λ��NDK��platforms�ļ����ڡ�����OpenSL ES��ʹ�ÿ��Խ���ndk-sample�鿴native-audio����:https://github.com/googlesamples/android-ndk/blob/master/native-audio/app/src/main/cpp/native-audio-jni.c


OpenSL ES�Ŀ���������Ҫ������7�����裺
?   1�������ӿڶ���
?   2�����û�����
?        3������������
?        4�����ò��Żص�����
?        5�����ò���״̬
?        6�������ص�����
?   7���ͷ�
1������������ӿ�
    SLresult result;
    // �������� SLObjectItf engineObject
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // ��ʼ������ 
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // ��ȡ����ӿ�SLEngineItf engineInterface
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE,
                                           &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

2�����û�����
    // ����������SLObjectItf outputMixObject
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0,
                                              0, 0);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    // ��ʼ��������outputMixObject
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //�����û�����Բ��û�ȡ�ӿ�
    // ��û������ӿ�
    //result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
     //                                         &outputMixEnvironmentalReverb);
    //if (SL_RESULT_SUCCESS == result) {
        //���û��� �� Ĭ�ϡ�
        //SL_I3DL2_ENVIRONMENT_PRESET_ROOM: ����
        //SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM : ���� ��
        //const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
        //(*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
         //       outputMixEnvironmentalReverb, &settings);
    //}

3������������
 /**
     * ��������������Ϣ
     */
    //����buffer�������͵Ķ��� 2������
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,2};
    //pcm���ݸ�ʽ
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN};

    //����Դ ������������Ϣ�ŵ��������Դ��
    SLDataSource slDataSource = {&android_queue, &pcm};


    //���û�����
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, NULL};
    //��Ҫ�Ľӿ�
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    //����������
    (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &slDataSource,
                                       &audioSnk, 1,
                                       ids, req);
    //��ʼ��������
    (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);

//    �õ��ӿں����  ��ȡPlayer�ӿ�
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerInterface);


4�����ò��Żص�
//��ȡ���������нӿ�
(*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                    &bqPlayerBufferQueue);
//���ûص�
(*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

5�����ò���״̬
// ���ò���״̬
(*bqPlayerInterface)->SetPlayState(bqPlayerInterface, SL_PLAYSTATE_PLAYING);

6�������ص�����
bqPlayerCallback(bqPlayerBufferQueue, this);

7���ͷ�
 //����ֹͣ״̬
    if (bqPlayerInterface) {
        (*bqPlayerInterface)->SetPlayState(bqPlayerInterface, SL_PLAYSTATE_STOPPED);
        bqPlayerInterface = 0;
    }
    //���ٲ�����
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bqPlayerBufferQueue = 0;
    }
    //���ٻ�����
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
    //��������
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineInterface = 0;
    }




