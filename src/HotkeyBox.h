// HotkeyBox.h — Salt okunur bir EDIT kontrolünü "kısayola bas" kutusuna çevirir.
//
// NEDEN comctl32'nin msctls_hotkey32 SINIFI KULLANILMIYOR: o kontrol metnini ve
// zeminini doğrudan GetSysColor(COLOR_WINDOW / COLOR_WINDOWTEXT) ile boyar ve
// ebeveynine WM_CTLCOLOR* SORMAZ. Sonuç, koyu temadaki ayarlar penceresinin
// ortasında bembeyaz bir kutudur — ölçüldü, projenin GROUPBOX'ları atma
// gerekçesiyle (SettingsPaint.h) birebir aynı kusur. Sıradan bir EDIT ise
// WinDark tarafından DarkMode_CFD ile temalandırılır ve WM_CTLCOLORSTATIC'e uyar.
//
// Yaprak modül: yalnızca Win32'ye bakar, Settings'i tanımaz. Değiştiriciler
// RegisterHotKey'in MOD_* biçimindedir; çeviri gerekmez.
#pragma once

#include <windows.h>

namespace kli {
namespace HotkeyBox {

// Kontrolü alt sınıflar (SetWindowSubclass). WM_NCDESTROY'da kendini söker.
void Attach(HWND edit);

// Değeri hem sakladığı yere hem görünen metne yazar; kontrol ebeveynine
// EN_CHANGE bildirir (SetWindowTextW'nin doğal sonucu). vk = 0 → kutu boş.
void Set(HWND edit, unsigned mods, unsigned vk);

[[nodiscard]] unsigned Mods(HWND edit) noexcept;
[[nodiscard]] unsigned Vk(HWND edit) noexcept;

}  // namespace HotkeyBox
}  // namespace kli
