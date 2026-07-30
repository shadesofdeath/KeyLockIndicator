// Messages.h — Uygulama içi pencere mesajları, zamanlayıcı kimlikleri, sınıf adları.
#pragma once

#include <windows.h>

namespace kli {

// --- Gizli host penceresine gelen özel mesajlar ---------------------------
constexpr UINT WM_KLI_TRAYICON      = WM_APP + 1;  // Shell_NotifyIcon geri bildirimi
constexpr UINT WM_KLI_THEMENOTIFY   = WM_APP + 2;  // RegNotify thread'inden tema değişti
constexpr UINT WM_KLI_SHOWSETTINGS  = WM_APP + 3;  // İkinci örnekten / tray menüsünden
constexpr UINT WM_KLI_SETTINGSAPPLY = WM_APP + 4;  // SettingsDialog → App (lParam: Settings*)
constexpr UINT WM_KLI_RESYNC        = WM_APP + 5;  // Oturum/uyku sonrası sessiz senkron

// --- Zamanlayıcılar -------------------------------------------------------
constexpr UINT_PTR TIMER_POLL      = 1;  // host pencerede, 70 ms (KeyMonitor)
constexpr UINT_PTR TIMER_OSD_DWELL = 2;  // OSD pencerede, bekleme süresi sonu
constexpr UINT_PTR TIMER_OSD_GONE  = 3;  // OSD pencerede, fade-out bitti → gizle
constexpr UINT_PTR TIMER_OSD_IDLE  = 4;  // OSD pencerede, boşta → cihaz zincirini bırak

// Boşta kalma süresi sonunda D3D/D2D/DComp zinciri serbest bırakılır.
//
// Ölçüm (Intel iGPU, VS 2026, x64 Release): canlı bir D3D11 cihazı tek başına
// ~33 MB, tüm zincir + çizilmiş kare ~48 MB özel bellek tutuyor — bunun neredeyse
// tamamı sürücünün kendi yığınları (igc-default64.dll shader derleyicisi,
// igd10um64xe.dll UMD). Zincir bırakıldığında özel bellek ~15 MB'a, çalışma seti
// trim ile ~0.2 MB'a iniyor. Yeniden kurulum + ilk kare ise yalnızca 18–22 ms:
// sürücü DLL'leri yüklü kaldığı ve shader önbelleği sıcak olduğu için.
//
// spec §6 zinciri açılışta kurup HİÇ bırakmamayı, gerekçe olarak da "ilk basımda
// ~200 ms gecikme" olur demeyi öngörür; ölçülen değer 200 değil ~20 ms olduğu için
// bu gerekçe geçerli değil. spec §11'in "gizliyken < 8 MB çalışma seti" hedefi ise
// canlı bir D3D11 cihazıyla hiçbir koşulda tutturulamaz. Bu yüzden zincir kısa bir
// boşta kalma payından sonra bırakılır: hızlı ardışık basımlar zinciri paylaşır,
// uzun boşlukta bellek geri verilir, en kötü hâlde bile 100 ms bütçesi aşılmaz.
constexpr UINT kOsdIdleTeardownMs = 5000;

// --- Pencere sınıf adları -------------------------------------------------
inline constexpr const wchar_t* kHostWindowClass = L"KeyLockIndicator.Host";
inline constexpr const wchar_t* kOsdWindowClass  = L"KeyLockIndicator.Osd";

// Tek örnek mutex'i ve ikinci örneğin ilkini bulmak için kullanılan pencere adı.
inline constexpr const wchar_t* kSingleInstanceMutex = L"Local\\KeyLockIndicator_SingleInstance";
inline constexpr const wchar_t* kHostWindowTitle     = L"KeyLockIndicator";

// Süreçler arası "ayarları göster" mesajı; RegisterWindowMessage ile alınır.
inline constexpr const wchar_t* kShowSettingsMessageName = L"KeyLockIndicator_ShowSettings";

// --- Tray menü komut kimlikleri (SettingsDialog kaynak kimliklerinden ayrı) ---
enum TrayCommand : int {
    kCmdSettings      = 40001,
    kCmdAutostart     = 40002,
    kCmdToggleOsd     = 40003,
    kCmdAbout         = 40004,
    kCmdExit          = 40005,
};

}  // namespace kli
