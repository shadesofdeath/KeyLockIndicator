// App.h — Uygulama koordinatörü + gizli mesaj penceresi (spec §2, §6).
//
// Tüm modüller arası bağlantı burada std::function callback'lerle kurulur.
// Modüller birbirini tanımaz.
#pragma once

#include "Autostart.h"
#include "KeyMonitor.h"
#include "OsdWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ThemeWatcher.h"
#include "TrayIcon.h"
#include "Util.h"

#include <windows.h>

namespace kli {

class App {
public:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    ~App();

    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Shutdown();

    // İkinci örneğin ilk örneği bulup ayarları açtırması için kullanılır.
    [[nodiscard]] static HWND FindExistingInstanceWindow();

private:
    static LRESULT CALLBACK HostWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateHostWindow();

    // Callback'ler
    void OnLockChanged(LockKey changed, LockState now);
    void OnSystemThemeChanged(AppTheme theme);
    void OnTrayCommand(int commandId);
    void OnSettingsApplied(const Settings& next);

    void ApplySettingsToModules(bool trayIconMayChange);
    void RefreshTray();
    void ShowSettings();
    void ShowAbout();
    [[nodiscard]] AppTheme EffectiveTheme() const;
    [[nodiscard]] OsdConfig BuildOsdConfig() const;

    HINSTANCE m_instance = nullptr;
    HWND m_host = nullptr;
    UINT m_taskbarCreatedMsg = 0;
    UINT m_showSettingsMsg = 0;
    bool m_sessionNotifyRegistered = false;

    Settings m_settings{};
    KeyMonitor m_keyMonitor;
    ThemeWatcher m_themeWatcher;
    OsdWindow m_osd;
    TrayIcon m_tray;
    SettingsDialog m_settingsDialog;
};

}  // namespace kli
