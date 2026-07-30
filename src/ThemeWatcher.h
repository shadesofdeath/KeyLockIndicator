// ThemeWatcher.h — Açık/koyu tema takibi (spec §4.2).
//
// Tespit iki yoldan birlikte yapılır; WM_SETTINGCHANGE bazı Windows
// sürümlerinde tetiklenmediği için RegNotifyChangeKeyValue thread'i de kurulur.
#pragma once

#include "Theme.h"
#include "Util.h"

#include <windows.h>

#include <atomic>
#include <functional>
#include <thread>

namespace kli {

class ThemeWatcher {
public:
    using ThemeCallback = std::function<void(AppTheme)>;

    ThemeWatcher() = default;
    ThemeWatcher(const ThemeWatcher&) = delete;
    ThemeWatcher& operator=(const ThemeWatcher&) = delete;
    ~ThemeWatcher();

    void Start(HWND hostWnd, ThemeCallback cb);
    void Stop();

    [[nodiscard]] AppTheme Current() const noexcept { return m_theme; }

    // Host pencere WM_SETTINGCHANGE + lParam == L"ImmersiveColorSet" aldığında.
    void OnSettingChange(LPCWSTR area);

    // Host pencere WM_KLI_THEMENOTIFY aldığında (RegNotify thread'i gönderir).
    void OnRegistryNotify();

    // HKCU\...\Themes\Personalize → AppsUseLightTheme (DWORD, 0 = koyu)
    [[nodiscard]] static AppTheme ReadSystemTheme() noexcept;

private:
    void Reevaluate();
    void WatchThread();

    HWND m_host = nullptr;
    ThemeCallback m_callback;
    AppTheme m_theme = AppTheme::Dark;
    std::thread m_thread;
    unique_handle m_stopEvent;
    std::atomic<bool> m_running{false};
};

}  // namespace kli
