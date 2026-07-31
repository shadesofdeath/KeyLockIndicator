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

// IDD_SETTINGS'teki bölüm başlıkları. Hem vurgu rengi hem ayırıcı konumu bu
// tablodan türer.
//
// IDD_SETTINGS artık İKİ KOLONLUDUR: ayırıcı çizgi pencerenin sağ kenarına
// kadar değil, başlığın ait olduğu KOLONUN sağ kenarına kadar uzanmalıdır —
// aksi hâlde sol kolonun çizgisi sağ kolonun kontrollerinin üstünden geçerdi.
// Bu yüzden her başlığın şablondaki x + genişlik değeri (DLU) burada tekrarlanır
// ve MapDialogRect ile piksele çevrilir; sabit bir DLU→px oranı varsayılmaz,
// dönüşümü diyaloğun kendi taban birimleri yapar.
struct SectionSpec {
    int id;
    int xDlu;
    int widthDlu;
};

constexpr SectionSpec kSections[] = {
    {IDC_GRP_GENERAL,  8, 286},
    {IDC_GRP_KEYS,     8, 286},
    {IDC_GRP_BEHAVIOR, 8, 286},
    {IDC_GRP_OSD,    306, 282},
    {IDC_GRP_EXCLUDE, 306, 282},
};

// Bölümün piksel cinsinden sol/sağ sınırı. MapDialogRect diyaloğun yazı tipi ve
// DPI'sını hesaba katar, bu yüzden her ölçekte doğrudur.
[[nodiscard]] bool SectionBoundsPx(HWND dlg, const SectionSpec& spec, RECT& out) noexcept {
    out = RECT{static_cast<LONG>(spec.xDlu), 0,
               static_cast<LONG>(spec.xDlu + spec.widthDlu), 8};
    return ::MapDialogRect(dlg, &out) != FALSE;
}

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

// Liste kutusunun çerçevesi. Ayırıcıdan bir tık belirgin: bu bir bölüm sınırı
// değil, kontrolün kendi sınırıdır ve kullanıcı nereye tıklayacağını görmeli.
[[nodiscard]] COLORREF ControlBorderColor(bool dark) noexcept {
    return dark ? RGB(84, 84, 84) : RGB(160, 160, 160);
}

// Kontrolün DIŞ kenarına 1 piksellik çerçeve çizer. Kontrolün kendi WS_BORDER'ı
// yoktur (bkz. app.rc IDC_LST_EXCLUDE notu); bu 1 piksel diyaloğun zeminine
// aittir, dolayısıyla WM_ERASEBKGND'de çizilir ve kontrol onu ezemez.
void FrameAroundControl(HWND dlg, HDC hdc, int ctrlId, HBRUSH brush) {
    const HWND ctrl = ::GetDlgItem(dlg, ctrlId);
    RECT rc{};
    if (ctrl == nullptr || brush == nullptr || !::GetWindowRect(ctrl, &rc)) {
        return;
    }
    POINT topLeft{rc.left, rc.top};
    POINT bottomRight{rc.right, rc.bottom};
    if (!::ScreenToClient(dlg, &topLeft) || !::ScreenToClient(dlg, &bottomRight)) {
        return;
    }
    RECT frame{topLeft.x - 1, topLeft.y - 1, bottomRight.x + 1, bottomRight.y + 1};
    ::FrameRect(hdc, &frame, brush);
}

// Bölüm başlığı gövde metninden bir tık daha parlak/koyu — renk değil kontrast
// vurgusu, böylece başlıklar tema rengine bağlı kalmadan öne çıkar.
[[nodiscard]] COLORREF HeaderTextColor(bool dark) noexcept {
    return dark ? RGB(235, 235, 235) : RGB(20, 20, 20);
}

// Kısayol çakışması uyarısı. Koyu zeminde saf kırmızı okunmaz; tema başına
// kontrastı tutan iki ton kullanılır.
[[nodiscard]] COLORREF WarningTextColor(bool dark) noexcept {
    return dark ? RGB(240, 120, 120) : RGB(176, 32, 32);
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
    for (const SectionSpec& spec : kSections) {
        if (spec.id == ctrlId) {
            return true;
        }
    }
    return false;
}

// Bölüm başlığı vurgulu, IDC_LBL_HINT soluk, kısayol uyarısı kırmızı çizilir.
// Renk hdc'ye yazıldıysa true döner; diğer statiklerde WinDark'ın varsayılan
// metin rengi korunur.
[[nodiscard]] bool OverrideStaticColor(HDC hdc, int ctrlId, bool dark) {
    if (hdc == nullptr) {
        return false;
    }
    const bool header = IsSectionHeader(ctrlId);
    if (!header && ctrlId != IDC_LBL_HINT && ctrlId != IDC_LBL_HOTKEYWARN) {
        return false;
    }
    // Zemin WM_ERASEBKGND'de boyandı; metnin arkasına ayrıca blok çizilmesin.
    SetBkMode(hdc, TRANSPARENT);
    if (header) {
        SetTextColor(hdc, HeaderTextColor(dark));
    } else if (ctrlId == IDC_LBL_HOTKEYWARN) {
        SetTextColor(hdc, WarningTextColor(dark));
    } else {
        SetTextColor(hdc, WinDark::DisabledTextColor(dark));
    }
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
    for (const SectionSpec& spec : kSections) {
        const HWND head = ::GetDlgItem(dlg, spec.id);
        if (head == nullptr) {
            continue;
        }
        wchar_t text[96]{};
        const int len = ::GetWindowTextW(head, text, static_cast<int>(_countof(text)));
        RECT rc{};
        if (len <= 0 || !::GetWindowRect(head, &rc)) {
            continue;
        }
        RECT bounds{};
        if (!SectionBoundsPx(dlg, spec, bounds)) {
            continue;
        }
        // Üst sınır kontrolün ANLIK genişliği DEĞİL, ŞABLONUN bıraktığı yerdir.
        // Anlık genişlik kullanılırsa bir kez daraltılan başlık daha UZUN bir
        // çeviriye geçildiğinde bir daha genişleyemez ve metin kırpılır — dil
        // canlı değişince "GENERAL" yerine "GENER" görünürdü.
        const int limit = static_cast<int>(bounds.right - bounds.left);
        int width = TextWidthPx(dc.get(), head, text, len) + pad;
        if (width <= pad || limit <= pad) {
            continue;  // ölçüm başarısız ya da kullanılabilir alan yok: dokunma
        }
        if (width > limit) {
            width = limit;  // çeviri şablona sığmıyor: taşırmak yerine sınıra oturt
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

    // Çizginin BAŞLANGICI başlık kontrolünün gerçek dikdörtgeninden gelir
    // (LayoutSectionHeaders statiği metnine kadar daralttığı için rc.right
    // metnin sağ kenarıdır), BİTİŞİ ise başlığın ait olduğu kolonun sağ
    // kenarından — iki kolonlu yerleşimde pencere kenarı yanlış olurdu.
    for (const SectionSpec& spec : kSections) {
        const HWND head = GetDlgItem(dlg, spec.id);
        RECT rc{};
        if (head == nullptr || !GetWindowRect(head, &rc)) {
            continue;
        }
        RECT bounds{};
        if (!SectionBoundsPx(dlg, spec, bounds)) {
            continue;
        }
        POINT start{rc.right, (rc.top + rc.bottom) / 2};
        if (!ScreenToClient(dlg, &start)) {
            continue;
        }
        const RECT sep{start.x + gap, start.y, bounds.right, start.y + 1};
        if (sep.left >= sep.right) {
            continue;
        }
        FillRect(hdc, &sep, line.get());
    }

    // İstisna listesinin çerçevesi (madde 18). Kontrolün kendi WS_BORDER'ı
    // koyu temada beyaz kaldığı için çerçeve buradan çizilir.
    const unique_hbrush border(CreateSolidBrush(ControlBorderColor(dark)));
    if (border) {
        FrameAroundControl(dlg, hdc, IDC_LST_EXCLUDE, border.get());
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
