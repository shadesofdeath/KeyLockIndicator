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

    // Dil değiştiğinde çağrılır: etiketler, açılır liste öğeleri ve bölüm başlığı
    // ayırıcıları yeni dile göre yeniden kurulur. Etiket metinleri yalnızca
    // WM_INITDIALOG'da konduğu için bu çağrı olmadan açık pencere eski dilde kalır
    // ve "değişiklikler anında uygulanır" sözü tutulmaz. Seçimler korunur.
    void OnLanguageChanged();

    // Dışarıdan (tray menüsü) yapılan değişikliği kontrollere yansıtır.
    void SyncFrom(const Settings& s);

    // Konum seçme kipi boyunca alt satırdaki ipucu sürükleme yönergesine döner ve
    // "Ekrandan seç" butonu pasifleşir; kip bitince ikisi de eski hâline gelir.
    void SetPickingHint(bool picking);

    // RegisterHotKey başarısız olduğunda (kısayol başkasında) görünür uyarı;
    // conflict=false uyarıyı temizler. Kararın sahibi App'tir, kayıt orada yapılır.
    void SetHotkeyWarning(bool conflict);

private:
    static INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
    INT_PTR HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void ApplyLocalizedText();
    void LoadControls();
    void PushFromControls();
    void UpdateValueLabels();
    void Emit();

    // Aşağıdaki beşi SettingsExclude.cpp içindedir (400 satır sınırı, spec §9):
    // istisna listesi (madde 18), içe/dışa aktarma (madde 32) ve taşınabilir kip
    // göstergesi (madde 25). Üçü de dosya seçici / liste kutusu ile çalıştığı
    // için aynı yerde toplandı.
    void RefreshExcludeList();
    void OnAddExcluded();
    void OnRemoveExcluded();
    void OnExportSettings();
    void OnImportSettings();
    void UpdateStorageLabel();

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    Settings m_settings{};
    ApplyCallback m_onApply;
    AppTheme m_theme = AppTheme::Dark;
    bool m_loading = false;
};

}  // namespace kli
