#include "BrowserWidget.h"
#include "include/cef_browser.h"
#include "include/cef_request_context.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_cookie.h"
#include "include/wrapper/cef_helpers.h"
#include "include/internal/cef_types_geometry.h"
#include <QResizeEvent>
#include <QCloseEvent>
#include <QMetaObject>
#include <QPointer>
#include <QDir>
#include <QTimer>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#endif

// External log function from CefHandler.cpp
extern void writeLog(const QString& msg);

namespace {
class FlushCallback : public CefCompletionCallback {
public:
    explicit FlushCallback(QObject* owner, std::function<void()> onDone)
        : owner_(owner)
        , onDone_(std::move(onDone)) {}

    void OnComplete() override {
        if (!owner_) {
            return;
        }
        QMetaObject::invokeMethod(owner_, [onDone = onDone_]() {
            if (onDone) {
                onDone();
            }
        }, Qt::QueuedConnection);
    }

private:
    QPointer<QObject> owner_;
    std::function<void()> onDone_;

    IMPLEMENT_REFCOUNTING(FlushCallback);
};
} // namespace

BrowserWidget::BrowserWidget(QWidget* parent)
    : QWidget(parent)
    , m_handler(nullptr)
    , m_browserCreated(false)
    , m_browserCreating(false)
    , m_recreatePending(false) {

    // Set widget attributes for embedding browser
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);

    // Create handler - NOT as child to avoid Qt auto-deletion while CEF still uses it
    m_handler = new CefHandler(nullptr);

    // Connect signals
    connect(m_handler, &CefHandler::browserCreated, this, [this]() {
        m_browserCreated = true;
        m_browserCreating = false;

        if (m_handler && m_handler->GetBrowser()) {
            m_handler->GetBrowser()->GetHost()->SetZoomLevel(0.0);
            ResizeBrowser();
        }

        emit browserCreated();
    });
    connect(m_handler, &CefHandler::browserClosed, this, [this]() {
        m_browserCreated = false;
        m_browserCreating = false;
        if (m_recreatePending) {
            m_recreatePending = false;
            CreateBrowserInternal(m_pendingUrl);
        }
    });
    connect(m_handler, &CefHandler::loadStarted, this, &BrowserWidget::loadStarted);
    connect(m_handler, &CefHandler::loadFinished, this, &BrowserWidget::loadFinished);
    connect(m_handler, &CefHandler::titleChanged, this, &BrowserWidget::titleChanged);
    connect(m_handler, &CefHandler::urlChanged, this, &BrowserWidget::urlChanged);
    connect(m_handler, &CefHandler::jsResultReceived, this, [this](const QString& result) {
        writeLog(QString("[BROWSERWIDGET] Received JS result, len=%1").arg(result.length()));
        emit jsResultReceived(result);
    });
    connect(m_handler, &CefHandler::networkLogReceived, this, &BrowserWidget::networkLogReceived);

    // X互关宝专用信号转发
    connect(m_handler, &CefHandler::newPostsFound, this, &BrowserWidget::newPostsFound);
    connect(m_handler, &CefHandler::followSuccess, this, &BrowserWidget::followSuccess);
    connect(m_handler, &CefHandler::alreadyFollowing, this, &BrowserWidget::alreadyFollowing);
    connect(m_handler, &CefHandler::followFailed, this, &BrowserWidget::followFailed);
}

BrowserWidget::~BrowserWidget() {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GetHost()->CloseBrowser(true);
    }
    m_handler = nullptr;
}

void BrowserWidget::CreateBrowser(const QString& url) {
    CreateBrowserInternal(url);
}

void BrowserWidget::CreateBrowserWithProfile(const QString& url, const QString& profilePath) {
    m_profilePath = profilePath;
    CreateBrowserInternal(url);
}

void BrowserWidget::SwitchProfile(const QString& url, const QString& profilePath) {
    if (profilePath == m_profilePath && m_handler && m_handler->GetBrowser()) {
        LoadUrl(url);
        return;
    }
    m_profilePath = profilePath;
    if (m_handler && m_handler->GetBrowser()) {
        m_pendingUrl = url;
        m_recreatePending = true;
        if (m_requestContext) {
            CefRefPtr<CefCookieManager> manager = m_requestContext->GetCookieManager(nullptr);
            if (manager) {
                manager->FlushStore(new FlushCallback(this, [this]() {
                    CloseBrowserForSwitch();
                }));
                return;
            }
        }
        CloseBrowserForSwitch();
        return;
    }
    CreateBrowserInternal(url);
}

void BrowserWidget::CreateBrowserInternal(const QString& url) {
    if (m_browserCreated) {
        LoadUrl(url);
        return;
    }

    if (m_browserCreating) {
        return;
    }

    setAttribute(Qt::WA_NativeWindow, true);

    WId hwnd = effectiveWinId();

    if (hwnd == 0 || width() <= 0 || height() <= 0 || !isVisible()) {
        m_pendingUrl = url;
        m_browserCreating = true;
        QTimer::singleShot(100, this, [this, url]() {
            m_browserCreating = false;
            CreateBrowserInternal(url);
        });
        return;
    }

    CefWindowInfo windowInfo;

#ifdef _WIN32
    HWND parentHwnd = (HWND)hwnd;

    LONG style = GetWindowLong(parentHwnd, GWL_STYLE);

    if (!(style & WS_CLIPCHILDREN)) {
        SetWindowLong(parentHwnd, GWL_STYLE, style | WS_CLIPCHILDREN);
    }

    CefRect cefRect(0, 0, width(), height());
    windowInfo.SetAsChild(parentHwnd, cefRect);

    windowInfo.style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
#endif

    CefBrowserSettings browserSettings;
    browserSettings.javascript_access_clipboard = STATE_ENABLED;
    browserSettings.javascript_dom_paste = STATE_ENABLED;

    if (m_profilePath.isEmpty()) {
        m_requestContext = nullptr;
    } else {
        QDir profileDir(m_profilePath);
        if (!profileDir.exists()) {
            profileDir.mkpath(".");
        }

        QString absolutePath = QDir::toNativeSeparators(profileDir.absolutePath());

        CefRequestContextSettings contextSettings;
        CefString(&contextSettings.cache_path).FromWString(absolutePath.toStdWString());
        contextSettings.persist_session_cookies = true;
        m_requestContext = CefRequestContext::CreateContext(contextSettings, nullptr);
    }

    m_browserCreating = true;

    m_handler->SetParentHwnd(parentHwnd);

    CefBrowserHost::CreateBrowser(
        windowInfo,
        m_handler,
        url.toStdString(),
        browserSettings,
        nullptr,
        m_requestContext
    );
}

void BrowserWidget::CloseBrowserForSwitch() {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GetHost()->CloseBrowser(true);
    }
}

void BrowserWidget::CloseBrowser() {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GetHost()->CloseBrowser(true);
    }
    m_browserCreated = false;
    m_browserCreating = false;
}

void BrowserWidget::DisconnectAll() {
    if (m_handler) {
        m_handler->disconnect(this);
    }
    this->disconnect();
}

void BrowserWidget::LoadUrl(const QString& url) {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GetMainFrame()->LoadURL(url.toStdString());
    }
}

void BrowserWidget::ExecuteJavaScript(const QString& code) {
    if (m_handler) {
        m_handler->ExecuteJavaScript(code);
    }
}

void BrowserWidget::Reload() {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->Reload();
    }
}

void BrowserWidget::GoBack() {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GoBack();
    }
}

void BrowserWidget::GoForward() {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GoForward();
    }
}

void BrowserWidget::ShowDevTools() {
    if (m_handler && m_handler->GetBrowser()) {
        CefWindowInfo windowInfo;
#ifdef _WIN32
        windowInfo.SetAsPopup(nullptr, "DevTools");
#endif
        CefBrowserSettings settings;
        m_handler->GetBrowser()->GetHost()->ShowDevTools(windowInfo, nullptr, settings, CefPoint());
    }
}

void BrowserWidget::CloseDevTools() {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GetHost()->CloseDevTools();
    }
}

void BrowserWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ResizeBrowser();
}

void BrowserWidget::closeEvent(QCloseEvent* event) {
    if (m_handler && m_handler->GetBrowser()) {
        m_handler->GetBrowser()->GetHost()->CloseBrowser(true);
    }
    QWidget::closeEvent(event);
}

void BrowserWidget::ResizeBrowser() {
    if (!m_handler || !m_handler->GetBrowser()) {
        return;
    }

#ifdef _WIN32
    HWND browserHwnd = m_handler->GetBrowser()->GetHost()->GetWindowHandle();
    if (browserHwnd) {
        // Get DPI scale factor for this widget
        qreal dpiScale = devicePixelRatioF();

        // Calculate physical pixels from logical pixels
        int physicalWidth = static_cast<int>(width() * dpiScale);
        int physicalHeight = static_cast<int>(height() * dpiScale);

        // Resize browser to fill the widget completely
        SetWindowPos(browserHwnd, nullptr,
                     0, 0,
                     physicalWidth,
                     physicalHeight,
                     SWP_NOZORDER);
    }
#endif
}
