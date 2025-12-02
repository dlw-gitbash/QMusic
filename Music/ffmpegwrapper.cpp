#include "ffmpegwrapper.h"
#include <QDebug>

// FFmpeg头文件
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

FFmpegWrapper::FFmpegWrapper(QObject *parent) : QObject(parent)
    , m_decodeThread(nullptr)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_formatCtx(nullptr)
    , m_videoCodecCtx(nullptr)
    , m_videoStream(nullptr)
    , m_videoStreamIndex(-1)
    , m_swsCtx(nullptr)
    , m_duration(0.0)
    , m_currentPosition(0.0)
    , m_videoWidth(0)
    , m_videoHeight(0)
    , m_rawFrame(nullptr)
    , m_rgbFrame(nullptr)
    , m_rgbBuffer(nullptr)
    , m_rgbBufferSize(0)
    , m_currentFilePath()
{
    initializeFFmpeg();
    
    // 创建解码线程
    m_decodeThread = new QThread(this);
    this->moveToThread(m_decodeThread);
    
    // 连接线程信号
    connect(m_decodeThread, &QThread::started, this, &FFmpegWrapper::decodeLoop);
    connect(m_decodeThread, &QThread::finished, this, [this]() {
        m_isRunning = false;
    });
}

FFmpegWrapper::~FFmpegWrapper()
{
    closeFile();
    freeResources();
    
    if (m_decodeThread) {
        m_decodeThread->quit();
        m_decodeThread->wait();
        delete m_decodeThread;
    }
}

void FFmpegWrapper::initializeFFmpeg()
{
    // 在FFmpeg 4.4中，av_register_all()已被废弃且不再需要
    // 只需要初始化网络模块
    avformat_network_init();
}

bool FFmpegWrapper::openFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    // 关闭之前的文件
    closeFile();
    
    // 保存当前文件路径
    m_currentFilePath = filePath;
    
    // 打开输入文件
    const char *cFilePath = filePath.toUtf8().constData();
    if (avformat_open_input(&m_formatCtx, cFilePath, nullptr, nullptr) != 0) {
        emit errorOccurred("无法打开视频文件");
        return false;
    }
    
    // 获取流信息
    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        emit errorOccurred("无法获取流信息");
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    
    // 查找视频流
    m_videoStreamIndex = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoStreamIndex < 0) {
        emit errorOccurred("未找到视频流");
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    
    // 获取视频流
    m_videoStream = m_formatCtx->streams[m_videoStreamIndex];
    
    // 查找视频解码器
    const AVCodec *videoCodec = avcodec_find_decoder(m_videoStream->codecpar->codec_id);
    if (!videoCodec) {
        emit errorOccurred("未找到合适的解码器");
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    
    // 创建解码器上下文
    m_videoCodecCtx = avcodec_alloc_context3(videoCodec);
    if (!m_videoCodecCtx) {
        emit errorOccurred("无法创建解码器上下文");
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    
    // 从流复制解码器参数
    if (avcodec_parameters_to_context(m_videoCodecCtx, m_videoStream->codecpar) < 0) {
        emit errorOccurred("无法复制解码器参数");
        avcodec_free_context(&m_videoCodecCtx);
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    
    // 打开解码器
    if (avcodec_open2(m_videoCodecCtx, videoCodec, nullptr) < 0) {
        emit errorOccurred("无法打开解码器");
        avcodec_free_context(&m_videoCodecCtx);
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
        return false;
    }
    
    // 获取视频信息
    m_videoWidth = m_videoCodecCtx->width;
    m_videoHeight = m_videoCodecCtx->height;
    m_duration = m_formatCtx->duration / (double)AV_TIME_BASE;
    
    // 分配视频帧
    m_rawFrame = av_frame_alloc();
    m_rgbFrame = av_frame_alloc();
    
    // 计算RGB缓冲区大小
    m_rgbBufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);
    m_rgbBuffer = (uint8_t *)av_malloc(m_rgbBufferSize);
    
    // 设置RGB帧的数据指针
    av_image_fill_arrays(m_rgbFrame->data, m_rgbFrame->linesize, m_rgbBuffer, 
                         AV_PIX_FMT_RGB24, m_videoWidth, m_videoHeight, 1);
    
    // 创建SWS上下文用于格式转换
    m_swsCtx = sws_getContext(m_videoWidth, m_videoHeight, m_videoCodecCtx->pix_fmt,
                              m_videoWidth, m_videoHeight, AV_PIX_FMT_RGB24,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    return true;
}

void FFmpegWrapper::closeFile()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_isRunning) {
        m_isRunning = false;
        if (m_decodeThread->isRunning()) {
            m_decodeThread->quit();
            m_decodeThread->wait();
        }
    }
    
    freeResources();
}

void FFmpegWrapper::freeResources()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    
    if (m_rgbBuffer) {
        av_free(m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }
    
    if (m_rgbFrame) {
        av_frame_free(&m_rgbFrame);
        m_rgbFrame = nullptr;
    }
    
    if (m_rawFrame) {
        av_frame_free(&m_rawFrame);
        m_rawFrame = nullptr;
    }
    
    if (m_videoCodecCtx) {
        avcodec_free_context(&m_videoCodecCtx);
        m_videoCodecCtx = nullptr;
    }
    
    if (m_formatCtx) {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }
    
    m_videoStream = nullptr;
    m_videoStreamIndex = -1;
    m_duration = 0.0;
    m_currentPosition = 0.0;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_currentFilePath.clear();
}

void FFmpegWrapper::play()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_formatCtx) {
        return;
    }
    
    if (!m_isRunning) {
        m_isRunning = true;
        m_decodeThread->start();
    }
    
    m_isPaused = false;
}

void FFmpegWrapper::pause()
{
    QMutexLocker locker(&m_mutex);
    m_isPaused = true;
}

void FFmpegWrapper::stop()
{
    // 先设置停止标志
    {   
        QMutexLocker locker(&m_mutex);
        m_isRunning = false;
        m_isPaused = false;
    }
    
    // 等待线程结束（在互斥锁外）
    if (m_decodeThread->isRunning()) {
        m_decodeThread->quit();
        m_decodeThread->wait();
    }
    
    // 跳转到开头和重置位置
    {   
        QMutexLocker locker(&m_mutex);
        // 重置位置
        m_currentPosition = 0.0;
        emit positionChanged(m_currentPosition);
        
        // 跳转到开头
        if (m_formatCtx) {
            int64_t targetTimestamp = 0;
            if (av_seek_frame(m_formatCtx, m_videoStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD) >= 0) {
                avcodec_flush_buffers(m_videoCodecCtx);
            }
        }
    }
}

void FFmpegWrapper::seek(double position)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_formatCtx) {
        return;
    }
    
    // 计算目标时间戳
    int64_t targetTimestamp = position * AV_TIME_BASE;
    
    // 跳转到指定位置
    if (av_seek_frame(m_formatCtx, m_videoStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD) >= 0) {
        // 刷新解码器缓冲区
        avcodec_flush_buffers(m_videoCodecCtx);
        m_currentPosition = position;
        emit positionChanged(m_currentPosition);
    }
}

double FFmpegWrapper::getDuration() const
{
    QMutexLocker locker(&m_mutex);
    return m_duration;
}

double FFmpegWrapper::getCurrentPosition() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentPosition;
}

int FFmpegWrapper::getVideoWidth() const
{
    QMutexLocker locker(&m_mutex);
    return m_videoWidth;
}

int FFmpegWrapper::getVideoHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_videoHeight;
}

bool FFmpegWrapper::isPlaying() const
{
    QMutexLocker locker(&m_mutex);
    return m_isRunning && !m_isPaused;
}

bool FFmpegWrapper::isPaused() const
{
    QMutexLocker locker(&m_mutex);
    return m_isRunning && m_isPaused;
}

void FFmpegWrapper::decodeLoop()
{
    AVPacket packet;
    
    // 帧率控制：限制最大帧率为30fps
    const int64_t frameInterval = 1000 / 30; // 毫秒
    int64_t lastFrameTime = av_gettime_relative() / 1000; // 毫秒
    
    while (m_isRunning) {
        {   
            QMutexLocker locker(&m_mutex);
            if (m_isPaused) {
                locker.unlock();
                QThread::msleep(100);
                continue;
            }
        }
        
        // 初始化数据包
        av_init_packet(&packet);
        packet.data = nullptr;
        packet.size = 0;
        
        // 读取数据包
        int ret;
        {   
            QMutexLocker locker(&m_mutex);
            ret = av_read_frame(m_formatCtx, &packet);
        }
        
        if (ret >= 0) {
            {   
                QMutexLocker locker(&m_mutex);
                if (packet.stream_index == m_videoStreamIndex) {
                    // 解码视频帧
                    if (decodeVideoFrame(&packet)) {
                        // 更新当前位置
                        if (m_rawFrame->pts != AV_NOPTS_VALUE) {
                            m_currentPosition = m_rawFrame->pts * av_q2d(m_videoStream->time_base);
                            emit positionChanged(m_currentPosition);
                        }
                    }
                }
            }
            
            av_packet_unref(&packet);
            
            // 帧率控制：限制解码速度
            int64_t currentTime = av_gettime_relative() / 1000;
            int64_t elapsed = currentTime - lastFrameTime;
            if (elapsed < frameInterval) {
                QThread::msleep(frameInterval - elapsed);
            }
            lastFrameTime = currentTime;
        } else {
            // 播放结束
            break;
        }
    }
    
    // 发送播放结束信号
    emit playbackFinished();
    
    // 停止线程
    m_decodeThread->quit();
}

bool FFmpegWrapper::decodeVideoFrame(AVPacket *packet)
{
    QMutexLocker locker(&m_mutex);
    
    // 解码视频帧
    int ret = avcodec_send_packet(m_videoCodecCtx, packet);
    if (ret < 0) {
        return false;
    }
    
    ret = avcodec_receive_frame(m_videoCodecCtx, m_rawFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return false;
    } else if (ret < 0) {
        return false;
    }
    
    // 转换为RGB格式
    sws_scale(m_swsCtx, m_rawFrame->data, m_rawFrame->linesize,
              0, m_videoHeight, m_rgbFrame->data, m_rgbFrame->linesize);
    
    // 创建QImage（使用copy避免线程安全问题）
    QImage image(m_rgbBuffer, m_videoWidth, m_videoHeight, QImage::Format_RGB888);
    QImage frameCopy = image.copy();
    
    // 发送帧信号（在互斥锁外发送，避免死锁）
    locker.unlock();
    emit frameReady(frameCopy);
    
    return true;
}
