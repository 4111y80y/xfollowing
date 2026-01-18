#include "CefApp.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include <windows.h>
#include <Shlwapi.h>
#include <string>
#include <cwctype>

BrowserApp::BrowserApp() {
}

void BrowserApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
    // Disable GPU to avoid crashes on some systems
    command_line->AppendSwitch("disable-gpu");
    command_line->AppendSwitch("disable-gpu-compositing");
    command_line->AppendSwitch("disable-software-rasterizer");

    // Disable sandbox for easier debugging
    command_line->AppendSwitch("no-sandbox");

    // Allow running insecure content (for local testing)
    command_line->AppendSwitch("allow-running-insecure-content");

    // Ignore certificate errors (for testing only)
    command_line->AppendSwitch("ignore-certificate-errors");

    // Set language to Simplified Chinese
    command_line->AppendSwitchWithValue("lang", "zh-CN");
    command_line->AppendSwitchWithValue("accept-lang", "zh-CN,zh");

    // Disable GCM to avoid noisy quota errors
    command_line->AppendSwitch("disable-gcm");
    command_line->AppendSwitch("disable-gcm-service");

    // Disable session restore to avoid popup dialogs
    command_line->AppendSwitch("disable-restore-session-state");
    command_line->AppendSwitch("disable-session-crashed-bubble");
    command_line->AppendSwitch("disable-infobars");
    command_line->AppendSwitchWithValue("restore-last-session", "false");

    // Additional switches to disable session/crash recovery
    command_line->AppendSwitch("disable-breakpad");
    command_line->AppendSwitch("disable-crash-reporter");
    command_line->AppendSwitch("disable-component-update");
    command_line->AppendSwitch("disable-domain-reliability");
    command_line->AppendSwitch("disable-features=TranslateUI,SessionRestore,TabHoverCards");

    // Prevent subprocess windows from appearing
    command_line->AppendSwitch("disable-extensions");

    // Prevent Chrome from showing its own window
    command_line->AppendSwitch("disable-background-networking");
    command_line->AppendSwitch("disable-default-apps");
    command_line->AppendSwitch("disable-hang-monitor");
    command_line->AppendSwitch("disable-prompt-on-repost");
    command_line->AppendSwitch("disable-sync");
    command_line->AppendSwitch("disable-translate");
    command_line->AppendSwitch("metrics-recording-only");
    command_line->AppendSwitch("no-first-run");
    command_line->AppendSwitch("safebrowsing-disable-auto-update");

    // Anti-detection: disable automation flags
    command_line->AppendSwitch("disable-blink-features=AutomationControlled");
}

void BrowserApp::OnContextInitialized() {
    CEF_REQUIRE_UI_THREAD();
    // Browser context initialized
}

namespace CefHelper {

bool Initialize(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Main args - Windows uses HINSTANCE
    CefMainArgs main_args(GetModuleHandle(nullptr));

    // Create browser app (needed for both main and subprocess)
    CefRefPtr<BrowserApp> app(new BrowserApp);

    // Execute subprocess if this is a subprocess
    int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
    if (exit_code >= 0) {
        // This is a subprocess, return the exit code
        return false;
    }

    // CEF settings
    CefSettings settings;
    settings.multi_threaded_message_loop = false;
    settings.external_message_pump = true;
    settings.no_sandbox = true;

    // Enable logging for debugging
    settings.log_severity = LOGSEVERITY_WARNING;

    // Set locale
    CefString(&settings.locale).FromASCII("zh-CN");

    // Disable GPU at settings level
    settings.windowless_rendering_enabled = false;

    // Use independent userdata directory for xfollowing
    std::wstring userDataDir = L"D:\\5118\\xfollowing\\userdata";
    CefString(&settings.root_cache_path).FromWString(userDataDir);

    // Persist cookies and session data (for keeping x.com login)
    settings.persist_session_cookies = true;

    // Initialize CEF
    return CefInitialize(main_args, settings, app.get(), nullptr);
}

void Shutdown() {
    CefShutdown();
}

void DoMessageLoopWork() {
    CefDoMessageLoopWork();
}

} // namespace CefHelper
