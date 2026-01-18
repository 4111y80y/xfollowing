#ifndef CEFAPP_H
#define CEFAPP_H

#include "include/cef_app.h"
#include "include/cef_browser.h"

// CefApp implementation for browser process
class BrowserApp : public CefApp, public CefBrowserProcessHandler {
public:
    BrowserApp();

    // CefApp methods
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    // Override to add command line switches
    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override;

    // CefBrowserProcessHandler methods
    void OnContextInitialized() override;

private:
    IMPLEMENT_REFCOUNTING(BrowserApp);
    DISALLOW_COPY_AND_ASSIGN(BrowserApp);
};

// Helper functions for CEF initialization
namespace CefHelper {
    bool Initialize(int argc, char* argv[]);
    void Shutdown();
    void DoMessageLoopWork();
}

#endif // CEFAPP_H
