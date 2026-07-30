// OemCheck.h — OEM çakışma kontrolü (spec §5).
//
// ASUS/Lenovo/HP/Acer/Dell kendi OSD ajanlarını önyüklü gönderir; ikisi aynı
// anda çıkarsa kullanıcı uygulamayı bozuk sanır. İlk çalıştırmada tek seferlik
// bilgilendirme gösterilir.
#pragma once

#include <windows.h>

#include <string>

namespace kli {
namespace OemCheck {

struct Result {
    bool found = false;
    std::wstring vendor;      // "ASUS" / "Lenovo" / "HP" / "Acer" / "Dell"
    std::wstring processName; // Eşleşen proses adı
};

// CreateToolhelp32Snapshot ile proses adları taranır; büyük/küçük harf
// duyarsız, kısmi eşleşme.
[[nodiscard]] Result Detect();

// Eşleşme varsa TaskDialog (doğrulama onay kutulu) gösterir ve kullanıcı
// "Bir daha gösterme" işaretlediyse kalıcı olarak işaretler.
// alreadyShown true ise hiçbir şey yapmaz.
void WarnOnce(HWND owner, bool alreadyShown);

}  // namespace OemCheck
}  // namespace kli
