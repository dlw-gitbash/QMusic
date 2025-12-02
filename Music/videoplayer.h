#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QMainWindow>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QString>

// Forward declaration to reduce compile time
class FFmpegWrapper;

QT_BEGIN_NAMESPACE
namespace Ui { class VideoPlayer; }
QT_END_NAMESPACE

/**
 * @brief 视频播放器主窗口类
 * 
 * 该类实现了视频播放器的UI界面和控制逻辑，包括：
 * - 视频显示
 * - 播放控制（播放/暂停/停止）
 * - 进度条控制
 * - 文件选择
 * - 时间显示
 */
class VideoPlayer : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit VideoPlayer(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~VideoPlayer() override;

private slots:
    /**
     * @brief 打开文件按钮点击事件
     */
    void on_openButton_clicked();
    
    /**
     * @brief 播放/暂停按钮点击事件
     */
    void on_playPauseButton_clicked();
    
    /**
     * @brief 停止按钮点击事件
     */
    void on_stopButton_clicked();
    
    /**
     * @brief 进度条拖动开始事件
     */
    void on_positionSlider_sliderPressed();
    
    /**
     * @brief 进度条拖动结束事件
     */
    void on_positionSlider_sliderReleased();
    
    /**
     * @brief 进度条值改变事件
     * @param value 新的进度值
     */
    void on_positionSlider_valueChanged(int value);
    
    /**
     * @brief 视频帧更新事件
     * @param image 解码后的视频帧
     */
    void onFrameReady(const QImage &image);
    
    /**
     * @brief 播放结束事件
     */
    void onPlaybackFinished();
    
    /**
     * @brief 错误发生事件
     * @param errorMsg 错误信息
     */
    void onErrorOccurred(const QString &errorMsg);
    
    /**
     * @brief 播放位置更新事件
     * @param position 当前位置（秒）
     */
    void onPositionChanged(double position);
    
    /**
     * @brief UI更新定时器事件
     */
    void updateUI();

private:
    /**
     * @brief 格式化时间
     * @param seconds 时间（秒）
     * @return 格式化后的时间字符串（mm:ss）
     */
    QString formatTime(double seconds) const;
    
    /**
     * @brief 更新播放状态显示
     */
    void updatePlaybackStatus();
    
    /**
     * @brief 重置播放器状态
     */
    void resetPlayer();
    
    Ui::VideoPlayer *ui;
    
    // Video playback core
    FFmpegWrapper *m_ffmpegWrapper;
    
    // UI state
    bool m_isPlaying;
    bool m_isDraggingSlider;
    
    // UI update timer
    QTimer *m_uiUpdateTimer;
    
    // Video information
    QString m_currentFilePath;
    double m_duration;
    double m_currentPosition;
};

#endif // VIDEOPLAYER_H
