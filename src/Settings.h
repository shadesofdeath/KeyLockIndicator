// Settings.h — HKCU registry oku/yaz (spec §4.6).
#pragma once

#include "LockTypes.h"
#include "Theme.h"

#include <string>

namespace kli {

enum class OsdPositionMode {
    Top = 0,
    Center = 1,
    Bottom = 2,
};

enum class ThemeMode {
    System = 0,
    Light = 1,
    Dark = 2,
};

struct Settings {
    bool showOsd = true;
    bool watchCaps = true;
    bool watchNum = true;
    bool watchScroll = false;
    unsigned osdDurationMs = 1400;               // 800 – 4000
    unsigned osdOpacity = 82;                    // 60 – 100 (yüzde)
    OsdPositionMode osdPosition = OsdPositionMode::Top;
    unsigned osdTopMarginDip = 72;               // 0 – 400
    ThemeMode themeMode = ThemeMode::System;
    bool suppressFullscreen = true;
    bool primaryMonitorOnly = false;
    LockKey trayIconKey = LockKey::Caps;
    std::wstring language = L"auto";              // auto | tr | en

    // Ayar tablosunda olmayan, ama aynı kök altında saklanan durum bilgisi.
    bool oemWarningShown = false;

    // Kök: HKCU\Software\ShadesOfDeath\KeyLockIndicator
    static const wchar_t* RegistryPath() noexcept;

    // Eksik değerler varsayılanla doldurulur; hiçbir hâlde başarısız olmaz.
    [[nodiscard]] static Settings Load();

    // Yalnızca tabloda tanımlı değerleri yazar.
    void Save() const;

    // Aralık dışı değerleri spec §4.6'daki aralıklara çeker.
    void Clamp() noexcept;

    // "Varsayılanlara dön" — oemWarningShown korunur.
    void ResetToDefaults() noexcept;

    [[nodiscard]] bool Watches(LockKey k) const noexcept {
        switch (k) {
            case LockKey::Caps:   return watchCaps;
            case LockKey::Num:    return watchNum;
            case LockKey::Scroll: return watchScroll;
        }
        return false;
    }

    // ThemeMode != System ise sistem teması yok sayılır (spec §4.2).
    [[nodiscard]] AppTheme ResolveTheme(AppTheme systemTheme) const noexcept {
        switch (themeMode) {
            case ThemeMode::Light: return AppTheme::Light;
            case ThemeMode::Dark:  return AppTheme::Dark;
            case ThemeMode::System:
            default:               return systemTheme;
        }
    }

    // Koyu temanın varsayılan kart alfası 0.82, açık temanın 0.85'tir. Kullanıcı
    // varsayılandan saptığında (osdOpacity != 82) ayar değeri her iki temada da
    // aynen uygulanır; dokunulmadıysa temaya özgü varsayılan kullanılır.
    [[nodiscard]] float CardAlphaFor(AppTheme t) const noexcept {
        if (osdOpacity == kDefaultOpacityPercent) {
            return t == AppTheme::Dark ? 0.82f : 0.85f;
        }
        return OpacityPercentToAlpha(osdOpacity);
    }

    static constexpr unsigned kDefaultOpacityPercent = 82;
    static constexpr unsigned kMinDurationMs = 800;
    static constexpr unsigned kMaxDurationMs = 4000;
    static constexpr unsigned kMinOpacity = 60;
    static constexpr unsigned kMaxOpacity = 100;
    static constexpr unsigned kMaxTopMarginDip = 400;
};

// OEM uyarısının gösterildiğini kalıcı işaretler (spec §5).
void MarkOemWarningShown();

}  // namespace kli
