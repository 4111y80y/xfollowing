#include "Logger.h"
#include <QDebug>
#include <QDateTime>

namespace Logger {

void init() {
    // 日志初始化在CefHandler中完成
}

void log(const QString& msg) {
    qDebug() << msg;
}

void debug(const QString& msg) {
    qDebug() << "[DEBUG]" << msg;
}

void info(const QString& msg) {
    qDebug() << "[INFO]" << msg;
}

void warning(const QString& msg) {
    qWarning() << "[WARNING]" << msg;
}

void error(const QString& msg) {
    qCritical() << "[ERROR]" << msg;
}

} // namespace Logger
