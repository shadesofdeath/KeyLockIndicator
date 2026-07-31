// AboutDialog.cpp — IDD_ABOUT'un çalışma zamanı: simge, büyük ad yazı tipi,
// yerelleştirme, koyu/açık tema.
#include "AboutDialog.h"

#include "Localization.h"
#include "MonitorUtil.h"
#include "resource.h"

#include <WinDark/WinDark.h>

#include <commctrl.h>   // LoadIconWithScaleDown (comctl32)

#include <string>

namespace kli {
namespace {

// Sürüm numarası. res/app.rc içindeki VS_VERSION_INFO ile aynı olmalıdır; iki
// yer de aynı dosya ailesinde olduğu için ayrışma gözden kaçmaz. Çeviri
// tablolarına gömülmez (IDS_ABOUT_VERSION yalnızca "%s" biçimini taşır).
constexpr wchar_t kVersionText[] = L"1.0.0";

// Uygulama simgesinin DIP boyutu ve uygulama adının punto/kalınlığı.
constexpr int kIconSizeDip = 48;
constexpr int kNamePointSize = 14;

// Sürüm ve yazar satırları soluk çizilir; ad ve açıklama normal metin rengiyle.
[[nodiscard]] bool IsDimText(int ctrlId) noexcept {
    return ctrlId == IDC_ABOUT_VERSION || ctrlId == IDC_ABOUT_AUTHOR;
}

[[nodiscard]] UINT DialogDpi(HWND dlg) noexcept {
    const UINT dpi = ::GetDpiForWindow(dlg);
    return (dpi != 0) ? dpi : kDefaultDpi;
}

// Simge statiğini tam kareye oturtur. DLU'nun yatay ve dikey taban birimleri
// farklıdır, bu yüzden şablonda kare bir kontrol tanımlamak mümkün değildir;
// SS_REALSIZECONTROL de simgeyi kontrolün dikdörtgenine gerdiği için kare
// olmayan bir kontrol simgeyi ezerdi.
void SquareIconControl(HWND dlg, int sizePx) {
    const HWND ctrl = ::GetDlgItem(dlg, IDC_ABOUT_ICON);
    if (ctrl == nullptr || sizePx <= 0) {
        return;
    }
    ::SetWindowPos(ctrl, nullptr, 0, 0, sizePx, sizePx,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Sahibin üzerinde ortalar; sahip görünmüyorsa (App'in 0x0 host penceresi)
// imlecin bulunduğu monitörün çalışma alanında ortalar.
void CenterOnOwnerOrMonitor(HWND dlg, HWND owner) {
    RECT self{};
    if (!::GetWindowRect(dlg, &self)) {
        return;
    }
    const int w = self.right - self.left;
    const int h = self.bottom - self.top;

    RECT area{};
    bool haveOwner = false;
    if (owner != nullptr && ::IsWindowVisible(owner) != FALSE &&
        ::GetWindowRect(owner, &area) != FALSE) {
        haveOwner = (area.right > area.left) && (area.bottom > area.top);
    }
    if (!haveOwner) {
        area = MonitorUtil::Active(false).work;
    }

    int x = area.left + ((area.right - area.left) - w) / 2;
    int y = area.top + ((area.bottom - area.top) - h) / 2;

    // Ortalanan pencere ekran dışına taşmasın (küçük sahip pencere, uç DPI).
    const RECT work = MonitorUtil::Active(false).work;
    if (x < work.left) { x = work.left; }
    if (y < work.top) { y = work.top; }
    if (x + w > work.right) { x = work.right - w; }
    if (y + h > work.bottom) { y = work.bottom - h; }

    ::SetWindowPos(dlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace

// ---------------------------------------------------------------------------
// Yaşam döngüsü
// ---------------------------------------------------------------------------

void AboutDialog::Show(HINSTANCE hInstance, HWND owner, AppTheme theme) {
    if (m_hwnd != nullptr) {
        // Modal olduğu için normalde imkânsız; yine de ikinci pencere açılmasın.
        ::SetForegroundWindow(m_hwnd);
        return;
    }
    m_instance = hInstance;
    m_theme = theme;

    // Dönüş değeri yalnızca hata ayırt etmek için: -1 pencere kurulamadı demektir.
    const INT_PTR result = ::DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_ABOUT), owner,
                                             DlgProc, reinterpret_cast<LPARAM>(this));
    if (result == -1) {
        LogV(L"DialogBoxParamW(IDD_ABOUT) başarısız: %lu", ::GetLastError());
    }
}

void AboutDialog::OnThemeChanged(AppTheme theme) {
    m_theme = theme;
    if (m_hwnd == nullptr) {
        return;
    }
    ApplyTheme();
    WinDark::RefreshWindow(m_hwnd);
}

INT_PTR CALLBACK AboutDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INITDIALOG) {
        ::SetWindowLongPtrW(hwnd, DWLP_USER, static_cast<LONG_PTR>(lParam));
    }
    auto* self = reinterpret_cast<AboutDialog*>(::GetWindowLongPtrW(hwnd, DWLP_USER));
    if (self == nullptr) {
        return FALSE;
    }
    if (msg == WM_INITDIALOG) {
        self->m_hwnd = hwnd;
    }
    return self->HandleMessage(msg, wParam, lParam);
}

INT_PTR AboutDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG:
            OnInitDialog(::GetWindow(m_hwnd, GW_OWNER));
            return TRUE;  // odak varsayılan butonda kalır

        case WM_COMMAND: {
            const int id = static_cast<int>(LOWORD(wParam));
            // Enter varsayılan butonun kimliğini, Esc IDCANCEL'ı gönderir.
            if (id == IDC_ABOUT_OK || id == IDOK || id == IDCANCEL) {
                ::EndDialog(m_hwnd, id);
                return TRUE;
            }
            break;
        }

        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            const bool dark = (m_theme == AppTheme::Dark);
            const HBRUSH brush = WinDark::OnCtlColor(msg, wParam, lParam, dark);
            if (msg == WM_CTLCOLORSTATIC) {
                const auto hdc = reinterpret_cast<HDC>(wParam);
                const int ctrlId = ::GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
                if (hdc != nullptr && IsDimText(ctrlId)) {
                    // Zemin diyalog fırçasıyla dolduruluyor; metnin arkasına blok
                    // çizilmesin, yoksa soluk renk bir kutu içinde kalır.
                    ::SetBkMode(hdc, TRANSPARENT);
                    ::SetTextColor(hdc, WinDark::DisabledTextColor(dark));
                    // Açık temada WinDark fırça vermez; nullptr dönersek DefDlgProc
                    // kendi rengini yazar ve soluklaştırma kaybolur.
                    return reinterpret_cast<INT_PTR>(
                        (brush != nullptr) ? brush : ::GetSysColorBrush(COLOR_3DFACE));
                }
            }
            if (brush != nullptr) {
                return reinterpret_cast<INT_PTR>(brush);
            }
            break;
        }

        case WM_CLOSE:
            ::EndDialog(m_hwnd, IDCANCEL);
            return TRUE;

        case WM_DESTROY:
            // Simge ve yazı tipi kontrollerden önce bırakılmaz: kontroller bu
            // noktada zaten yıkıldı, dolayısıyla kimse onlara başvurmuyor.
            m_icon.reset();
            m_nameFont.reset();
            m_hwnd = nullptr;
            break;

        default:
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Kurulum
// ---------------------------------------------------------------------------

void AboutDialog::OnInitDialog(HWND owner) {
    const UINT dpi = DialogDpi(m_hwnd);
    ApplyLocalizedText();
    LoadAppIcon(dpi);
    CreateNameFont(dpi);
    ApplyTheme();
    CenterOnOwnerOrMonitor(m_hwnd, owner);
}

void AboutDialog::ApplyLocalizedText() {
    ::SetWindowTextW(m_hwnd, Loc::Str(IDS_ABOUT_TITLE).c_str());
    ::SetDlgItemTextW(m_hwnd, IDC_ABOUT_NAME, Loc::Str(IDS_APPNAME).c_str());
    ::SetDlgItemTextW(m_hwnd, IDC_ABOUT_DESC, Loc::Str(IDS_ABOUT_DESC).c_str());
    ::SetDlgItemTextW(m_hwnd, IDC_ABOUT_AUTHOR, Loc::Str(IDS_ABOUT_AUTHOR).c_str());
    ::SetDlgItemTextW(m_hwnd, IDC_ABOUT_OK, Loc::Str(IDS_ABOUT_OK).c_str());

    // IDS_ABOUT_VERSION bir kalıptır ("Sürüm %s"). Yerine koyma elle yapılır:
    // printf ailesine kaynaktan gelen (literal olmayan) bir biçim dizesi vermek
    // /W4'te C4774 üretir ve dize bozuksa davranış tanımsızlaşır.
    std::wstring version = Loc::Str(IDS_ABOUT_VERSION);
    const size_t slot = version.find(L"%s");
    if (slot != std::wstring::npos) {
        version.replace(slot, 2, kVersionText);
    } else {
        // Çeviride belirteç yok: sürüm yine de görünsün.
        version += L' ';
        version += kVersionText;
    }
    ::SetDlgItemTextW(m_hwnd, IDC_ABOUT_VERSION, version.c_str());
}

void AboutDialog::LoadAppIcon(UINT dpi) {
    const int px = ::MulDiv(kIconSizeDip, static_cast<int>(dpi), static_cast<int>(kDefaultDpi));

    // LoadIconWithScaleDown, .ico içindeki bir üst kareyi (256) alıp yüksek
    // kaliteli küçültme yapar; LoadImage en yakın kareyi sertçe gerdiği için
    // 125/150 % ölçeklemede tırtıklı görünür.
    HICON hIcon = nullptr;
    HR_LOG(::LoadIconWithScaleDown(m_instance, MAKEINTRESOURCEW(IDI_APP), px, px, &hIcon));
    if (hIcon == nullptr) {
        hIcon = static_cast<HICON>(::LoadImageW(m_instance, MAKEINTRESOURCEW(IDI_APP),
                                                IMAGE_ICON, px, px, LR_DEFAULTCOLOR));
    }
    if (hIcon == nullptr) {
        LogV(L"Hakkında: uygulama simgesi yüklenemedi (%lu)", ::GetLastError());
        return;
    }
    m_icon.reset(hIcon);

    // Kontrol simgeyle aynı piksel karesine getirilir: SS_REALSIZECONTROL simgeyi
    // kontrolün dikdörtgenine gerer, boyutlar eşitse hiç yeniden örnekleme olmaz.
    SquareIconControl(m_hwnd, px);
    ::SendDlgItemMessageW(m_hwnd, IDC_ABOUT_ICON, STM_SETICON,
                          reinterpret_cast<WPARAM>(m_icon.get()), 0);
}

void AboutDialog::CreateNameFont(UINT dpi) {
    // Negatif yükseklik = karakter yüksekliği (iç boşluk hariç), punto → piksel.
    const int height = -::MulDiv(kNamePointSize, static_cast<int>(dpi), 72);
    const HFONT font = ::CreateFontW(height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_DONTCARE,
                                     L"Segoe UI");
    if (font == nullptr) {
        LogV(L"Hakkında: uygulama adı yazı tipi oluşturulamadı");
        return;  // kontrol diyaloğun kendi yazı tipiyle kalır
    }
    m_nameFont.reset(font);
    // WM_SETFONT sahipliği devretmez; yazı tipi WM_DESTROY'a kadar yaşamalıdır.
    ::SendDlgItemMessageW(m_hwnd, IDC_ABOUT_NAME, WM_SETFONT,
                          reinterpret_cast<WPARAM>(m_nameFont.get()), TRUE);
}

void AboutDialog::ApplyTheme() {
    // Proses genelindeki app modu App'in sorumluluğunda; burada yalnızca bu
    // pencere ve kontrolleri temalandırılır (başlık çubuğu dahil).
    const bool dark = (m_theme == AppTheme::Dark);
    WinDark::ApplyToWindow(m_hwnd, dark);
    WinDark::ApplyToChildren(m_hwnd, dark);
}

}  // namespace kli
