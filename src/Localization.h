// Localization.h — Çoklu dil (spec §7). Yaprak modül; yalnızca kaynaklara bakar.
//
// LoadStringW yerine RT_STRING blokları dil kimliğiyle doğrudan aranır: böylece
// MUI sıralamasından bağımsız olarak seçim bizde kalır ve EN'e düşüş garanti.
#pragma once

#include "LockTypes.h"

#include <windows.h>

#include <string>

namespace kli {
namespace Loc {

// Desteklenen bir arayüz dili.
//
// TEK DOĞRULUK KAYNAĞI: Ayarlar penceresindeki açılır liste, kaydedilen ayar
// kodunun doğrulaması ve RT_STRING araması için kullanılan LANGID — üçü de bu
// tablodan gelir. Yeni bir dil eklemek için tabloya bir satır ve res/strings.rc
// dosyasına bir STRINGTABLE kümesi eklemek yeterlidir.
//
// endonym ÇEVRİLMEZ: bir dilin kendi adı ("Deutsch", "Русский", "日本語")
// arayüz hangi dilde olursa olsun aynı yazılır. Bu yüzden adlar kaynak
// dizelerinde DEĞİL burada literal olarak durur — aksi hâlde 24 ad x 24 tablo
// = 576 gereksiz çeviri satırı olurdu.
struct Language {
    const wchar_t* code;      // ayar değeri: "auto", "tr", "pt-BR", "zh-TW" ...
    const wchar_t* endonym;   // listedeki ad; nullptr ise IDS_LANG_AUTO kullanılır
    LANGID langId;            // RT_STRING araması; "auto" için 0
};

// İlk öğe DAİMA "auto"dur (görev tanımı: "auto ilk öğe").
[[nodiscard]] const Language* Languages() noexcept;
[[nodiscard]] int LanguageCount() noexcept;

// Tablodaki indeks ⇄ ayar kodu. Bilinmeyen kod 0'a ("auto") düşer.
[[nodiscard]] int LanguageIndex(const std::wstring& code) noexcept;
[[nodiscard]] const wchar_t* LanguageCodeAt(int index) noexcept;

// Kod tabloda var mı (Settings::Clamp doğrulaması). "auto" da geçerlidir.
[[nodiscard]] bool IsKnownLanguage(const std::wstring& code) noexcept;

// languageSetting: L"auto" ya da tablodaki bir kod.
// auto → GetUserDefaultUILanguage() ile en yakın tablo satırı, yoksa EN.
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
