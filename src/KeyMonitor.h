// KeyMonitor.h — Tuş durumu polling + değişim olayı (spec §4.1).
//
// Hook kullanılmaz: LL klavye hook'u AV yazılımlarında ve Store sertifikasyonunda
// gereksiz risk yaratır, harici klavye/KVM/RDP kaynaklı değişimleri kaçırır ve
// UIPI nedeniyle yükseltilmiş pencerelerde çalışmaz.
#pragma once

#include "LockTypes.h"

#include <windows.h>

#include <functional>

namespace kli {

class KeyMonitor {
public:
    using ChangeCallback = std::function<void(LockKey changed, LockState now)>;

    KeyMonitor() = default;
    KeyMonitor(const KeyMonitor&) = delete;
    KeyMonitor& operator=(const KeyMonitor&) = delete;
    ~KeyMonitor();

    // hostWnd üzerinde TIMER_POLL ile 70 ms aralıklı zamanlayıcı kurar.
    // Mevcut durum okunur ama callback tetiklenmez (açılışta OSD çıkmasın).
    void Start(HWND hostWnd, ChangeCallback cb);
    void Stop();

    // Host pencerenin WM_TIMER / TIMER_POLL dalından çağrılır.
    void Tick();

    // Oturum değişimi / uykudan dönüş sonrası: durumu sessizce senkronlar,
    // callback tetiklemez (sahte OSD çıkmasın).
    void Resync();

    [[nodiscard]] LockState Current() const noexcept { return m_state; }
    [[nodiscard]] bool Running() const noexcept { return m_running; }

    // GetKeyState düşük biti okunur: yüksek bit "basılı", düşük bit "açık".
    [[nodiscard]] static LockState ReadHardwareState() noexcept;

    static constexpr UINT kPollIntervalMs = 70;

private:
    HWND m_host = nullptr;
    ChangeCallback m_callback;
    LockState m_state{};
    bool m_running = false;
};

}  // namespace kli
