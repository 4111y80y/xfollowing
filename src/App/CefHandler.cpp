#include "CefHandler.h"
#include "include/wrapper/cef_helpers.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QMutex>
#include <QThread>
#include <windows.h>

// Global log file for debugging
static QFile* g_logFile = nullptr;
static QMutex g_logMutex;

static void initLog() {
    if (g_logFile) return;

    QString logDir = QCoreApplication::applicationDirPath() + "/logs";
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString logPath = logDir + "/debug_" + timestamp + ".log";

    g_logFile = new QFile(logPath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(g_logFile);
        out << "=== X互关宝 Debug Log Started: " << QDateTime::currentDateTime().toString() << " ===" << Qt::endl;
        qDebug() << "[INFO] Log file created:" << logPath;
    } else {
        qWarning() << "[ERROR] Failed to create log file:" << logPath;
        delete g_logFile;
        g_logFile = nullptr;
    }
}

void writeLog(const QString& msg) {
    QMutexLocker locker(&g_logMutex);
    initLog();
    if (g_logFile && g_logFile->isOpen()) {
        QTextStream out(g_logFile);
        out << "[" << QDateTime::currentDateTime().toString("HH:mm:ss.zzz") << "] " << msg << Qt::endl;
        g_logFile->flush();
    }
}

CefHandler::CefHandler(QObject* parent)
    : QObject(parent)
    , m_isClosing(false) {
}

CefHandler::~CefHandler() {
}

bool CefHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefProcessId source_process,
                                          CefRefPtr<CefProcessMessage> message) {
    const std::string& name = message->GetName();

    if (name == "N_JsResult") {
        CefRefPtr<CefListValue> args = message->GetArgumentList();
        if (args->GetSize() > 0) {
            QString result = QString::fromStdString(args->GetString(0).ToString());
            emit jsResultReceived(result);
        }
        return true;
    }

    return false;
}

bool CefHandler::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int popup_id,
                               const CefString& target_url,
                               const CefString& target_frame_name,
                               WindowOpenDisposition target_disposition,
                               bool user_gesture,
                               const CefPopupFeatures& popupFeatures,
                               CefWindowInfo& windowInfo,
                               CefRefPtr<CefClient>& client,
                               CefBrowserSettings& settings,
                               CefRefPtr<CefDictionaryValue>& extra_info,
                               bool* no_javascript_access) {
    std::string url = target_url.ToString();

    // Allow Google OAuth login popups
    if (url.find("accounts.google.com") != std::string::npos ||
        url.find("google.com/signin") != std::string::npos ||
        url.find("google.com/o/oauth") != std::string::npos ||
        url.find("accounts.youtube.com") != std::string::npos) {
        qDebug() << "[INFO] Allow Google login popup:" << QString::fromStdString(url);
        return false;
    }

    // Allow Twitter/X login popups
    if (url.find("twitter.com/i/oauth") != std::string::npos ||
        url.find("x.com/i/oauth") != std::string::npos ||
        url.find("api.twitter.com") != std::string::npos ||
        url.find("api.x.com") != std::string::npos) {
        qDebug() << "[INFO] Allow Twitter login popup:" << QString::fromStdString(url);
        return false;
    }

    // Block other popups
    qDebug() << "[INFO] Block popup:" << QString::fromStdString(url);
    return true;
}

void CefHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    m_browserList.push_back(browser);

    // Immediately embed browser window
    HWND browserHwnd = browser->GetHost()->GetWindowHandle();

    if (browserHwnd && m_parentHwnd) {
        // Hide window first to prevent flash
        ShowWindow(browserHwnd, SW_HIDE);

        // Remove from taskbar
        LONG exStyle = GetWindowLong(browserHwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_APPWINDOW | WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE);
        exStyle |= WS_EX_NOACTIVATE;
        SetWindowLong(browserHwnd, GWL_EXSTYLE, exStyle);

        // Set as child window style
        LONG style = GetWindowLong(browserHwnd, GWL_STYLE);
        style &= ~(WS_POPUP | WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_THICKFRAME |
                   WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_BORDER);
        style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        SetWindowLong(browserHwnd, GWL_STYLE, style);

        // Set parent window
        SetParent(browserHwnd, m_parentHwnd);

        // Get parent client rect for sizing
        RECT parentRect;
        GetClientRect(m_parentHwnd, &parentRect);

        // Position and size to fill parent
        SetWindowPos(browserHwnd, HWND_TOP, 0, 0,
                     parentRect.right - parentRect.left,
                     parentRect.bottom - parentRect.top,
                     SWP_FRAMECHANGED | SWP_NOACTIVATE);

        // Show the embedded window
        ShowWindow(browserHwnd, SW_SHOW);
    }

    m_browser = browser;
    emit browserCreated();
}

bool CefHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (m_browserList.size() == 1) {
        m_isClosing = true;
    }

    return false;
}

void CefHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    for (auto it = m_browserList.begin(); it != m_browserList.end(); ++it) {
        if ((*it)->IsSame(browser)) {
            m_browserList.erase(it);
            break;
        }
    }

    if (m_browser && m_browser->IsSame(browser)) {
        m_browser = m_browserList.empty() ? nullptr : m_browserList.back();
    }

    if (m_browserList.empty()) {
        m_browser = nullptr;
        emit browserClosed();
    }
}

void CefHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             TransitionType transition_type) {
    CEF_REQUIRE_UI_THREAD();

    if (frame->IsMain()) {
        emit loadStarted();
    }
}

void CefHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int httpStatusCode) {
    CEF_REQUIRE_UI_THREAD();

    if (frame->IsMain()) {
        emit loadFinished(httpStatusCode >= 200 && httpStatusCode < 400);
    }
}

void CefHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             ErrorCode errorCode,
                             const CefString& errorText,
                             const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();

    if (errorCode == ERR_ABORTED) {
        return;
    }

    if (frame->IsMain()) {
        qWarning() << "[ERROR] Load failed:" << QString::fromStdString(errorText.ToString())
                   << "URL:" << QString::fromStdString(failedUrl.ToString());
        emit loadFinished(false);
    }
}

void CefHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                               const CefString& title) {
    CEF_REQUIRE_UI_THREAD();
    emit titleChanged(QString::fromStdString(title.ToString()));
}

void CefHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 const CefString& url) {
    CEF_REQUIRE_UI_THREAD();

    if (frame->IsMain()) {
        emit urlChanged(QString::fromStdString(url.ToString()));
    }
}

void CefHandler::ExecuteJavaScript(const QString& code) {
    if (m_browser && m_browser->GetMainFrame()) {
        m_browser->GetMainFrame()->ExecuteJavaScript(
            code.toStdString(), "", 0);
    }
}

bool CefHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                   cef_log_severity_t level,
                                   const CefString& message,
                                   const CefString& source,
                                   int line) {
    QString msg = QString::fromStdString(message.ToString());

    // Log for debugging
    if (msg.startsWith("[XFOLLOW]") || msg.startsWith("[DEBUG]")) {
        writeLog(msg);
        qDebug() << msg;
    }

    // X互关宝: 检测新帖子
    if (msg.startsWith("XFOLLOWING_NEW_POSTS:")) {
        QString jsonData = msg.mid(21);
        writeLog("[NEW_POSTS] " + jsonData.left(500));
        emit newPostsFound(jsonData);
    }
    // X互关宝: 关注成功
    else if (msg.startsWith("XFOLLOWING_FOLLOW_SUCCESS:")) {
        QString userHandle = msg.mid(26);
        writeLog("[FOLLOW_SUCCESS] " + userHandle);
        emit followSuccess(userHandle);
    }
    // X互关宝: 已关注
    else if (msg.startsWith("XFOLLOWING_ALREADY_FOLLOWING:")) {
        QString userHandle = msg.mid(29);
        writeLog("[ALREADY_FOLLOWING] " + userHandle);
        emit alreadyFollowing(userHandle);
    }
    // X互关宝: 关注失败
    else if (msg.startsWith("XFOLLOWING_FOLLOW_FAILED:")) {
        QString userHandle = msg.mid(25);
        writeLog("[FOLLOW_FAILED] " + userHandle);
        emit followFailed(userHandle);
    }
    // X互关宝: 账号被封禁
    else if (msg.startsWith("XFOLLOWING_ACCOUNT_SUSPENDED:")) {
        QString userHandle = msg.mid(29);
        writeLog("[ACCOUNT_SUSPENDED] " + userHandle);
        emit accountSuspended(userHandle);
    }
    // X互关宝: 回关检查 - 对方回关了
    else if (msg.startsWith("XFOLLOWING_CHECK_FOLLOWS_BACK:")) {
        QString userHandle = msg.mid(30);
        writeLog("[CHECK_FOLLOWS_BACK] " + userHandle);
        emit checkFollowsBack(userHandle);
    }
    // X互关宝: 回关检查 - 对方没有回关
    else if (msg.startsWith("XFOLLOWING_CHECK_NOT_FOLLOW_BACK:")) {
        QString userHandle = msg.mid(33);
        writeLog("[CHECK_NOT_FOLLOW_BACK] " + userHandle);
        emit checkNotFollowBack(userHandle);
    }
    // X互关宝: 回关检查 - 账号被封禁
    else if (msg.startsWith("XFOLLOWING_CHECK_SUSPENDED:")) {
        QString userHandle = msg.mid(27);
        writeLog("[CHECK_SUSPENDED] " + userHandle);
        emit checkSuspended(userHandle);
    }
    // X互关宝: 回关检查 - 我没有关注对方
    else if (msg.startsWith("XFOLLOWING_CHECK_NOT_FOLLOWING:")) {
        QString userHandle = msg.mid(31);
        writeLog("[CHECK_NOT_FOLLOWING] " + userHandle);
        emit checkNotFollowing(userHandle);
    }
    // X互关宝: 取消关注成功
    else if (msg.startsWith("XFOLLOWING_UNFOLLOW_SUCCESS:")) {
        QString userHandle = msg.mid(28);
        writeLog("[UNFOLLOW_SUCCESS] " + userHandle);
        emit unfollowSuccess(userHandle);
    }
    // X互关宝: 取消关注失败
    else if (msg.startsWith("XFOLLOWING_UNFOLLOW_FAILED:")) {
        QString userHandle = msg.mid(27);
        writeLog("[UNFOLLOW_FAILED] " + userHandle);
        emit unfollowFailed(userHandle);
    }
    // X互关宝: 粉丝采集
    else if (msg.startsWith("XFOLLOWING_NEW_FOLLOWERS:")) {
        QString jsonData = msg.mid(25);
        writeLog("[NEW_FOLLOWERS] " + jsonData.left(500));
        emit newFollowersFound(jsonData);
    }
    // X互关宝: 用户已登录
    else if (msg.startsWith("XFOLLOWING_USER_LOGGED_IN")) {
        writeLog("[USER_LOGGED_IN]");
        emit userLoggedIn();
    }
    // X互关宝: 回关探测
    else if (msg.startsWith("XFOLLOWING_FOLLOWBACK_DETECTED:")) {
        QString jsonData = msg.mid(31);
        writeLog("[FOLLOWBACK_DETECTED] " + jsonData.left(500));
        emit followBackDetected(jsonData);
    }
    // Check if this is a JS result message
    else if (msg.startsWith("[JSRESULT]")) {
        QString result = msg.mid(10).trimmed();
        writeLog("[JSRESULT] " + result.left(500) + "...");
        emit jsResultReceived(result);
    }

    return false;
}

void CefHandler::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefContextMenuParams> params,
                                      CefRefPtr<CefMenuModel> model) {
    // Only disable context menu in main page, allow in devtools
    std::string url = frame->GetURL().ToString();
    if (url.find("devtools://") == std::string::npos) {
        model->Clear();
    }
}

bool CefHandler::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                const CefKeyEvent& event,
                                CefEventHandle os_event,
                                bool* is_keyboard_shortcut) {
    // Block browser shortcuts to make it look like a native app
    if (event.type == KEYEVENT_RAWKEYDOWN || event.type == KEYEVENT_KEYDOWN) {
        bool ctrl = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
        bool alt = (event.modifiers & EVENTFLAG_ALT_DOWN) != 0;
        bool shift = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;

        // Block F keys (except F12 for debugging)
        if (event.windows_key_code >= 0x70 && event.windows_key_code <= 0x7B) {
            if (event.windows_key_code == 0x74) return true;  // F5 refresh
        }

        // Block Ctrl+key shortcuts
        if (ctrl && !alt) {
            switch (event.windows_key_code) {
                case 'R':  // Ctrl+R refresh
                case 'F':  // Ctrl+F find
                case 'G':  // Ctrl+G find next
                case 'P':  // Ctrl+P print
                case 'S':  // Ctrl+S save
                case 'U':  // Ctrl+U view source
                case 'O':  // Ctrl+O open file
                case 'L':  // Ctrl+L address bar
                case 'T':  // Ctrl+T new tab
                case 'N':  // Ctrl+N new window
                case 'W':  // Ctrl+W close tab
                case 'H':  // Ctrl+H history
                case 'J':  // Ctrl+J downloads
                case 'D':  // Ctrl+D bookmark
                case 'E':  // Ctrl+E search
                case 'K':  // Ctrl+K search
                    return true;
                case 'I':  // Ctrl+Shift+I dev tools
                    if (shift) return true;
                    break;
            }
        }

        // Block Alt+key shortcuts
        if (alt && !ctrl) {
            switch (event.windows_key_code) {
                case VK_LEFT:
                case VK_RIGHT:
                case 'D':
                case VK_HOME:
                    return true;
            }
        }
    }

    return false;
}
