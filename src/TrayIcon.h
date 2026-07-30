// TrayIcon.h — Shell_NotifyIcon + bağlam menüsü (spec §4.5).
#pragma once

#include "LockTypes.h"
#include "Theme.h"
#include "Util.h"

#include <windows.h>
#include <shellapi.h>

#include <functional>

namespace kli {

class TrayIcon {
public:
    using CommandCallback = std::function<void(int commandId)>;

    TrayIcon() = default;
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;
    ~TrayIcon();

    bool Add(HINSTANCE hInstance, HWND hostWnd, CommandCallback onCommand);
    void Remove();

    // İkonu (tuş + durum + tema) ve çok satırlı tooltip'i günceller.
    void Update(LockState state, LockKey displayKey, AppTheme theme);

    // Menüdeki onay işaretleri için.
    void SetMenuState(bool osdEnabled, bool autostartEnabled, bool autostartLocked);

    // Host pencereden yönlendirilir (WM_KLI_TRAYICON).
    void OnTrayMessage(WPARAM wParam, LPARAM lParam);

    // RegisterWindowMessage(L"TaskbarCreated") alındığında ikon yeniden eklenir.
    void OnTaskbarCreated();

    [[nodiscard]] static UINT TaskbarCreatedMessage();
    [[nodiscard]] bool Added() const noexcept { return m_added; }

    static constexpr UINT kIconId = 1;

private:
    bool SendAdd();
    void ShowContextMenu();
    void LoadIconFor(LockKey key, bool on, AppTheme theme);
    void BuildTooltip(LockState state, wchar_t* out, size_t cch) const;

    HINSTANCE m_instance = nullptr;
    HWND m_host = nullptr;
    CommandCallback m_onCommand;
    unique_hicon m_icon;
    LockState m_state{};
    LockKey m_displayKey = LockKey::Caps;
    AppTheme m_theme = AppTheme::Dark;
    bool m_added = false;
    bool m_osdEnabled = true;
    bool m_autostartEnabled = false;
    bool m_autostartLocked = false;
    int m_loadedIconIndex = -1;
};

}  // namespace kli
