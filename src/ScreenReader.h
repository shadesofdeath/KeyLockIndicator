// ScreenReader.h — Durum değişimini ekran okuyucuya duyurur (madde 30).
//
// Yaprak modül: yalnızca windows.h ve std::wstring'e bağlıdır, uygulamanın
// hiçbir tipini tanımaz.
#pragma once

#include <windows.h>

#include <string>

namespace kli {
namespace ScreenReader {

// text'i ekran okuyucuya "şimdi oku" olarak iletir. owner, duyurunun sahibi
// olacak gerçek bir pencere olmalıdır (görünür olması gerekmez).
//
// Duyuru gerçekten yapıldıysa true döner. false; ya hiçbir erişilebilirlik
// istemcisi dinlemiyordur (normal durum, hata değil) ya da API kullanılamıyordur.
[[nodiscard]] bool Announce(HWND owner, const std::wstring& text);

}  // namespace ScreenReader
}  // namespace kli
