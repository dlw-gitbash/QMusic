#include <QApplication>
#include <QIcon>
#include "videoplayer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序属性
    QApplication::setApplicationName("Qt6 FFmpeg视频播放器");
    QApplication::setApplicationVersion("1.0.0");
    
    // 设置应用程序图标
    QIcon appIcon("music.ico");
    QApplication::setWindowIcon(appIcon);
    
    // 创建主窗口
    VideoPlayer w;
    w.setWindowIcon(appIcon);
    w.show();
    
    return a.exec();
}
