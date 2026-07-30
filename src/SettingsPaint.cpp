// SettingsPaint.cpp — Ayarlar penceresi zemini, Rufus tarzı bölüm başlıkları, vurgu renkleri.
//
// Yalnızca GDI kullanılır; diyalog kontrollerinin arkasına Direct2D katmanı koymak
// WM_CTLCOLOR* akışını bozardı. Fırçalar çağrı başına kurulup RAII ile bırakılır:
// WM_ERASEBKGND yalnızca pencere geçersizleştiğinde gelir, önbelleğe değmez ve
// tema değişiminde bayat renk kalma riski de ortadan kalkar.
#include "SettingsPaint.h"

#include "Util.h"
#include "resource.h"

#include <WinDark/WinDark.h>

namespace kli {
namespace SettingsPaint {
namespace {

// IDD_SETTINGS'teki dört bölüm başlığı. Hem vurgu rengi hem ayırıcı konumu bu
// tablodan türer; kaynak yerleşimi değişse de kod uyum sağlar.
constexpr int kSectionIds[] = {IDC_GRP_GENERAL, IDC_GRP_KEYS, IDC_GRP_OSD,
                               IDC_GRP_BEHAVIOR};

// Başlık metninin sağ kenarı ile çizginin başlangıcı arasındaki boşluk.
constexpr float kLineGapDip = 6.0f;

// Ölçülen metin genişliğine eklenen pay: statiğin DrawText'i son harfin bir iki
// pikselini taşırabiliyor, daraltma yüzünden kırpılma olmasın.
constexpr float kTextPadDip = 2.0f;

// Ayırıcı zeminden yalnızca hafifçe ayrışır: GROUPBOX'ın parlak çerçevesi yerine
// "az ama yeterli" bir bölüm sınırı.
[[nodiscard]] COLORREF SeparatorColor(bool dark) noexcept {
    return dark ? RGB(64, 64, 64) : RGB(210, 210, 210);
}

// Bölüm başlığı gövde metninden bir tık daha parlak/koyu — renk değil kontrast
// vurgusu, böylece başlıklar tema rengine bağlı kalmadan öne çıkar.
[[nodiscard]] COLORREF HeaderTextColor(bool dark) noexcept {
    return dark ? RGB(235, 235, 235) : RGB(20, 20, 20);
}

[[nodiscard]] UINT DialogDpi(HWND dlg) noexcept {
    const UINT dpi = ::GetDpiForWindow(dlg);
    return (dpi != 0) ? dpi : kDefaultDpi;
}

// Ölçüm için pencere DC'si. Ölçüm yolunda erken çıkışlar var; elle ReleaseDC
// çağırmak kaçak riski taşır.
class window_dc {
public:
    explicit window_dc(HWND owner) noexcept : m_owner(owner), m_dc(::GetDC(owner)) {}
    ~window_dc() noexcept {
        if (m_dc != nullptr) {
            ::ReleaseDC(m_owner, m_dc);
        }
    }
    window_dc(const window_dc&) = delete;
    window_dc& operator=(const window_dc&) = delete;

    [[nodiscard]] HDC get() const noexcept { return m_dc; }

private:
    HWND m_owner;
    HDC m_dc;
};

// Metnin, kontrolün KENDİ yazı tipiyle piksel genişliği. Font seçimiyle geri alma
// arasında erken çıkış yoktur; DC daima ilk hâline döner.
[[nodiscard]] int TextWidthPx(HDC dc, HWND ctrl, const wchar_t* text, int len) {
    const auto font = reinterpret_cast<HFONT>(::SendMessageW(ctrl, WM_GETFONT, 0, 0));
    const HGDIOBJ prev = (font != nullptr) ? ::SelectObject(dc, font) : nullptr;
    SIZE size{};
    const bool ok = (::GetTextExtentPoint32W(dc, text, len, &size) != FALSE);
    if (prev != nullptr) {
        ::SelectObject(dc, prev);
    }
    return ok ? static_cast<int>(size.cx) : 0;
}

// Kontrol kimliği IDD_SETTINGS bölüm başlıklarından biri mi?
[[nodiscard]] bool IsSectionHeader(int ctrlId) noexcept {
    for (const int id : kSectionIds) {
        if (id == ctrlId) {
            return true;
        }
    }
    return false;
}

// Bölüm başlığı vurgulu, IDC_LBL_HINT soluk çizilir. Renk hdc'ye yazıldıysa true
// döner; diğer statiklerde WinDark'ın varsayılan metin rengi korunur.
[[nodiscard]] bool OverrideStaticColor(HDC hdc, int ctrlId, bool dark) {
    if (hdc == nullptr) {
        return false;
    }
    const bool header = IsSectionHeader(ctrlId);
    if (!header && ctrlId != IDC_LBL_HINT) {
        return false;
    }
    // Zemin WM_ERASEBKGND'de boyandı; metnin arkasına ayrıca blok çizilmesin.
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, header ? HeaderTextColor(dark) : WinDark::DisabledTextColor(dark));
    return true;
}

}  // namespace

void LayoutSectionHeaders(HWND dlg) {
    if (dlg == nullptr) {
        return;
    }
    const window_dc dc(dlg);
    if (dc.get() == nullptr) {
        return;
    }
    const int pad = DipToPx(kTextPadDip, DialogDpi(dlg));
    for (const int id : kSectionIds) {
        const HWND head = ::GetDlgItem(dlg, id);
        if (head == nullptr) {
            continue;
        }
        wchar_t text[96]{};
        const int len = ::GetWindowTextW(head, text, static_cast<int>(_countof(text)));
        RECT rc{};
        if (len <= 0 || !::GetWindowRect(head, &rc)) {
            continue;
        }
        const int width = TextWidthPx(dc.get(), head, text, len) + pad;
        const int limit = static_cast<int>(rc.right - rc.left);
        if (width <= pad || width >= limit) {
            continue;  // ölçüm başarısız ya da metin şablona sığmıyor: dokunma
        }
        ::SetWindowPos(head, nullptr, 0, 0, width, static_cast<int>(rc.bottom - rc.top),
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void Background(HWND dlg, HDC hdc, bool dark) {
    RECT client{};
    if (dlg == nullptr || hdc == nullptr || !GetClientRect(dlg, &client)) {
        return;
    }

    const unique_hbrush back(CreateSolidBrush(WinDark::BackgroundColor(dark)));
    if (!back) {
        return;
    }
    FillRect(hdc, &client, back.get());

    const unique_hbrush line(CreateSolidBrush(SeparatorColor(dark)));
    if (!line) {
        return;  // zemin boyandı; ayırıcı olmadan da pencere okunabilir
    }

    const int gap = DipToPx(kLineGapDip, DialogDpi(dlg));

    // Çizgiler başlık kontrolünün GERÇEK dikdörtgeninden türer: DLU→piksel
    // dönüşümü tekrarlanmaz, her DPI'da ve her yerleşim değişikliğinde hizalı
    // kalır. LayoutSectionHeaders statiği metnine kadar daralttığı için rc.right
    // metnin sağ kenarıdır.
    for (const int id : kSectionIds) {
        const HWND head = GetDlgItem(dlg, id);
        RECT rc{};
        if (head == nullptr || !GetWindowRect(head, &rc)) {
            continue;
        }
        POINT start{rc.right, (rc.top + rc.bottom) / 2};
        POINT inset{rc.left, start.y};
        if (!ScreenToClient(dlg, &start) || !ScreenToClient(dlg, &inset)) {
            continue;
        }
        // Sağ kenar payı başlığın sol payıyla eşitlenir; çizgi alt butonlarla
        // aynı hizada biter.
        const RECT sep{start.x + gap, start.y, client.right - inset.x, start.y + 1};
        if (sep.left >= sep.right) {
            continue;
        }
        FillRect(hdc, &sep, line.get());
    }
}

HBRUSH OnCtlColor(UINT msg, WPARAM wParam, LPARAM lParam, bool dark) {
    // Koyu temada zemin/metin renklerinin tamamı WinDark'tan gelir; açık temada
    // nullptr döner ve sistem varsayılanı bozulmaz.
    HBRUSH brush = WinDark::OnCtlColor(msg, wParam, lParam, dark);
    if (msg != WM_CTLCOLORSTATIC) {
        return brush;
    }
    const int ctrlId = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
    if (!OverrideStaticColor(reinterpret_cast<HDC>(wParam), ctrlId, dark)) {
        return brush;
    }
    // Açık temada WinDark fırça vermez; hdc'ye yazdığımız renk kalıcı olsun diye
    // diyalog zeminiyle aynı sistem fırçasını döndürmek zorundayız — nullptr
    // dönersek DefDlgProc kendi rengini yazar ve vurgu kaybolur.
    return (brush != nullptr) ? brush : GetSysColorBrush(COLOR_3DFACE);
}

}  // namespace SettingsPaint
}  // namespace kli
