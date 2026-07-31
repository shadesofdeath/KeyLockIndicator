// AboutDialog.h — "Hakkında" penceresi (IDD_ABOUT).
//
// MessageBox yerine kendi diyaloğumuz: MessageBox koyu temaya uymaz (başlık
// çubuğu ve zemini her zaman açık kalır) ve uygulama simgesini yalnızca 32
// piksel olarak gösterir. Bu pencere WinDark ile temalandırılır ve simgeyi
// DPI'ya göre 48 DIP yükler.
//
// MODAL (DialogBoxParamW): App::ShowAbout kapanana kadar dönmez. Tepsi ikonu,
// tuş yoklaması ve OSD çalışmayı sürdürür — modal döngü de aynı iş parçacığının
// mesajlarını dağıtır.
#pragma once

#include "Theme.h"
#include "Util.h"

#include <windows.h>

namespace kli {

class AboutDialog {
public:
    AboutDialog() = default;
    AboutDialog(const AboutDialog&) = delete;
    AboutDialog& operator=(const AboutDialog&) = delete;

    // owner: görünürse pencere onun üzerinde ortalanır, aksi hâlde aktif monitörde.
    void Show(HINSTANCE hInstance, HWND owner, AppTheme theme);

    // Modal döngü sürerken sistem teması değişirse çağrılır; pencere kapalıysa
    // yalnızca sonraki açılış için tema hatırlanır.
    void OnThemeChanged(AppTheme theme);

    [[nodiscard]] bool IsOpen() const noexcept { return m_hwnd != nullptr; }

private:
    static INT_PTR CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
    INT_PTR HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnInitDialog(HWND owner);
    void ApplyLocalizedText();
    void LoadAppIcon(UINT dpi);
    void CreateNameFont(UINT dpi);
    void ApplyTheme();

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    AppTheme m_theme = AppTheme::Dark;

    // Diyalog ömrüne bağlı GDI kaynakları; WM_DESTROY'da bırakılır.
    unique_hicon m_icon;
    unique_hfont m_nameFont;
};

}  // namespace kli
