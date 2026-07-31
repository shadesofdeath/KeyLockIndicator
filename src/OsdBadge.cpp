// OsdBadge.cpp — Kalıcı rozet kipi (madde 16).
//
// OsdWindow sınıfının parçasıdır; bölünme yalnızca dosya başına 400 satır sınırı
// içindir (spec §9). OsdWindow.cpp yaşam döngüsünü, OsdDevice.cpp cihaz
// zincirini, OsdAnimation.cpp animasyonları, OsdPosition.cpp konumu, bu dosya da
// "kaybolmayan kart" kipini tutar.
//
// KİPİN SÖZLEŞMESİ
//   * Bekleme (TIMER_OSD_DWELL) ve çıkış (TIMER_OSD_GONE) sayaçları kurulmaz;
//     kart hiçbir zaman kendiliğinden inmez.
//   * Giriş/çıkış animasyonu uygulanmaz. Sürekli görünen bir öğenin her tuş
//     basımında solup belirmesi titreme olarak okunurdu.
//   * Boşta cihaz yıkımı (TIMER_OSD_IDLE) da iptal edilir: kart ekranda
//     durduğu sürece çizim zincirine ihtiyaç vardır.
//   * WS_EX_TRANSPARENT'a DOKUNULMAZ — rozet tıklama geçirgen kalır. (Yalnızca
//     "konumu ekrandan seç" kipi bu stili geçici olarak kaldırır.)
//   * Tam ekran bastırma bu kipte de geçerlidir; kart zaten ekranda olduğu için
//     gösterim anında bir kez sormak yetmez, durum yoklanır.
#include "OsdWindow.h"

#include "Messages.h"
#include "MonitorUtil.h"

namespace kli {

void OsdWindow::ShowPersistent() {
    if (m_hwnd == nullptr) {
        return;
    }
    // Çağıran (Show) içeriği ve cihaz zinciri hazırlığını zaten yaptı; burada
    // yalnızca "kaybolma" mekanizmalarının hepsi kapatılır.
    ::KillTimer(m_hwnd, TIMER_OSD_DWELL);
    ::KillTimer(m_hwnd, TIMER_OSD_GONE);
    ::KillTimer(m_hwnd, TIMER_OSD_IDLE);

    PositionForCurrentMonitor();
    if (!RenderWithDeviceRecovery()) {
        // Çizilemeyen bir kartı göstermek boş/çöp bir dikdörtgen bırakırdı.
        return;
    }
    if (!Visible()) {
        // ShowWindow(SW_SHOW) yerine SetWindowPos: odak ve z-düzeni yan etkisi yok.
        ::SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    SetStaticOpacity(1.0f);
    // Faz "Dwell" kalır: Visible() true döner ve tema/ayar değişimleri kartı
    // anında yeniden çizer. Bu fazdan çıkışı yalnızca HideImmediate sağlar.
    m_phase = Phase::Dwell;
}

void OsdWindow::PollFullscreenSuppression() {
    if (m_hwnd == nullptr) {
        return;
    }
    if (!m_config.persistent) {
        // Kip kapandı ama sayaç bir tur daha attı: kendini söndürür.
        ::KillTimer(m_hwnd, TIMER_OSD_BADGE);
        return;
    }
    if (m_picking) {
        return;   // konum seçme kipi kullanıcının elindedir, bölünmez
    }

    const bool suppress =
        m_config.suppressFullscreen && MonitorUtil::IsPresentationOrFullscreen();
    if (suppress) {
        if (Visible()) {
            // HideImmediate boşta sayacını da kurar: tam ekran oyun sürerken
            // sürücünün ~33 MB'lık yığınları geri verilir (bkz. Messages.h).
            // Rozet sayacı yaşamaya devam eder, dönüşü o yakalar.
            HideImmediate();
        }
        return;
    }
    if (!Visible()) {
        // Tam ekran bitti: rozet aynı içerikle geri gelir. Show, zincir
        // bırakılmışsa yeniden kurar ve ShowPersistent'a yönlendirir.
        Show(m_content.key, m_content.on);
    }
}

}  // namespace kli
