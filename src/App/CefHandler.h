#ifndef CEFHANDLER_H
#define CEFHANDLER_H

#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_context_menu_handler.h"
#include "include/cef_keyboard_handler.h"
#include <QObject>
#include <QString>
#include <list>
#include <functional>
#include <windows.h>

class CefHandler : public QObject,
                   public CefClient,
                   public CefLifeSpanHandler,
                   public CefLoadHandler,
                   public CefDisplayHandler,
                   public CefContextMenuHandler,
                   public CefKeyboardHandler {
    Q_OBJECT

public:
    explicit CefHandler(QObject* parent = nullptr);
    ~CefHandler();

    // CefClient methods
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override;

    // CefLifeSpanHandler methods
    bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
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
                       bool* no_javascript_access) override;
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler methods
    void OnLoadStart(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     TransitionType transition_type) override;
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;
    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString& errorText,
                     const CefString& failedUrl) override;

    // CefDisplayHandler methods
    void OnTitleChange(CefRefPtr<CefBrowser> browser,
                       const CefString& title) override;
    void OnAddressChange(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const CefString& url) override;
    bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                          cef_log_severity_t level,
                          const CefString& message,
                          const CefString& source,
                          int line) override;

    // CefContextMenuHandler methods - disable right-click menu
    void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame,
                             CefRefPtr<CefContextMenuParams> params,
                             CefRefPtr<CefMenuModel> model) override;

    // CefKeyboardHandler methods - block browser shortcuts
    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyEvent& event,
                       CefEventHandle os_event,
                       bool* is_keyboard_shortcut) override;

    // Browser access
    CefRefPtr<CefBrowser> GetBrowser() const { return m_browser; }
    bool IsClosing() const { return m_isClosing; }

    // Execute JavaScript
    void ExecuteJavaScript(const QString& code);

    // Set parent window for embedding
    void SetParentHwnd(HWND hwnd) { m_parentHwnd = hwnd; }

signals:
    void browserCreated();
    void browserClosed();
    void loadStarted();
    void loadFinished(bool success);
    void titleChanged(const QString& title);
    void urlChanged(const QString& url);
    void jsResultReceived(const QString& result);
    void networkLogReceived(const QString& log);

    // X互关宝专用信号
    void newPostsFound(const QString& jsonData);
    void followSuccess(const QString& userHandle);
    void alreadyFollowing(const QString& userHandle);
    void followFailed(const QString& userHandle);

private:
    CefRefPtr<CefBrowser> m_browser;
    std::list<CefRefPtr<CefBrowser>> m_browserList;
    bool m_isClosing;
    HWND m_parentHwnd = nullptr;

    IMPLEMENT_REFCOUNTING(CefHandler);
    DISALLOW_COPY_AND_ASSIGN(CefHandler);
};

#endif // CEFHANDLER_H
