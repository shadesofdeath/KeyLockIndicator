// SettingsPaint.h — Ayarlar penceresinin tema duyarlı zemin/ayırıcı çizimi.
//
// IDD_SETTINGS GROUPBOX kullanmaz: Win32 grup çerçevesi koyu temaya uymaz, her
// koşulda parlak "etched" bir dikdörtgen çizer ve koyu zeminde kalın beyaz bir
// kutu gibi görünür. Yerine Rufus düzeni: IDC_GRP_* kimlikleri düz bölüm başlığı
// (LTEXT) olur ve başlık metninin sağından pencere kenarına kadar uzayan ince
// çizgi burada çizilir.
//
// Ayrı modül: SettingsDialog.cpp 400 satır sınırına dayandı (spec §9 → böl).
#pragma once

#include <windows.h>

namespace kli {
namespace SettingsPaint {

// WM_INITDIALOG: her bölüm başlığı statiğini kendi metninin genişliğine kısaltır.
// Zorunlu — statik kontrol WM_CTLCOLORSTATIC fırçasıyla TÜM istemci alanını
// doldurur, şablon genişliğinde kalırsa başlığın sağındaki çizgiyi silerdi.
// Ayrıca çizginin başlangıcı doğrudan bu daraltılmış sağ kenardan okunur.
void LayoutSectionHeaders(HWND dlg);

// WM_ERASEBKGND: istemci alanını palet zeminiyle doldurur, ardından her bölüm
// başlığının sağına, metnin dikey ortasında 1 piksellik ayırıcı çizer.
void Background(HWND dlg, HDC hdc, bool dark);

// WM_CTLCOLOR* ailesinin tamamı için tek giriş noktası: WinDark'ın koyu tema
// renklerine bölüm başlığı vurgusu ve soluk ipucu metni eklenir. Kullanılacak
// zemin fırçası döner; hiçbir şey geçersiz kılınmadıysa nullptr (çağıran o zaman
// mesajı DefDlgProc'a bırakır).
[[nodiscard]] HBRUSH OnCtlColor(UINT msg, WPARAM wParam, LPARAM lParam, bool dark);

}  // namespace SettingsPaint
}  // namespace kli
