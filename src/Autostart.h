// Autostart.h — Windows ile başlat (spec §4.7).
//
// Portable derleme: HKCU\...\CurrentVersion\Run altında değer.
// MSIX derleme:     AppxManifest windows.startupTask + StartupTask WinRT API'si.
// Hangi yol aktif: derleme zamanı KLI_PACKAGED + çalışma zamanı
// GetCurrentPackageFullName() doğrulaması.
#pragma once

namespace kli {
namespace Autostart {

// Çalışma zamanında paketli (MSIX) çalışıp çalışmadığımız.
[[nodiscard]] bool IsPackaged();

[[nodiscard]] bool IsEnabled();

// Başarılıysa true. Paketli modda kullanıcı Görev Yöneticisi'nden devre dışı
// bıraktıysa false döner (StartupTask::RequestEnableAsync reddedilebilir).
bool SetEnabled(bool enable);

// Kullanıcının sistem tarafında kalıcı olarak devre dışı bıraktığı durum;
// bu hâlde onay kutusu pasifleştirilir.
[[nodiscard]] bool IsDisabledByPolicy();

inline constexpr const wchar_t* kRunValueName = L"KeyLockIndicator";
inline constexpr const wchar_t* kRunKeyPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr const wchar_t* kStartupTaskId = L"KeyLockIndicatorStartup";

}  // namespace Autostart
}  // namespace kli
