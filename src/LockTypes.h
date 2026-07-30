// LockTypes.h — Kilit tuşu tipleri.
//
// Spec §4.1 bu tipleri KeyMonitor.h içinde gösterir; ancak spec §2'deki
// "OsdWindow, KeyMonitor'ı tanımaz" kuralı ihlal edilemez olduğu için tipler
// bağımsız bir yaprak başlığa taşındı. KeyMonitor.h bu başlığı include eder,
// dolayısıyla spec'teki bildirim yüzeyi korunur; bağımlılık yönü de bozulmaz.
#pragma once

#include <cstddef>

namespace kli {

enum class LockKey {
    Caps = 0,
    Num = 1,
    Scroll = 2,
};

inline constexpr size_t kLockKeyCount = 3;

struct LockState {
    bool caps = false;
    bool num = false;
    bool scroll = false;

    [[nodiscard]] bool Get(LockKey k) const noexcept {
        switch (k) {
            case LockKey::Caps:   return caps;
            case LockKey::Num:    return num;
            case LockKey::Scroll: return scroll;
        }
        return false;
    }

    void Set(LockKey k, bool v) noexcept {
        switch (k) {
            case LockKey::Caps:   caps = v; break;
            case LockKey::Num:    num = v; break;
            case LockKey::Scroll: scroll = v; break;
        }
    }

    [[nodiscard]] bool operator==(const LockState& o) const noexcept {
        return caps == o.caps && num == o.num && scroll == o.scroll;
    }
};

}  // namespace kli
