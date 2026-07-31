// OsdPosition.cpp — OSD'nin monitördeki yerleşimi (3x3 ızgara + serbest konum)
// ve "konumu ekrandan seç" sürükleme kipi.
//
// OsdWindow sınıfının parçasıdır; bölünme yalnızca dosya başına 400 satır sınırı
// içindir (spec §9). OsdWindow.cpp yaşam döngüsünü, OsdDevice.cpp cihaz
// zincirini, OsdAnimation.cpp animasyonları, bu dosya da konumu tutar.
#include "OsdWindow.h"

#include "Localization.h"
#include "Messages.h"
#include "MonitorUtil.h"

#include <utility>

namespace kli {
namespace {

// Tek eksendeki hizalama. 3x3 ızgaranın dokuz konumu, birbirinden bağımsız iki
// eksen hizasının çarpımıdır; her konum için ayrı formül yazmak yerine eksenler
// ayrıştırılır (görev tanımı: "yatay ve dikey hizayı ayrı ayrı hesaplasın").
enum class Align {
    Near,     // sol / üst
    Middle,   // ortalı
    Far,      // sağ / alt
};

struct Alignment {
    Align h;
    Align v;
};

[[nodiscard]] Alignment AlignmentFor(OsdPlacement p) noexcept {
    switch (p) {
        case OsdPlacement::TopLeft:      return {Align::Near,   Align::Near};
        case OsdPlacement::TopCenter:    return {Align::Middle, Align::Near};
        case OsdPlacement::TopRight:     return {Align::Far,    Align::Near};
        case OsdPlacement::MiddleLeft:   return {Align::Near,   Align::Middle};
        case OsdPlacement::Center:       return {Align::Middle, Align::Middle};
        case OsdPlacement::MiddleRight:  return {Align::Far,    Align::Middle};
        case OsdPlacement::BottomLeft:   return {Align::Near,   Align::Far};
        case OsdPlacement::BottomCenter: return {Align::Middle, Align::Far};
        case OsdPlacement::BottomRight:  return {Align::Far,    Align::Far};
        case OsdPlacement::Custom:       break;   // çağıran ayrı ele alır
    }
    return {Align::Middle, Align::Near};
}

// Bir eksende yüzeyin çalışma alanı içindeki başlangıç noktası.
//
// Kenar boşluğu ARTIK HER İKİ EKSENDE de uygulanır (1.0'da yalnızca dikeydi):
// kenara yaslı hizalarda boşluk kadar içeri çekilir, ortalı hizada anlamı yoktur.
// pad, yüzeyin kart çevresindeki gölge payıdır; kullanıcı boşluğu KARTIN kenara
// uzaklığı olarak okur, bu yüzden geri alınır.
[[nodiscard]] int OffsetFor(Align a, int work, int surface, int margin, int pad) noexcept {
    switch (a) {
        case Align::Near:   return margin - pad;
        case Align::Middle: return (work - surface) / 2;
        case Align::Far:    return work - margin - surface + pad;
    }
    return 0;
}

// Oran → piksel. span, yüzeyin kayabileceği toplam mesafedir.
[[nodiscard]] int SpanOffset(int span, unsigned ratio) noexcept {
    if (span <= 0) {
        return 0;
    }
    const unsigned r = Clamp(ratio, 0u, kCustomRangeMax);
    // Ara çarpım 64 bitte yapılır: 32 bitte span çok büyük bir sanal masaüstünde
    // taşabilir ve konum negatife düşerdi.
    const long long v = (static_cast<long long>(span) * r + kCustomRangeMax / 2) /
                        kCustomRangeMax;
    return static_cast<int>(v);
}

// Piksel → oran (SpanOffset'in tersi).
[[nodiscard]] unsigned RatioFor(int offset, int span) noexcept {
    if (span <= 0) {
        return 0;
    }
    const int clamped = Clamp(offset, 0, span);
    const long long v = (static_cast<long long>(clamped) * kCustomRangeMax + span / 2) / span;
    return static_cast<unsigned>(v);
}

}  // namespace

// ---------------------------------------------------------------------------
// Yerleşim
// ---------------------------------------------------------------------------

MonitorMetrics OsdWindow::TargetMonitor() const {
    // Sabitlenmiş monitör "tüm monitörlerde göster" kipinden gelir: o kipte her
    // OsdWindow örneği kendi ekranına çakılıdır, imleç nerede olursa olsun.
    if (m_config.pinnedMonitor != nullptr) {
        return MonitorUtil::ForHandle(m_config.pinnedMonitor);
    }
    return MonitorUtil::Active(m_config.primaryMonitorOnly);
}

void OsdWindow::PositionForCurrentMonitor() {
    if (m_hwnd == nullptr) {
        return;
    }
    const MonitorMetrics m = TargetMonitor();

    // Yüzey, pencere boyutunu belirler; ikisi aynı DPI'dan türetilmezse kart
    // ölçeklenir ve bulanıklaşır.
    HR_LOG(EnsureSurface(m.dpi));

    // Tüm ölçüler tam sayı piksele yuvarlanır — yarım piksel kayması kartın
    // kenarlarını bulanıklaştırır (spec §11). Boyut çarpanı yüzeye de, gölge
    // payına da uygulanır; EnsureSurface tam olarak aynı hesabı yapar.
    // Yüzey artık KARE DEĞİLDİR (minimal çubuk kipi): iki eksen ayrı ölçülür.
    const int surfaceW = DipToPx(SurfaceWidthDip(), m.dpi);
    const int surfaceH = DipToPx(SurfaceHeightDip(), m.dpi);
    const int padPx = DipToPx(SurfacePaddingDip(), m.dpi);
    const int marginPx = DipToPx(static_cast<float>(m_config.topMarginDip), m.dpi);

    const int workLeft = static_cast<int>(m.work.left);
    const int workTop = static_cast<int>(m.work.top);
    const int workW = static_cast<int>(m.work.right - m.work.left);
    const int workH = static_cast<int>(m.work.bottom - m.work.top);

    int x = 0;
    int y = 0;
    if (m_config.placement == OsdPlacement::Custom) {
        // Serbest konum: oran, yüzeyin KAYABİLECEĞİ mesafeye uygulanır (sol-üst
        // köşenin mutlak oranına değil), böylece 1.0 oranı kartı ekran dışına
        // taşırmadan sağ/alt kenara oturtur.
        x = workLeft + SpanOffset(workW - surfaceW, m_config.customX);
        y = workTop + SpanOffset(workH - surfaceH, m_config.customY);
    } else {
        const Alignment a = AlignmentFor(m_config.placement);
        x = workLeft + OffsetFor(a.h, workW, surfaceW, marginPx, padPx);
        y = workTop + OffsetFor(a.v, workH, surfaceH, marginPx, padPx);
    }

    // Uç ayar değerlerinde (0 boşluk, küçük ekran) yüzey monitörden taşabilir;
    // gölge payı çalışma alanının dışına çıkabilir ama ekranın dışına çıkmamalı.
    const int minX = static_cast<int>(m.full.left);
    const int minY = static_cast<int>(m.full.top);
    const int maxX = static_cast<int>(m.full.right) - surfaceW;
    const int maxY = static_cast<int>(m.full.bottom) - surfaceH;
    x = Clamp(x, minX, maxX > minX ? maxX : minX);
    y = Clamp(y, minY, maxY > minY ? maxY : minY);

    ::SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, surfaceW, surfaceH, SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// Konum seçme kipi
// ---------------------------------------------------------------------------

void OsdWindow::ApplyPickStyles(bool picking) {
    if (m_hwnd == nullptr) {
        return;
    }
    // WS_EX_TRANSPARENT kaldırılmadan pencere fare mesajı almaz (tıklama altındaki
    // pencereye gider); WS_EX_NOACTIVATE kaldırılmadan da odak alamaz, dolayısıyla
    // Esc tuşu hiç ulaşmaz. İkisi de kip bitince aynen geri konur.
    const LONG_PTR bits = static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    LONG_PTR ex = ::GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    ex = picking ? (ex & ~bits) : (ex | bits);
    ::SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex);
    ::SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);
}

void OsdWindow::BeginPositionPicking(PositionPickedCallback onPicked) {
    if (m_hwnd == nullptr) {
        return;
    }
    m_onPicked = std::move(onPicked);
    if (m_picking) {
        ::SetForegroundWindow(m_hwnd);
        return;
    }

    // Boşta bırakılmış zincir geri kurulur; sürüklenecek bir kart olmalı.
    ::KillTimer(m_hwnd, TIMER_OSD_IDLE);
    if (!m_deviceChainValid) {
        if (FAILED(CreateDeviceChain())) {
            return;
        }
        m_surfaceDpi = 0;
    }
    ::KillTimer(m_hwnd, TIMER_OSD_DWELL);
    ::KillTimer(m_hwnd, TIMER_OSD_GONE);

    // Örnek içerik: kip boyunca kartın gerçek boyutu ve görünümü sürüklenir,
    // kullanıcı sonucu birebir görsün.
    m_content.key = LockKey::Caps;
    m_content.on = true;
    m_content.title = Loc::KeyTitle(LockKey::Caps);
    m_content.status = Loc::StateText(true);

    m_picking = true;
    m_dragging = false;
    ApplyPickStyles(true);
    PositionForCurrentMonitor();
    if (!RenderWithDeviceRecovery()) {
        m_picking = false;
        ApplyPickStyles(false);
        return;
    }

    ::SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    // Animasyon yok: kart anında ve tam opak belirir, sayaç da kurulmaz — kip
    // kapanana kadar ekranda kalır.
    SetStaticOpacity(1.0f);
    m_phase = Phase::Dwell;
    ::SetForegroundWindow(m_hwnd);
    ::SetFocus(m_hwnd);
}

void OsdWindow::EndPositionPicking(bool commit) {
    if (!m_picking) {
        return;
    }
    if (m_dragging) {
        ::ReleaseCapture();
        m_dragging = false;
    }

    // Oran, kartın BIRAKILDIĞI monitöre göre ölçülür: kullanıcı ikinci ekrana
    // sürüklediyse kastettiği yer orasıdır.
    unsigned rx = 0;
    unsigned ry = 0;
    bool measured = false;
    RECT rc{};
    if (commit && m_hwnd != nullptr && ::GetWindowRect(m_hwnd, &rc)) {
        const MonitorMetrics m = MonitorUtil::ForWindow(m_hwnd);
        const int surfaceW = static_cast<int>(rc.right - rc.left);
        const int surfaceH = static_cast<int>(rc.bottom - rc.top);
        rx = RatioFor(static_cast<int>(rc.left - m.work.left),
                      static_cast<int>(m.work.right - m.work.left) - surfaceW);
        ry = RatioFor(static_cast<int>(rc.top - m.work.top),
                      static_cast<int>(m.work.bottom - m.work.top) - surfaceH);
        measured = true;
    }

    m_picking = false;
    ApplyPickStyles(false);
    HideImmediate();

    if (m_onPicked) {
        // Kopya üzerinden çağrılır: geri çağrı App'e döner, App SetConfig ile bu
        // nesneye yeniden girer ve m_onPicked'i değiştirebilir.
        const PositionPickedCallback cb = m_onPicked;
        cb(measured, rx, ry);
    }
}

bool OsdWindow::HandlePickMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result) {
    (void)lParam;
    if (!m_picking) {
        return false;
    }
    result = 0;
    switch (msg) {
        case WM_NCHITTEST:
            // Kip boyunca tıklama YAKALANIR (normalde HTTRANSPARENT dönülür).
            result = HTCLIENT;
            return true;

        case WM_SETCURSOR:
            // Pencere sınıfının imleci yok; kip boyunca taşıma imleci gösterilir.
            ::SetCursor(::LoadCursorW(nullptr, IDC_SIZEALL));
            result = TRUE;
            return true;

        case WM_LBUTTONDOWN: {
            RECT rc{};
            POINT pt{};
            if (::GetWindowRect(m_hwnd, &rc) && ::GetCursorPos(&pt)) {
                // Fare ile sol-üst köşe farkı korunur: kart imlecin altına
                // sıçramaz, tutulduğu yerden sürüklenir.
                m_dragGrab.x = pt.x - rc.left;
                m_dragGrab.y = pt.y - rc.top;
                m_dragging = true;
                ::SetCapture(m_hwnd);
            }
            return true;
        }

        case WM_MOUSEMOVE: {
            POINT pt{};
            if (m_dragging && ::GetCursorPos(&pt)) {
                ::SetWindowPos(m_hwnd, HWND_TOPMOST, pt.x - m_dragGrab.x,
                               pt.y - m_dragGrab.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return true;
        }

        case WM_LBUTTONUP:
            // Bırakma hem kaydeder hem kipten çıkar.
            EndPositionPicking(true);
            return true;

        case WM_RBUTTONDOWN:
            EndPositionPicking(false);
            return true;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                EndPositionPicking(false);
            } else if (wParam == VK_RETURN || wParam == VK_SPACE) {
                EndPositionPicking(true);
            }
            return true;

        case WM_CAPTURECHANGED:
            // Yakalama başkasına geçtiyse sürükleme de bitmiştir.
            m_dragging = false;
            return true;

        default:
            break;
    }
    return false;
}

}  // namespace kli
