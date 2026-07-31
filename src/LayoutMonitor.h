// LayoutMonitor.h — Aktif klavye düzeni izleyicisi (madde 28).
//
// KENDİ ZAMANLAYICISI YOKTUR ve KENDİ İŞ PARÇACIĞINI AÇMAZ: Tick() App'in zaten
// çalışan 70 ms'lik TIMER_POLL dalından çağrılır. Böylece KeyMonitor'ın
// sözleşmesine hiç dokunulmadan aynı tick'te düzen de okunur.
//
// NEDEN AYRI SINIF: KeyMonitor'ın callback'i (LockKey, LockState) taşır ve
// dönüştürülemez; klavye düzeni ise bambaşka bir olaydır. İkisini tek sınıfa
// sıkıştırmak KeyMonitor'ın sözleşmesini bozardı (spec §2/§9).
#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace kli {

struct KeyboardLayoutInfo {
    HKL hkl = nullptr;
    // İki harfli dil kodu, BÜYÜK harf: "TR", "EN". OSD kartının başlığı budur.
    std::wstring code;
    // Düzenin okunabilir adı: "Türkçe Q", "United States-International".
    // Sistemden gelir (sistemin dilinde) — düzen adları arayüz diline göre
    // çevrilmez, Windows'un dil çubuğu da böyle davranır.
    std::wstring name;

    [[nodiscard]] bool operator==(const KeyboardLayoutInfo& o) const noexcept {
        return hkl == o.hkl;
    }
};

class LayoutMonitor {
public:
    using ChangeCallback = std::function<void(const KeyboardLayoutInfo&)>;

    LayoutMonitor() = default;
    LayoutMonitor(const LayoutMonitor&) = delete;
    LayoutMonitor& operator=(const LayoutMonitor&) = delete;

    // Mevcut düzen okunur ama callback TETİKLENMEZ: açılışta sebepsiz bir OSD
    // çıkması KeyMonitor'daki kuralın aynısıyla engellenir.
    void Start(ChangeCallback cb);
    void Stop();

    // Host pencerenin WM_TIMER / TIMER_POLL dalından, KeyMonitor::Tick ile
    // AYNI tick'te çağrılır.
    void Tick();

    // Oturum değişimi / uykudan dönüş: sessiz senkron, callback tetiklenmez.
    void Resync();

    [[nodiscard]] const KeyboardLayoutInfo& Current() const noexcept { return m_current; }

    // Ön plandaki pencerenin iş parçacığının klavye düzeni. Ön planda pencere
    // yoksa çağıran iş parçacığının düzenine düşer.
    [[nodiscard]] static KeyboardLayoutInfo Read();

private:
    ChangeCallback m_callback;
    KeyboardLayoutInfo m_current{};
    bool m_running = false;
};

}  // namespace kli
