// KeyMonitor.cpp — Kilit tuşu durumu polling'i (spec §4.1).
//
// Polling'in maliyeti tick başına birkaç mikrosaniyedir; hook'un aksine harici
// klavye / KVM / RDP kaynaklı değişimleri de yakalar ve UIPI'ye takılmaz.

#include "KeyMonitor.h"

#include "Messages.h"
#include "Util.h"

#include <utility>

namespace kli {

// GetKeyState'in DÜŞÜK biti "kilit açık", YÜKSEK biti "tuş şu an basılı"
// anlamındadır. Yüksek bit okunursa tuşa basılı tutulduğu sürece durum yanlış
// görünür; bu yüzden yalnızca 0x0001 maskesi kullanılır.
LockState KeyMonitor::ReadHardwareState() noexcept {
    LockState s{};
    s.caps   = (::GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    s.num    = (::GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
    s.scroll = (::GetKeyState(VK_SCROLL) & 0x0001) != 0;
    return s;
}

KeyMonitor::~KeyMonitor() {
    Stop();
}

void KeyMonitor::Start(HWND hostWnd, ChangeCallback cb) {
    // Yeniden başlatma (ayar değişimi) sırasında eski zamanlayıcı kalmasın.
    if (m_running) {
        Stop();
    }

    m_host = hostWnd;
    m_callback = std::move(cb);

    // Açılış durumu okunur ama callback TETİKLENMEZ: aksi hâlde uygulama her
    // açıldığında sebepsiz bir OSD belirir (spec §4.1, kabul kriteri).
    m_state = ReadHardwareState();

    if (m_host == nullptr) {
        LogV(L"KeyMonitor::Start host pencere olmadan çağrıldı");
        return;
    }

    if (::SetTimer(m_host, TIMER_POLL, kPollIntervalMs, nullptr) == 0) {
        LogV(L"SetTimer(TIMER_POLL) başarısız (hata %lu)", ::GetLastError());
        return;
    }

    m_running = true;
}

void KeyMonitor::Stop() {
    if (m_running && m_host != nullptr) {
        ::KillTimer(m_host, TIMER_POLL);
    }
    m_running = false;
    m_host = nullptr;
    // Callback App'i yakalar; durdurulduktan sonra o referans tutulmaz.
    m_callback = nullptr;
}

void KeyMonitor::Tick() {
    // Stop sonrası kuyrukta kalmış bir WM_TIMER sahte OSD çıkarmasın.
    if (!m_running) {
        return;
    }

    const LockState now = ReadHardwareState();
    if (now == m_state) {
        return;
    }

    const LockState prev = m_state;

    // Durum callback'ten ÖNCE yazılır: callback içinde Current() çağrılırsa
    // gösterilen değerle tutarlı olmalı.
    m_state = now;

    if (!m_callback) {
        return;
    }

    // Çağrı yerel kopya üzerinden yapılır: geri çağrı içinden Stop() çağrılırsa
    // m_callback sıfırlanır ve o an çalışan hedef yıkılır — tanımsız davranış.
    // Kopya tek bir işaretçi yakalar, MSVC std::function'ında küçük nesne
    // optimizasyonuna sığar; tahsis maliyeti yoktur.
    const ChangeCallback cb = m_callback;

    // Aynı tick'te birden fazla tuş değişebilir (ör. uykudan dönüş, KVM
    // geçişi). Her biri için ayrı callback; sıra sabit tutulur ki App'in
    // "son geleni göster" davranışı deterministik olsun.
    constexpr LockKey kOrder[kLockKeyCount] = {LockKey::Caps, LockKey::Num, LockKey::Scroll};
    for (const LockKey key : kOrder) {
        if (prev.Get(key) == now.Get(key)) {
            continue;
        }
        cb(key, now);
        // Callback içinden Stop çağrılmış olabilir; kalan tuşları bildirmeyiz.
        if (!m_running) {
            return;
        }
    }
}

void KeyMonitor::Resync() {
    // Oturum değişimi / uykudan dönüş: donanım durumu bizim bilgimizden farklı
    // olabilir. Sessizce hizalanır, callback tetiklenmez (sahte OSD çıkmasın).
    m_state = ReadHardwareState();
}

}  // namespace kli
