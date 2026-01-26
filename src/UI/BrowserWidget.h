#ifndef BROWSERWIDGET_H
#define BROWSERWIDGET_H

#include <QWidget>
#include "App/CefHandler.h"
#include "include/cef_browser.h"

class BrowserWidget : public QWidget {
    Q_OBJECT

public:
    explicit BrowserWidget(QWidget* parent = nullptr);
    ~BrowserWidget();

    // Create browser with URL
    void CreateBrowser(const QString& url);
    void CreateBrowserWithProfile(const QString& url, const QString& profilePath);
    void SwitchProfile(const QString& url, const QString& profilePath);

    // Navigate to URL
    void LoadUrl(const QString& url);

    // Execute JavaScript
    void ExecuteJavaScript(const QString& code);
    void executeJavaScript(const QString& code) { ExecuteJavaScript(code); }

    // Get handler
    CefHandler* GetHandler() const { return m_handler; }
    CefHandler* handler() const { return m_handler; }

    // Reload page
    void Reload();

    // Go back/forward
    void GoBack();
    void GoForward();

    // Show DevTools
    void ShowDevTools();
    void CloseDevTools();

    // Close browser
    void CloseBrowser();

    // Disconnect all signals for safe deletion
    void DisconnectAll();

    // Check if browser is fully closed
    bool IsClosed() const { return !m_browserCreated && !m_browserCreating; }

signals:
    void browserCreated();
    void loadStarted();
    void loadFinished(bool success);
    void titleChanged(const QString& title);
    void urlChanged(const QString& url);
    void jsResultReceived(const QString& result);
    void networkLogReceived(const QString& log);

    // X互关宝专用信号转发
    void newPostsFound(const QString& jsonData);
    void followSuccess(const QString& userHandle);
    void alreadyFollowing(const QString& userHandle);
    void followFailed(const QString& userHandle);
    void accountSuspended(const QString& userHandle);
    // 回关检查信号
    void checkFollowsBack(const QString& userHandle);
    void checkNotFollowBack(const QString& userHandle);
    void checkSuspended(const QString& userHandle);
    void checkNotFollowing(const QString& userHandle);
    // 取消关注信号
    void unfollowSuccess(const QString& userHandle);
    void unfollowFailed(const QString& userHandle);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void ResizeBrowser();
    void CreateBrowserInternal(const QString& url);
    void CloseBrowserForSwitch();

    CefHandler* m_handler;
    bool m_browserCreated;
    bool m_browserCreating;
    bool m_recreatePending;
    QString m_profilePath;
    QString m_pendingUrl;
    CefRefPtr<CefRequestContext> m_requestContext;
};

#endif // BROWSERWIDGET_H
