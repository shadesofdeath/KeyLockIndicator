// OsdWindow.cpp — Pencere sınıfı kaydı, yaşam döngüsü, mesaj yönlendirme ve
// monitör yerleşimi (spec §4.3, §6).
//
// Sınıfın geri kalanı OsdDevice.cpp (D3D/D2D/DComp zinciri) ve
// OsdAnimation.cpp (DirectComposition animasyonları) içindedir. Bölünme
// yalnızca dosya başına 400 satır sınırı içindir (spec §9); üç dosya aynı
// sınıfın parçalarıdır, yeni bir katman değildir.
#include "OsdWindow.h"

#include "Localization.h"
#include "Messages.h"
#include "MonitorUtil.h"
#include "OsdRenderer.h"

namespace kli {
namespace {

// Pencere sınıfı yalnızca bir kez kaydedilir. Kayıt durumunu dosya kapsamlı bir
// bayrakta tutmak yerine (spec §9: global değişken yok) ikinci çağrının
// döndürdüğü ERROR_CLASS_ALREADY_EXISTS tolere edilir.
bool RegisterOsdClass(HINSTANCE instance, WNDPROC proc) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    // Zemin fırçası yok: her piksel DirectComposition yüzeyinden gelir. GDI ile
    // silinen bir zemin, fade sırasında siyah kenar artefaktı yaratır (spec §11).
    wc.hbrBackground = nullptr;
    // İmleç yok: pencere tıklama da almadığı için imleci değiştirmemesi gerekir.
    wc.hCursor = nullptr;
    wc.lpszClassName = kOsdWindowClass;

    if (::RegisterClassExW(&wc) == 0) {
        const DWORD err = ::GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            LogV(L"OSD pencere sınıfı kaydedilemedi: %lu", err);
            return false;
        }
    }
    return true;
}

}  // namespace

OsdWindow::~OsdWindow() {
    // Destroy iki kez çağrılabilir; yıkıcıda tekrar çağırmak güvenlidir.
    Destroy();
}

// ---------------------------------------------------------------------------
// Oluşturma / yıkım
// ---------------------------------------------------------------------------

bool OsdWindow::Create(HINSTANCE hInstance) {
    if (m_hwnd != nullptr) {
        return true;
    }
    m_instance = hInstance;
    if (!RegisterOsdClass(hInstance, &OsdWindow::WndProc)) {
        return false;
    }

    // spec §4.3: tam olarak bu stiller. WS_EX_TRANSPARENT + WS_EX_NOACTIVATE
    // odak ve tıklama geçirgenliğini, WS_EX_NOREDIRECTIONBITMAP ise DWM'nin
    // gereksiz yönlendirme yüzeyi ayırmasını engeller (DComp kendi yüzeyini sunar).
    const DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW |
                          WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;
    const DWORD style = WS_POPUP;

    // Başlangıç boyutu 96 DPI karşılığı; gerçek boyut ilk gösterimde aktif
    // monitörün DPI'sıyla PositionForCurrentMonitor içinde ayarlanır.
    const int side = DipToPx(OsdLayout::kSurfaceSize, kDefaultDpi);

    // WS_VISIBLE verilmez → pencere gizli doğar (spec §6: açılışta hazırlanır,
    // gösterilmez). `this` WM_NCCREATE'te GWLP_USERDATA'ya taşınır.
    HWND hwnd = ::CreateWindowExW(exStyle, kOsdWindowClass, L"", style,
                                  0, 0, side, side,
                                  nullptr, nullptr, hInstance, this);
    if (hwnd == nullptr) {
        LogV(L"OSD penceresi oluşturulamadı: %lu", ::GetLastError());
        return false;
    }
    m_hwnd = hwnd;

    // WS_EX_LAYERED pencere, bir katman özniteliği (alfa ya da renk anahtarı)
    // ayarlanmadıkça DWM tarafından HİÇ kompozit edilmez — ekranda hiçbir şey
    // görünmez. 255 alfa ile bir kez ayarlanır; gerçek saydamlık DComp
    // yüzeyinin per-pixel alfasından gelir, buradaki global alfadan değil.
    if (!::SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA)) {
        LogV(L"SetLayeredWindowAttributes başarısız: %lu", ::GetLastError());
    }

    // Tüm zincir şimdi kurulur; ilk tuş basımında ~200 ms gecikme kabul edilemez.
    const HRESULT hr = CreateDeviceChain();
    if (FAILED(hr)) {
        LogV(L"OSD cihaz zinciri kurulamadı: 0x%08X", static_cast<unsigned>(hr));
        Destroy();
        return false;
    }

    m_phase = Phase::Hidden;
    return true;
}

void OsdWindow::Destroy() {
    if (m_hwnd != nullptr) {
        ::KillTimer(m_hwnd, TIMER_OSD_DWELL);
        ::KillTimer(m_hwnd, TIMER_OSD_GONE);
        ::KillTimer(m_hwnd, TIMER_OSD_IDLE);
    }
    ReleaseDeviceChain();

    if (m_hwnd != nullptr) {
        // Yıkım sırasında gelen mesajların yarı yıkılmış nesneye ulaşmaması için
        // geri işaretçi önce koparılır.
        ::SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, 0);
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_phase = Phase::Hidden;
    m_instance = nullptr;
}

// ---------------------------------------------------------------------------
// Mesaj yönlendirme
// ---------------------------------------------------------------------------

LRESULT CALLBACK OsdWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<OsdWindow*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) {
            // HandleMessage m_hwnd üzerinden çalışır; CreateWindowExW dönmeden
            // gelen mesajlar için tutamacı burada kesinleştir.
            self->m_hwnd = hwnd;
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    auto* self = reinterpret_cast<OsdWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr) {
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(msg, wParam, lParam);
}

LRESULT OsdWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_NCHITTEST:
            // Ek güvence: WS_EX_TRANSPARENT'a ek olarak isabet testi de reddedilir,
            // böylece OSD üzerine yapılan tıklama altındaki pencereye gider.
            return HTTRANSPARENT;

        case WM_ERASEBKGND:
            // Zemin silinmez; içerik tamamen DComp yüzeyinden gelir.
            return 1;

        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
            // Önerilen dikdörtgen (lParam) yok sayılır: konum ve boyut her zaman
            // aktif monitörün çalışma alanından yeniden hesaplanır.
            OnDpiOrMonitorChanged();
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_OSD_DWELL) {
                ::KillTimer(m_hwnd, TIMER_OSD_DWELL);
                ApplyExitAnimation();
                m_phase = Phase::Exiting;
                // Animasyon süresi + küçük pay: son kare sunulmadan pencereyi
                // gizlemek görünür bir kesme yaratır.
                constexpr UINT kGoneDelayMs =
                    static_cast<UINT>(kExitDurationSec * 1000.0) + 40u;
                ::SetTimer(m_hwnd, TIMER_OSD_GONE, kGoneDelayMs, nullptr);
                return 0;
            }
            if (wParam == TIMER_OSD_GONE) {
                ::KillTimer(m_hwnd, TIMER_OSD_GONE);
                HideImmediate();
                return 0;
            }
            if (wParam == TIMER_OSD_IDLE) {
                ::KillTimer(m_hwnd, TIMER_OSD_IDLE);
                // Boşta: sürücünün ~33 MB'lık yığınlarını geri ver (bkz. Messages.h).
                ReleaseDeviceChain();
                // Çalışma setini boşalt — Görev Yöneticisi'nin gösterdiği sayı budur.
                // Sayfalar gerektiğinde geri yüklenir; işlevsel maliyeti yok.
                ::SetProcessWorkingSetSizeEx(::GetCurrentProcess(),
                                             static_cast<SIZE_T>(-1),
                                             static_cast<SIZE_T>(-1), 0);
                return 0;
            }
            break;

        case WM_DESTROY:
            // Pencere dışarıdan yıkılırsa (oturum sonu) sarkan tutamaç kalmasın.
            m_hwnd = nullptr;
            m_phase = Phase::Hidden;
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Yapılandırma ve tema
// ---------------------------------------------------------------------------

void OsdWindow::SetConfig(const OsdConfig& config) {
    m_config = config;
    m_renderer.SetTheme(m_theme, m_config.cardAlpha);
    if (Visible()) {
        // Yerleşim/opaklık ayarları anında etkili olmalı (spec §4.6).
        PositionForCurrentMonitor();
        RenderWithDeviceRecovery();
    }
}

void OsdWindow::OnThemeChanged(AppTheme theme) {
    m_theme = theme;
    m_renderer.SetTheme(theme, m_config.cardAlpha);
    if (Visible()) {
        // Geçiş animasyonu yok: anında yeni renklerle yeniden çizilir (spec §4.2).
        RenderWithDeviceRecovery();
    }
}

void OsdWindow::OnDpiOrMonitorChanged() {
    if (Visible()) {
        PositionForCurrentMonitor();
        RenderWithDeviceRecovery();
    } else {
        // Gizliyken yüzeyi yeniden kurmanın anlamı yok; DPI'yı geçersiz kılmak
        // bir sonraki gösterimde doğru boyutta yüzey oluşmasını sağlar.
        m_surfaceDpi = 0;
    }
}

// ---------------------------------------------------------------------------
// Gösterim
// ---------------------------------------------------------------------------

void OsdWindow::Show(LockKey key, bool isOn) {
    if (m_hwnd == nullptr) {
        return;
    }
    // Tam ekran oyun/sunumda rahatsız etmemek için bastırılır (spec §4.4).
    if (m_config.suppressFullscreen && MonitorUtil::IsPresentationOrFullscreen()) {
        return;
    }

    // Boşta yıkım sayacı iptal edilir; zincir bırakılmışsa yeniden kurulur.
    // Ölçülen maliyet 18–22 ms, spec §11'in 100 ms bütçesinin çok altında.
    ::KillTimer(m_hwnd, TIMER_OSD_IDLE);
    if (!m_deviceChainValid) {
        if (FAILED(CreateDeviceChain())) {
            return;
        }
        // Yüzey zincirle birlikte gitti; doğru DPI ile yeniden kurulmalı.
        m_surfaceDpi = 0;
    }

    m_content.key = key;
    m_content.on = isOn;
    m_content.title = Loc::KeyTitle(key);
    m_content.status = Loc::StateText(isOn);

    switch (m_phase) {
        case Phase::Hidden: {
            PositionForCurrentMonitor();
            if (!RenderWithDeviceRecovery()) {
                // Çizim kurulamadıysa boş/çöp bir pencere göstermektense hiç
                // göstermemek yeğdir; bekleme sayacı da kurulmaz.
                return;
            }
            // ShowWindow(SW_SHOW) ASLA kullanılmaz: odak/z-düzeni yan etkileri
            // olur. Geometri PositionForCurrentMonitor'da ayarlandı, burada
            // yalnızca görünürlük ve topmost tazelenir.
            ::SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                           SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            ApplyEnterAnimation(0.0f);
            m_phase = Phase::Entering;
            break;
        }

        case Phase::Entering:
        case Phase::Dwell:
            // Animasyon BAŞTAN BAŞLATILMAZ (spec §3.6); yalnızca içerik anında
            // güncellenir. Böylece hızlı ardışık basımlarda titreme olmaz.
            RenderWithDeviceRecovery();
            break;

        case Phase::Exiting: {
            // Fade-out başlamışsa opaklık mevcut değerinden 1'e geri çıkar.
            const float current = EstimateCurrentOpacity();
            RenderWithDeviceRecovery();
            ApplyEnterAnimation(current);
            m_phase = Phase::Entering;
            break;
        }
    }

    // Her hâlde bekleme sayacı sıfırlanır; pencere birikmez, tek pencere yaşar.
    ArmDwellTimer();
}

void OsdWindow::HideImmediate() {
    if (m_hwnd == nullptr) {
        return;
    }
    ::KillTimer(m_hwnd, TIMER_OSD_DWELL);
    ::KillTimer(m_hwnd, TIMER_OSD_GONE);
    // SW_HIDE odak veya z-düzeni değiştirmez; yasak olan yalnızca SW_SHOW'dur.
    ::ShowWindow(m_hwnd, SW_HIDE);
    // Bir sonraki gösterim 0'dan başlasın: gizliyken opaklık sıfırda tutulur.
    SetStaticOpacity(0.0f);
    m_phase = Phase::Hidden;

    // Kısa bir paydan sonra cihaz zinciri bırakılır (bkz. Messages.h). Hemen
    // bırakmak, hızlı ardışık basımlarda her seferinde yeniden kurulum demekti.
    ::SetTimer(m_hwnd, TIMER_OSD_IDLE, kOsdIdleTeardownMs, nullptr);
}

void OsdWindow::ArmDwellTimer() {
    if (m_hwnd == nullptr) {
        return;
    }
    // Yeniden tetiklenmede çıkış sayacı iptal edilir, aksi hâlde eski sayaç
    // pencereyi ortada gizler. Faz Entering olarak korunur: giriş animasyonu
    // kesilmesin ama bekleme süresi baştan sayılsın.
    ::KillTimer(m_hwnd, TIMER_OSD_GONE);
    ::SetTimer(m_hwnd, TIMER_OSD_DWELL, m_config.durationMs, nullptr);
}

// ---------------------------------------------------------------------------
// Yerleşim
// ---------------------------------------------------------------------------

void OsdWindow::PositionForCurrentMonitor() {
    if (m_hwnd == nullptr) {
        return;
    }
    const MonitorMetrics m = MonitorUtil::Active(m_config.primaryMonitorOnly);

    // Yüzey, pencere boyutunu belirler; ikisi aynı DPI'dan türetilmezse kart
    // ölçeklenir ve bulanıklaşır.
    HR_LOG(EnsureSurface(m.dpi));

    // Tüm ölçüler tam sayı piksele yuvarlanır — yarım piksel kayması kartın
    // kenarlarını bulanıklaştırır (spec §11).
    const int surfacePx = DipToPx(OsdLayout::kSurfaceSize, m.dpi);
    const int padPx = DipToPx(OsdLayout::kSurfacePadding, m.dpi);
    const int marginPx = DipToPx(static_cast<float>(m_config.topMarginDip), m.dpi);

    const int workLeft = static_cast<int>(m.work.left);
    const int workTop = static_cast<int>(m.work.top);
    const int workW = static_cast<int>(m.work.right - m.work.left);
    const int workH = static_cast<int>(m.work.bottom - m.work.top);

    int x = workLeft + (workW - surfacePx) / 2;
    int y = workTop;
    switch (m_config.placement) {
        case OsdPlacement::Top:
            // Boşluk kartın kendisi için; yüzeyin gölge payı geri alınır.
            y = workTop + marginPx - padPx;
            break;
        case OsdPlacement::Center:
            y = workTop + (workH - surfacePx) / 2;
            break;
        case OsdPlacement::Bottom:
            y = workTop + workH - marginPx - surfacePx + padPx;
            break;
    }

    // Uç ayar değerlerinde (0 boşluk, küçük ekran) yüzey monitörden taşabilir;
    // gölge payı çalışma alanının dışına çıkabilir ama ekranın dışına çıkmamalı.
    const int minX = static_cast<int>(m.full.left);
    const int minY = static_cast<int>(m.full.top);
    const int maxX = static_cast<int>(m.full.right) - surfacePx;
    const int maxY = static_cast<int>(m.full.bottom) - surfacePx;
    x = Clamp(x, minX, maxX > minX ? maxX : minX);
    y = Clamp(y, minY, maxY > minY ? maxY : minY);

    ::SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, surfacePx, surfacePx, SWP_NOACTIVATE);
}

}  // namespace kli
