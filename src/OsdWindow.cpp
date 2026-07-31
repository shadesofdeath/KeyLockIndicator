// OsdWindow.cpp — Pencere sınıfı kaydı, yaşam döngüsü, mesaj yönlendirme ve
// monitör yerleşimi (spec §4.3, §6).
//
// Sınıfın geri kalanı OsdDevice.cpp (D3D/D2D/DComp zinciri), OsdAnimation.cpp
// (DirectComposition animasyonları) ve OsdPosition.cpp (monitör yerleşimi +
// konum seçme kipi) içindedir. Bölünme yalnızca dosya başına 400 satır sınırı
// içindir (spec §9); dört dosya aynı sınıfın parçalarıdır, yeni katman değildir.
#include "OsdWindow.h"

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

    // Başlangıç boyutu 96 DPI karşılığı ve VARSAYILAN görünüm kipinin ölçüsü;
    // gerçek boyut ilk gösterimde aktif monitörün DPI'sı ve seçili kiple
    // PositionForCurrentMonitor içinde ayarlanır.
    const int startW = DipToPx(SurfaceWidthDip(), kDefaultDpi);
    const int startH = DipToPx(SurfaceHeightDip(), kDefaultDpi);

    // WS_VISIBLE verilmez → pencere gizli doğar (spec §6: açılışta hazırlanır,
    // gösterilmez). `this` WM_NCCREATE'te GWLP_USERDATA'ya taşınır.
    HWND hwnd = ::CreateWindowExW(exStyle, kOsdWindowClass, L"", style,
                                  0, 0, startW, startH,
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
        ::KillTimer(m_hwnd, TIMER_OSD_BADGE);
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
    // Konum seçme kipi, aşağıdaki normal davranışların bir kısmını (özellikle
    // isabet testini) tersine çevirir; bu yüzden en başta sorulur (OsdPosition.cpp).
    LRESULT pickResult = 0;
    if (HandlePickMessage(msg, wParam, lParam, pickResult)) {
        return pickResult;
    }

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
            if (wParam == TIMER_OSD_BADGE) {
                PollFullscreenSuppression();
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
    const bool wasPersistent = m_config.persistent;
    m_config = config;
    // Settings zaten kısıtlıyor; OsdWindow onu tanımadığı için sınır burada da
    // uygulanır (bozuk bir çarpan yüzey boyutunu 0 ya da devasa yapabilir).
    m_config.scale = Clamp(m_config.scale, kMinScale, kMaxScale);
    m_renderer.SetTheme(m_theme, m_config.cardAlpha, m_config.highContrast);
    m_renderer.SetScale(m_config.scale);
    m_renderer.SetView(m_config.view);

    // Kalıcı rozet kipinin tam ekran yoklaması. Sayaç YALNIZCA bu kipte yaşar;
    // geçici OSD'de tam ekran kontrolü zaten Show() içinde yapılıyor.
    if (m_hwnd != nullptr) {
        if (m_config.persistent) {
            ::SetTimer(m_hwnd, TIMER_OSD_BADGE, kOsdBadgePollMs, nullptr);
        } else {
            ::KillTimer(m_hwnd, TIMER_OSD_BADGE);
            if (wasPersistent && Visible()) {
                // Kalıcı kip kapatıldı: ekranda asılı kalan rozet hemen inmeli,
                // yoksa hiçbir sayaç onu kaldırmayacağı için sonsuza dek kalır.
                HideImmediate();
                return;
            }
        }
    }

    if (m_picking) {
        // Sürükleme sırasında kartı yeniden konumlandırmak elden kaçırırdı;
        // yalnızca görünüm tazelenir, konumu kullanıcının faresi belirler.
        RenderWithDeviceRecovery();
        return;
    }
    if (Visible()) {
        // Yerleşim/opaklık ayarları anında etkili olmalı (spec §4.6).
        PositionForCurrentMonitor();
        RenderWithDeviceRecovery();
    }
}

void OsdWindow::OnThemeChanged(AppTheme theme) {
    m_theme = theme;
    m_renderer.SetTheme(theme, m_config.cardAlpha, m_config.highContrast);
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

// Gösterim (Show / ShowKeyboardLayout / HideImmediate / BeginShow / Present /
// ArmDwellTimer) OsdShow.cpp, kalıcı rozet kipi ise OsdBadge.cpp içindedir
// (400 satır sınırı, spec §9).

}  // namespace kli
