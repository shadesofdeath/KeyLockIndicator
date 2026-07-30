// SettingsDialog.h — WinDark ile temalandırılmış modeless ayar penceresi (spec §4.6, §9).
//
// Her değişiklik anında onApply ile App'e bildirilir; "Uygula" butonu yoktur,
// yeniden başlatma gerekmez.
#pragma once

#include "Settings.h"
#include "Theme.h"

#include <windows.h>

#include <functional>

namespace kli {

class SettingsDialog {
public:
    using ApplyCallback = std::function<void(const Settings&)>;

    SettingsDialog() = default;
    SettingsDialog(const SettingsDialog&) = delete;
    SettingsDialog& operator=(const SettingsDialog&) = delete;
    ~SettingsDialog();

    // Modeless. Zaten açıksa öne getirir ve değerleri yeniler.
    void Show(HINSTANCE hInstance, HWND owner, const Settings& current,
              ApplyCallback onApply);
    void Close();

    [[nodiscard]] bool IsOpen() const noexcept { return m_hwnd != nullptr; }
    [[nodiscard]] HWND Handle() const noexcept { return m_hwnd; }

    // Sistem teması değiştiğinde WinDark'a bildirilir ve kontroller yenilenir.
    void OnThemeChanged(AppTheme theme);

    // Dışarıdan (tray menüsü) yapılan değişikliği kontrollere yansıtır.
    void SyncFrom(const Settings& s);

private:
    static INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
    INT_PTR HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void ApplyLocalizedText();
    void LoadControls();
    void PushFromControls();
    void UpdateValueLabels();
    void Emit();

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    Settings m_settings{};
    ApplyCallback m_onApply;
    AppTheme m_theme = AppTheme::Dark;
    bool m_loading = false;
};

}  // namespace kli
