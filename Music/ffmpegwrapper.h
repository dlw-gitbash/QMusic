#ifndef FFMPEGWRAPPER_H
#define FFMPEGWRAPPER_H

#include <QObject>
#include <QImage>
#include <QString>
#include <QThread>
#include <QMutex>

// Forward declarations for FFmpeg structs to improve compile time
extern "C" {
    struct AVFormatContext;
    struct AVCodecContext;
    struct AVStream;
    struct SwsContext;
    struct AVFrame;
    struct AVPacket;
}

/**
 * @brief FFmpeg封装类，负责视频文件的解码和播放控制
 * 
 * 该类封装了FFmpeg的核心功能，提供了简洁的接口用于视频播放控制
 * 使用多线程进行视频解码，避免阻塞UI线程
 */
class FFmpegWrapper : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit FFmpegWrapper(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~FFmpegWrapper() override;
    
    /**
     * @brief 打开视频文件
     * @param filePath 视频文件路径
     * @return 是否成功打开
     */
    bool openFile(const QString &filePath);
    
    /**
     * @brief 关闭视频文件
     */
    void closeFile();
    
    /**
     * @brief 开始播放视频
     */
    void play();
    
    /**
     * @brief 暂停视频播放
     */
    void pause();
    
    /**
     * @brief 停止视频播放
     */
    void stop();
    
    /**
     * @brief 跳转到指定位置
     * @param position 目标位置（秒）
     */
    void seek(double position);
    
    /**
     * @brief 获取视频时长
     * @return 视频时长（秒）
     */
    double getDuration() const;
    
    /**
     * @brief 获取当前播放位置
     * @return 当前位置（秒）
     */
    double getCurrentPosition() const;
    
    /**
     * @brief 获取视频宽度
     * @return 视频宽度（像素）
     */
    int getVideoWidth() const;
    
    /**
     * @brief 获取视频高度
     * @return 视频高度（像素）
     */
    int getVideoHeight() const;
    
    /**
     * @brief 检查是否正在播放
     * @return 是否正在播放
     */
    bool isPlaying() const;
    
    /**
     * @brief 检查是否已暂停
     * @return 是否已暂停
     */
    bool isPaused() const;

signals:
    /**
     * @brief 视频帧就绪信号
     * @param image 解码后的视频帧
     */
    void frameReady(const QImage &image);
    
    /**
     * @brief 播放结束信号
     */
    void playbackFinished();
    
    /**
     * @brief 错误发生信号
     * @param errorMsg 错误信息
     */
    void errorOccurred(const QString &errorMsg);
    
    /**
     * @brief 播放位置更新信号
     * @param position 当前位置（秒）
     */
    void positionChanged(double position);

private slots:
    /**
     * @brief 解码线程主函数
     */
    void decodeLoop();

private:
    /**
     * @brief 初始化FFmpeg库
     */
    void initializeFFmpeg();
    
    /**
     * @brief 释放所有资源
     */
    void freeResources();
    
    /**
     * @brief 解码单个视频帧
     * @param packet 待解码的数据包
     * @return 是否成功解码
     */
    bool decodeVideoFrame(AVPacket *packet);
    
    // Thread management
    QThread *m_decodeThread;
    bool m_isRunning;
    bool m_isPaused;
    mutable QMutex m_mutex;
    
    // FFmpeg core components
    AVFormatContext *m_formatCtx;
    AVCodecContext *m_videoCodecCtx;
    AVStream *m_videoStream;
    int m_videoStreamIndex;
    SwsContext *m_swsCtx;
    
    // Video information
    double m_duration;
    double m_currentPosition;
    int m_videoWidth;
    int m_videoHeight;
    
    // Frame buffers
    AVFrame *m_rawFrame;
    AVFrame *m_rgbFrame;
    uint8_t *m_rgbBuffer;
    int m_rgbBufferSize;
    
    // Current file path
    QString m_currentFilePath;
};

#endif // FFMPEGWRAPPER_H
