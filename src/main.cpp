#include <QApplication>
#include <QDebug>
#include <QDir>
#include "App/CefApp.h"
#include "UI/MainWindow.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    // Set console output to UTF-8
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Initialize CEF first (handles subprocess)
    if (!CefHelper::Initialize(argc, argv)) {
        // This was a subprocess, exit normally
        return 0;
    }

    // High DPI support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Create Qt application
    QApplication app(argc, argv);
    app.setApplicationName("X互关宝");
    app.setOrganizationName("xfollowing");
    app.setApplicationVersion("1.0.5");

    qDebug() << "[INFO] X互关宝启动中...";

    // Create main window
    MainWindow* window = new MainWindow();
    window->show();

    qDebug() << "[INFO] 主窗口已显示";

    // Run Qt event loop
    int result = app.exec();

    delete window;

    // Shutdown CEF
    CefHelper::Shutdown();

    return result;
}
