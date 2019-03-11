//
// Created by Administrator on 2018/9/5.
//

#include <cstring>
#include <pthread.h>

extern "C" {
#include <libavutil/time.h>
}

#include "DNFFmpeg.h"
#include "macro.h"


void *task_prepare(void *args) {
    DNFFmpeg *ffmpeg = static_cast<DNFFmpeg *>(args);
    ffmpeg->_prepare();
    return 0;
}

DNFFmpeg::DNFFmpeg(JavaCallHelper *callHelper, const char *dataSource) {
    this->callHelper = callHelper;
    //防止 dataSource参数 指向的内存被释放
    //strlen 获得字符串的长度 不包括\0
    this->dataSource = new char[strlen(dataSource) + 1];
    strcpy(this->dataSource, dataSource);
}

DNFFmpeg::~DNFFmpeg() {
    //释放
    DELETE(dataSource);
}


void DNFFmpeg::prepare() {
    //创建一个线程
    pthread_create(&pid, 0, task_prepare, this);
}

void DNFFmpeg::_prepare() {
    // 初始化网络 让ffmpeg能够使用网络
    avformat_network_init();
    //1、打开媒体地址(文件地址、直播地址)
    // AVFormatContext  包含了 视频的 信息(宽、高等)
    //文件路径不对 手机没网
    // 第3个参数： 指示打开的媒体格式(传NULL，ffmpeg就会自动推到出是mp4还是flv)
    AVDictionary *options = 0;
    //设置超时时间 微妙 超时时间5秒
    av_dict_set(&options, "timeout", "5000000", 0);
    int ret = avformat_open_input(&formatContext, dataSource, 0, &options);
    av_dict_free(&options);
    //ret不为0表示 打开媒体失败
    if (ret != 0) {
        LOGE("打开媒体失败:%s", av_err2str(ret));
        if (isPlaying) {
            callHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_OPEN_URL);
        }
        return;
    }
    //2、查找媒体中的 音视频流 (给 contxt里的 streams等成员赋)
    ret = avformat_find_stream_info(formatContext, 0);
    // 小于0 则失败
    if (ret < 0) {
        LOGE("查找流失败:%s", av_err2str(ret));
        if (isPlaying) {
            callHelper->onError(THREAD_CHILD, FFMPEG_CAN_NOT_FIND_STREAMS);
        }
        return;
    }
    //nb_streams :几个流(几段视频/音频)
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        //可能代表是一个视频 也可能代表是一个音频
        AVStream *stream = formatContext->streams[i];
        //包含了 解码 这段流 的各种参数信息(宽、高、码率、帧率)
        AVCodecParameters *codecpar = stream->codecpar;

        //无论视频还是音频都需要干的一些事情（获得解码器）
        // 1、通过 当前流 使用的 编码方式，查找解码器
        AVCodec *dec = avcodec_find_decoder(codecpar->codec_id);
        if (dec == NULL) {
            LOGE("查找解码器失败:%s", av_err2str(ret));
            if (isPlaying) {
                callHelper->onError(THREAD_CHILD, FFMPEG_FIND_DECODER_FAIL);
            }
            return;
        }
        //2、获得解码器上下文
        AVCodecContext *context = avcodec_alloc_context3(dec);
        if (context == NULL) {
            LOGE("创建解码上下文失败:%s", av_err2str(ret));
            if (isPlaying) {
                callHelper->onError(THREAD_CHILD, FFMPEG_ALLOC_CODEC_CONTEXT_FAIL);
            }
            return;
        }
        //3、设置上下文内的一些参数 (context->width)
//        context->width = codecpar->width;
//        context->height = codecpar->height;
        ret = avcodec_parameters_to_context(context, codecpar);
        //失败
        if (ret < 0) {
            LOGE("设置解码上下文参数失败:%s", av_err2str(ret));
            if (isPlaying) {
                callHelper->onError(THREAD_CHILD, FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL);
            }
            return;
        }
        // 4、打开解码器
        ret = avcodec_open2(context, dec, 0);
        if (ret != 0) {
            LOGE("打开解码器失败:%s", av_err2str(ret));
            if (isPlaying) {
                callHelper->onError(THREAD_CHILD, FFMPEG_OPEN_DECODER_FAIL);
            }
            return;
        }
        // 单位
        AVRational time_base = stream->time_base;
        //音频
        if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            //0
            audioChannel = new AudioChannel(i, context, time_base);
        } else if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            //1
            //帧率： 单位时间内 需要显示多少个图像
            AVRational frame_rate = stream->avg_frame_rate;
            int fps = av_q2d(frame_rate);

            videoChannel = new VideoChannel(i, context, time_base, fps);
            videoChannel->setRenderFrameCallback(callback);
        }
    }
    //没有音视频  (很少见)
    if (!audioChannel && !videoChannel) {
        LOGE("没有音视频");
        callHelper->onError(THREAD_CHILD, FFMPEG_NOMEDIA);
        return;
    }
    // 准备完了 通知java 你随时可以开始播放
    if (isPlaying) {
        callHelper->onPrepare(THREAD_CHILD);
    }
};

void *play(void *args) {
    DNFFmpeg *ffmpeg = static_cast<DNFFmpeg *>(args);
    ffmpeg->_start();
    return 0;
}


void DNFFmpeg::start() {
    // 正在播放
    isPlaying = 1;

    //启动声音的解码与播放
    if (audioChannel) {
        audioChannel->play();
    }
    if (videoChannel) {
        videoChannel->setAudioChannel(audioChannel);
        videoChannel->play();
    }
    pthread_create(&pid_play, 0, play, this);
}

/**
 * 专门读取数据包
 */
void DNFFmpeg::_start() {
    //1、读取媒体数据包(音视频数据包)
    int ret;
    while (isPlaying) {
        //读取文件的时候没有网络请求，一下子读完了，可能导致oom
        //特别是读本地文件的时候 一下子就读完了
        if (audioChannel && audioChannel->packets.size() > 100) {
            //10ms
            av_usleep(1000 * 10);
            continue;
        }
        if (videoChannel && videoChannel->packets.size() > 100) {
            av_usleep(1000 * 10);
            continue;
        }
        AVPacket *packet = av_packet_alloc();
        ret = av_read_frame(formatContext, packet);
        //=0成功 其他:失败
        if (ret == 0) {
            //stream_index 这一个流的一个序号
            if (audioChannel && packet->stream_index == audioChannel->id) {
                audioChannel->packets.push(packet);
            } else if (videoChannel && packet->stream_index == videoChannel->id) {
                videoChannel->packets.push(packet);
            }
        } else if (ret == AVERROR_EOF) {
            //读取完成 但是可能还没播放完
            if (audioChannel->packets.empty() && audioChannel->frames.empty()
                && videoChannel->packets.empty() && videoChannel->frames.empty()) {
                break;
            }
            //为什么这里要让它继续循环 而不是sleep
            //如果是做直播 ，可以sleep
            //如果要支持点播(播放本地文件） seek 后退
        } else {
            //
            break;
        }
    }
    isPlaying = 0;
    audioChannel->stop();
    videoChannel->stop();
};

void DNFFmpeg::setRenderFrameCallback(RenderFrameCallback callback) {
    this->callback = callback;
}

void *aync_stop(void *args) {
    DNFFmpeg *ffmpeg = static_cast<DNFFmpeg *>(args);
    //   等待prepare结束
    pthread_join(ffmpeg->pid, 0);
    // 保证 start线程结束
    pthread_join(ffmpeg->pid_play, 0);
    DELETE(ffmpeg->videoChannel);
    DELETE(ffmpeg->audioChannel);
    // 这时候释放就不会出现问题了
    if (ffmpeg->formatContext) {
        //先关闭读取 (关闭fileintputstream)
        avformat_close_input(&ffmpeg->formatContext);
        avformat_free_context(ffmpeg->formatContext);
        ffmpeg->formatContext = 0;
    }
    DELETE(ffmpeg);
    return 0;
}

void DNFFmpeg::stop() {
    isPlaying = 0;
    callHelper = 0;
    // formatContext
    pthread_create(&pid_stop, 0, aync_stop, this);
};