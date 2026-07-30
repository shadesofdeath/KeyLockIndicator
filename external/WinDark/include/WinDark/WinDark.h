// WinDark — Win32 pencereleri için koyu tema desteği.
//
// NOT: KEYLOCK_INDICATOR_SPEC.md bu kütüphaneyi bir git submodule olarak
// tanımlar. Belirtilen kaynak (herhangi bir genel "WinDark" deposu) mevcut
// olmadığından, aynı görevi gören bağımlılıksız bir yerel uygulama buraya
// vendor edilmiştir: aynı dizin konumu, aynı rol, aynı çağrı yüzeyi.
// Gerçek submodule bulunursa yalnızca external/WinDark değiştirilir;
// src/ tarafında hiçbir değişiklik gerekmez.
//
// Bağımlılık: yalnızca Win32 + uxtheme + dwmapi (spec §1'deki link listesinde
// zaten var). vcpkg/conan paketi kullanılmaz.
#pragma once

#include <windows.h>

namespace WinDark {

enum class AppMode {
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3,
    Max = 4,
};

// Undocumented uxtheme ordinal'leri bir kez çözülür. Windows 10 1809'dan
// eskisinde tüm çağrılar sessizce no-op olur.
void Initialize();

[[nodiscard]] bool IsSupported();

// HKCU\...\Themes\Personalize okunur (uxtheme ordinal'i olmadan da çalışır).
[[nodiscard]] bool IsSystemDarkMode();

// Uygulama genelinde tercih edilen mod (SetPreferredAppMode / AllowDarkModeForApp).
void SetAppMode(AppMode mode);

// Başlık çubuğu (DWMWA_USE_IMMERSIVE_DARK_MODE) + pencere teması.
void ApplyToWindow(HWND hwnd, bool dark);

// Alt kontrollere sınıflarına uygun koyu tema uygular (button, edit, combobox,
// trackbar, updown, scrollbar). Özyinelemeli.
void ApplyToChildren(HWND parent, bool dark);

// Tema değişimi sonrası çerçevenin yeniden çizilmesi.
void RefreshWindow(HWND hwnd);
void RefreshImmersiveColorPolicyState();

// Menü teması önbelleğini boşaltır (uxtheme ordinal 136). TrackPopupMenu ile
// açılan menülerin koyu temaya geçmesini asıl sağlayan çağrı budur; app modu
// tek başına yalnızca sonraki menü sınıfı kayıtlarını etkiler.
void FlushMenuThemes();

// --- Dialog boyama yardımcıları -------------------------------------------
// WM_CTLCOLORDLG / WM_CTLCOLORSTATIC / WM_CTLCOLORBTN / WM_CTLCOLOREDIT /
// WM_CTLCOLORLISTBOX için tek giriş noktası. Uygun HBRUSH döner ya da nullptr.
[[nodiscard]] HBRUSH OnCtlColor(UINT msg, WPARAM wParam, LPARAM lParam, bool dark);

// Sahiplenilmiş fırça/kalem gibi kaynakları serbest bırakır (uygulama çıkışında).
void ReleaseCachedObjects();

[[nodiscard]] COLORREF TextColor(bool dark);
[[nodiscard]] COLORREF DisabledTextColor(bool dark);
[[nodiscard]] COLORREF BackgroundColor(bool dark);
[[nodiscard]] COLORREF ControlBackgroundColor(bool dark);

}  // namespace WinDark
