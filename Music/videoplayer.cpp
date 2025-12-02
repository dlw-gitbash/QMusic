#include "videoplayer.h"
#include "ui_videoplayer.h"
#include "ffmpegwrapper.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>

VideoPlayer::VideoPlayer(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::VideoPlayer)
    , m_ffmpegWrapper(new FFmpegWrapper(this))
    , m_isPlaying(false)
    , m_isDraggingSlider(false)
    , m_uiUpdateTimer(new QTimer(this))
    , m_currentFilePath()
    , m_duration(0.0)
    , m_currentPosition(0.0)
{
    ui->setupUi(this);
    
    // 初始化UI
    ui->positionSlider->setRange(0, 1000);
    ui->positionSlider->setValue(0);
    
    // 连接信号槽
    connect(m_ffmpegWrapper, &FFmpegWrapper::frameReady, this, &VideoPlayer::onFrameReady);
    connect(m_ffmpegWrapper, &FFmpegWrapper::playbackFinished, this, &VideoPlayer::onPlaybackFinished);
    connect(m_ffmpegWrapper, &FFmpegWrapper::errorOccurred, this, &VideoPlayer::onErrorOccurred);
    connect(m_ffmpegWrapper, &FFmpegWrapper::positionChanged, this, &VideoPlayer::onPositionChanged);
    
    // 连接菜单动作
    connect(ui->actionOpen, &QAction::triggered, this, &VideoPlayer::on_openButton_clicked);
    connect(ui->actionExit, &QAction::triggered, this, &QMainWindow::close);
    
    // 连接定时器
    connect(m_uiUpdateTimer, &QTimer::timeout, this, &VideoPlayer::updateUI);
    m_uiUpdateTimer->start(100);
    
    // 初始化播放器状态
    resetPlayer();
}

VideoPlayer::~VideoPlayer()
{
    delete ui;
}

void VideoPlayer::on_openButton_clicked()
{
    // 打开文件对话框
    QString filePath = QFileDialog::getOpenFileName(
                this, 
                tr("打开视频文件"), 
                "/", 
                tr("视频文件 (*.mp4 *.avi *.mkv *.flv *.wmv *.mov);;所有文件 (*.*)")
                );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // 打开视频文件
    if (m_ffmpegWrapper->openFile(filePath)) {
        m_currentFilePath = filePath;
        m_duration = m_ffmpegWrapper->getDuration();
        
        // 更新UI
        ui->totalTimeLabel->setText(formatTime(m_duration));
        ui->positionSlider->setRange(0, static_cast<int>(m_duration * 1000));
        ui->statusLabel->setText(tr("已加载: %1").arg(filePath.split("/").last()));
        
        // 重置播放状态
        m_isPlaying = false;
        ui->playPauseButton->setText(tr("播放"));
        ui->currentTimeLabel->setText(formatTime(0.0));
        ui->positionSlider->setValue(0);
        m_currentPosition = 0.0;
    } else {
        ui->statusLabel->setText(tr("打开文件失败"));
    }
}

void VideoPlayer::on_playPauseButton_clicked()
{
    if (m_currentFilePath.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先打开一个视频文件"));
        return;
    }
    
    if (m_isPlaying) {
        // 暂停播放
        m_ffmpegWrapper->pause();
        m_isPlaying = false;
        ui->playPauseButton->setText(tr("播放"));
        ui->statusLabel->setText(tr("已暂停"));
    } else {
        // 开始播放
        m_ffmpegWrapper->play();
        m_isPlaying = true;
        ui->playPauseButton->setText(tr("暂停"));
        ui->statusLabel->setText(tr("正在播放"));
    }
}

void VideoPlayer::on_stopButton_clicked()
{
    if (m_currentFilePath.isEmpty()) {
        return;
    }
    
    // 停止播放
    m_ffmpegWrapper->stop();
    m_isPlaying = false;
    ui->playPauseButton->setText(tr("播放"));
    ui->statusLabel->setText(tr("已停止"));
    ui->currentTimeLabel->setText(formatTime(0.0));
    ui->positionSlider->setValue(0);
    m_currentPosition = 0.0;
}

void VideoPlayer::on_positionSlider_sliderPressed()
{
    m_isDraggingSlider = true;
}

void VideoPlayer::on_positionSlider_sliderReleased()
{
    m_isDraggingSlider = false;
    
    // 跳转到指定位置
    double position = ui->positionSlider->value() / 1000.0;
    m_ffmpegWrapper->seek(position);
}

void VideoPlayer::on_positionSlider_valueChanged(int value)
{
    if (m_isDraggingSlider) {
        // 正在拖动，只更新时间显示
        double position = value / 1000.0;
        ui->currentTimeLabel->setText(formatTime(position));
    }
}

void VideoPlayer::onFrameReady(const QImage &image)
{
    // 将QImage转换为QPixmap并显示
    QPixmap pixmap = QPixmap::fromImage(image);
    
    // 缩放pixmap以适应label大小，保持宽高比
    QPixmap scaledPixmap = pixmap.scaled(
                ui->videoLabel->size(), 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation);
    
    ui->videoLabel->setPixmap(scaledPixmap);
}

void VideoPlayer::onPlaybackFinished()
{
    m_isPlaying = false;
    ui->playPauseButton->setText(tr("播放"));
    ui->statusLabel->setText(tr("播放结束"));
    ui->positionSlider->setValue(static_cast<int>(m_duration * 1000));
    ui->currentTimeLabel->setText(formatTime(m_duration));
    m_currentPosition = m_duration;
}

void VideoPlayer::onErrorOccurred(const QString &errorMsg)
{
    QMessageBox::critical(this, tr("错误"), errorMsg);
    ui->statusLabel->setText(tr("错误: %1").arg(errorMsg));
    
    // 停止播放
    m_isPlaying = false;
    ui->playPauseButton->setText(tr("播放"));
}

void VideoPlayer::onPositionChanged(double position)
{
    if (!m_isDraggingSlider) {
        // 更新进度条和时间显示
        int sliderValue = static_cast<int>(position * 1000);
        ui->positionSlider->setValue(sliderValue);
        ui->currentTimeLabel->setText(formatTime(position));
    }
}

void VideoPlayer::updateUI()
{
    // 更新播放状态显示
    updatePlaybackStatus();
}

void VideoPlayer::updatePlaybackStatus()
{
    // 根据播放器状态更新UI
    if (m_ffmpegWrapper->isPlaying()) {
        ui->statusLabel->setText(tr("正在播放"));
    } else if (m_ffmpegWrapper->isPaused()) {
        ui->statusLabel->setText(tr("已暂停"));
    } else {
        ui->statusLabel->setText(tr("已停止"));
    }
}

void VideoPlayer::resetPlayer()
{
    // 重置播放器状态
    m_isPlaying = false;
    m_currentPosition = 0.0;
    m_duration = 0.0;
    m_currentFilePath.clear();
    
    // 重置UI
    ui->positionSlider->setRange(0, 1000);
    ui->positionSlider->setValue(0);
    ui->currentTimeLabel->setText(formatTime(0.0));
    ui->totalTimeLabel->setText(formatTime(0.0));
    ui->playPauseButton->setText(tr("播放"));
    ui->statusLabel->setText(tr("就绪"));
    ui->videoLabel->setText(tr("请打开视频文件"));
    ui->videoLabel->setPixmap(QPixmap());
}

QString VideoPlayer::formatTime(double seconds) const
{
    int totalSeconds = static_cast<int>(seconds);
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}
