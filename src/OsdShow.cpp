// OsdShow.cpp — OSD kartının gösterilmesi ve gizlenmesi (spec §3.6, §4.4).
//
// OsdWindow sınıfının parçasıdır; bölünme yalnızca dosya başına 400 satır sınırı
// içindir (spec §9). OsdWindow.cpp pencere/yaşam döngüsünü, OsdDevice.cpp cihaz
// zincirini, OsdAnimation.cpp animasyonları, OsdPosition.cpp konumu, OsdBadge.cpp
// kalıcı rozet kipini, bu dosya da gösterim akışını tutar.
//
// AKIŞ: BeginShow (gösterilebilir mi?) → içerik yazımı (çağıranın işi) → Present
// (faz makinesi + bekleme sayacı). Kilit tuşu kartı ile klavye düzeni kartı
// (madde 28) yalnızca ORTADAKİ adımda ayrışır; kalan her şey ortaktır.
#include "OsdWindow.h"

#include "Localization.h"
#include "Messages.h"
#include "MonitorUtil.h"
#include "resource.h"

namespace kli {

bool OsdWindow::BeginShow() {
    if (m_hwnd == nullptr) {
        return false;
    }
    // Konum seçme kipinde kart, kullanıcının sürüklediği örnek karttır: içeriğini
    // değiştirmek ve bekleme sayacı kurmak kipi ortasından bozardı.
    if (m_picking) {
        return false;
    }
    // Tam ekran oyun/sunumda rahatsız etmemek için bastırılır (spec §4.4).
    if (m_config.suppressFullscreen && MonitorUtil::IsPresentationOrFullscreen()) {
        return false;
    }

    // Boşta yıkım sayacı iptal edilir; zincir bırakılmışsa yeniden kurulur.
    // Ölçülen maliyet 18–22 ms, spec §11'in 100 ms bütçesinin çok altında.
    ::KillTimer(m_hwnd, TIMER_OSD_IDLE);
    if (!m_deviceChainValid) {
        if (FAILED(CreateDeviceChain())) {
            return false;
        }
        // Yüzey zincirle birlikte gitti; doğru DPI ile yeniden kurulmalı.
        m_surfaceDpi = 0;
    }
    return true;
}

void OsdWindow::Present() {
    // Kalıcı rozet: animasyon da bekleme sayacı da yok, kart anında güncellenir
    // ve yerinde kalır (madde 16). Aşağıdaki faz makinesi tamamen atlanır.
    if (m_config.persistent) {
        ShowPersistent();
        return;
    }

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

void OsdWindow::Show(LockKey key, bool isOn) {
    if (!BeginShow()) {
        return;
    }
    m_content.keyboardLayout = false;
    m_content.key = key;
    m_content.on = isOn;
    m_content.title = Loc::KeyTitle(key);
    m_content.status = Loc::StateText(isOn);
    Present();
}

void OsdWindow::ShowKeyboardLayout(const std::wstring& code, const std::wstring& name) {
    // Kalıcı rozet kipinde atlanır (bkz. OsdWindow.h'deki gerekçe): rozet tek
    // kart taşır, düzen kartı kilit tuşu göstergesini kalıcı olarak yutardı.
    if (m_config.persistent) {
        return;
    }
    if (!BeginShow()) {
        return;
    }
    m_content.keyboardLayout = true;
    // key/on düzen kartında okunmaz ama bayat kalmasınlar: bir sonraki kilit
    // tuşu gösteriminde Show ikisini de yeniden yazar.
    m_content.on = false;
    m_content.title = code;
    m_content.status = name;
    // Kod okunamadıysa kart başlıksız kalmasın; en azından ne olduğu yazsın.
    if (m_content.title.empty()) {
        m_content.title = Loc::Str(IDS_LAYOUT_TITLE);
    }
    Present();
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

}  // namespace kli
