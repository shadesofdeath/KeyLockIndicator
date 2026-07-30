// Localization.h — Çoklu dil (spec §7). Yaprak modül; yalnızca kaynaklara bakar.
//
// LoadStringW yerine RT_STRING blokları dil kimliğiyle doğrudan aranır: böylece
// MUI sıralamasından bağımsız olarak TR/EN seçimi kesinleşir, EN'e düşüş garanti.
#pragma once

#include "LockTypes.h"

#include <windows.h>

#include <string>

namespace kli {
namespace Loc {

// languageSetting: L"auto" | L"tr" | L"en".
// auto → GetUserDefaultUILanguage() primary ID'si LANG_TURKISH ise TR, değilse EN.
void Initialize(HINSTANCE hInstance, const std::wstring& languageSetting);

// Ayar anında değiştiğinde yeniden çağrılır.
void SetLanguage(const std::wstring& languageSetting);

// Bulunamayan kimlik için boş dize döner (asla nullptr değil).
[[nodiscard]] std::wstring Str(UINT id);

// CAPS LOCK / NUM LOCK / SCROLL LOCK — büyük harf başlık metni.
[[nodiscard]] std::wstring KeyTitle(LockKey key);

// Açık / Kapalı
[[nodiscard]] std::wstring StateText(bool on);

// Etkin dilin LANGID'i (tanı amaçlı).
[[nodiscard]] LANGID CurrentLangId();

}  // namespace Loc
}  // namespace kli
