#include "CefApp.h"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"
#include <windows.h>
#include <Shlwapi.h>
#include <string>
#include <cwctype>

// Helper function to get exe directory using Windows API (works before QApplication)
static std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    // Remove filename, keep directory
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
    }
    return std::wstring(path);
}

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

    // Disable GCM to avoid noisy quota errors (DEPRECATED_ENDPOINT)
    command_line->AppendSwitch("disable-gcm");
    command_line->AppendSwitch("disable-gcm-service");
    command_line->AppendSwitchWithValue("gcm-checkin-url", "");
    command_line->AppendSwitchWithValue("gcm-mcs-endpoint", "");
    command_line->AppendSwitchWithValue("gcm-registration-url", "");
    command_line->AppendSwitch("disable-push-api-background-mode");
    command_line->AppendSwitch("disable-notifications");

    // Disable Bluetooth to avoid adapter errors
    command_line->AppendSwitch("disable-bluetooth");

    // Disable device event logging
    command_line->AppendSwitch("disable-logging");
    command_line->AppendSwitch("disable-device-event-log");

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

    // Disable profile error dialogs
    command_line->AppendSwitch("disable-client-side-phishing-detection");
    command_line->AppendSwitch("disable-dinosaur-easter-egg");
    command_line->AppendSwitch("silent-launch");

    // Completely disable profile error/warning dialogs
    command_line->AppendSwitch("disable-profile-error-dialogs");
    command_line->AppendSwitch("noerrdialogs");
    command_line->AppendSwitch("disable-popup-blocking");
    command_line->AppendSwitch("disable-features=ProfileMenuRevamp,ProfilePicker");
    command_line->AppendSwitch("disable-signin-promo");
    command_line->AppendSwitch("no-default-browser-check");
    command_line->AppendSwitch("wm-window-animations-disabled");
    command_line->AppendSwitch("disable-modal-animations");

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

    // Use independent userdata directory for xfollowing (relative to exe)
    std::wstring exeDir = GetExeDirectory();
    std::wstring userDataDir = exeDir + L"\\userdata";
    // Create directory if not exists
    CreateDirectoryW(userDataDir.c_str(), nullptr);

    // Clean up session restore files to prevent popup windows after crash
    std::wstring defaultDir = userDataDir + L"\\Default";  // 注意大写D
    DeleteFileW((userDataDir + L"\\Last Browser").c_str());
    DeleteFileW((defaultDir + L"\\Current Session").c_str());
    DeleteFileW((defaultDir + L"\\Current Tabs").c_str());
    DeleteFileW((defaultDir + L"\\Last Session").c_str());
    DeleteFileW((defaultDir + L"\\Last Tabs").c_str());
    DeleteFileW((defaultDir + L"\\Visited Links").c_str());
    DeleteFileW((defaultDir + L"\\History").c_str());
    DeleteFileW((defaultDir + L"\\History-journal").c_str());

    // Clean up files that cause profile error dialogs (but preserve login state)
    // Note: Do NOT delete Login Data, Preferences, Secure Preferences, Local State
    // as they are required for maintaining login sessions
    DeleteFileW((defaultDir + L"\\Web Data").c_str());
    DeleteFileW((defaultDir + L"\\Web Data-journal").c_str());
    DeleteFileW((defaultDir + L"\\Network Action Predictor").c_str());
    DeleteFileW((defaultDir + L"\\Network Action Predictor-journal").c_str());

    // Delete lock files to prevent multi-instance conflicts
    DeleteFileW((defaultDir + L"\\lockfile").c_str());
    DeleteFileW((defaultDir + L"\\LOCK").c_str());

    // Clean up scanner profile (used by search and followers browsers)
    std::wstring scannerDir = userDataDir + L"\\scanner";
    CreateDirectoryW(scannerDir.c_str(), nullptr);
    std::wstring scannerDefaultDir = scannerDir + L"\\Default";
    DeleteFileW((scannerDir + L"\\Last Browser").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Current Session").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Current Tabs").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Last Session").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Last Tabs").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Visited Links").c_str());
    DeleteFileW((scannerDefaultDir + L"\\History").c_str());
    DeleteFileW((scannerDefaultDir + L"\\History-journal").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Web Data").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Web Data-journal").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Network Action Predictor").c_str());
    DeleteFileW((scannerDefaultDir + L"\\Network Action Predictor-journal").c_str());
    DeleteFileW((scannerDefaultDir + L"\\lockfile").c_str());
    DeleteFileW((scannerDefaultDir + L"\\LOCK").c_str());

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
