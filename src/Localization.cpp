// Localization.cpp — Çoklu dil (spec §7).
//
// LoadStringW kullanılmaz. Sebep: LoadStringW, bulunacak dili modülün MUI
// sıralamasına ve iş parçacığı diline göre seçer; app.rc içinde TR ve EN
// tablolarının ikisi de gömülü olduğu için "ayarda EN seçili ama sistem TR"
// durumunda yanlış tablo gelir ve bunu dışarıdan zorlamanın taşınabilir bir
// yolu yoktur. Bunun yerine RT_STRING blokları dil kimliğiyle doğrudan aranır;
// böylece seçim bizde kalır ve EN'e düşüş garanti edilir.
//
// RT_STRING blok düzeni: kaynak adı = id/16 + 1, blok içinde 16 ardışık giriş,
// her giriş [WORD cch][WCHAR text[cch]] (sonlandırıcı YOK), hedef id%16.
#include "Localization.h"

#include "resource.h"

#include <cstring>   // std::memcpy
#include <cwchar>    // _wcsicmp

namespace kli {
namespace Loc {
namespace {

// Yaprak modül önbelleği (spec §9 istisnası): dosya kapsamlı, iç bağlantılı.
// Loc yalnızca ana iş parçacığından çağrılır, kilit gerekmez.
HINSTANCE s_instance = nullptr;
LANGID s_langId = static_cast<LANGID>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));

constexpr unsigned kStringsPerBlock = 16;

constexpr LANGID kEnglishUs =
    static_cast<LANGID>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));

// Desteklenen diller. Sıra Ayarlar'daki açılır listenin sırasıdır: "auto" ilk,
// ardından kodun alfabetik sırası (kullanıcı listeyi böyle tarar).
//
// SUBLANG SEÇİMİ res/strings.rc ile BİREBİR aynı olmak zorundadır; FindResourceEx
// tam LANGID ile arar. pt-BR/pt-PT ve zh-CN/zh-TW çiftleri yalnızca alt dille
// ayrıştığı için bu eşleşme kritiktir.
//
// SAĞDAN SOLA diller (ar/he/fa) BİLEREK YOKTUR: yerleşimin aynalanması
// (WS_EX_LAYOUTRTL + tüm kontrol koordinatlarının çevrilmesi) gerekir ve yarım
// yapılmış bir RTL desteği, hiç olmamasından daha kötü bir deneyim verir.
constexpr Language kLanguages[] = {
    {L"auto",  nullptr,        0},
    {L"cs",    L"Čeština",     MAKELANGID(LANG_CZECH, SUBLANG_CZECH_CZECH_REPUBLIC)},
    {L"da",    L"Dansk",       MAKELANGID(LANG_DANISH, SUBLANG_DANISH_DENMARK)},
    {L"de",    L"Deutsch",     MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN)},
    {L"el",    L"Ελληνικά",    MAKELANGID(LANG_GREEK, SUBLANG_GREEK_GREECE)},
    {L"en",    L"English",     MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)},
    {L"es",    L"Español",     MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH)},
    {L"fi",    L"Suomi",       MAKELANGID(LANG_FINNISH, SUBLANG_FINNISH_FINLAND)},
    {L"fr",    L"Français",    MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH)},
    {L"hu",    L"Magyar",      MAKELANGID(LANG_HUNGARIAN, SUBLANG_HUNGARIAN_HUNGARY)},
    {L"it",    L"Italiano",    MAKELANGID(LANG_ITALIAN, SUBLANG_ITALIAN)},
    {L"ja",    L"日本語",       MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN)},
    {L"ko",    L"한국어",       MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN)},
    {L"nb",    L"Norsk bokmål", MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL)},
    {L"nl",    L"Nederlands",  MAKELANGID(LANG_DUTCH, SUBLANG_DUTCH)},
    {L"pl",    L"Polski",      MAKELANGID(LANG_POLISH, SUBLANG_POLISH_POLAND)},
    {L"pt-BR", L"Português (Brasil)",
                               MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN)},
    {L"pt-PT", L"Português (Portugal)",
                               MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE)},
    {L"ro",    L"Română",      MAKELANGID(LANG_ROMANIAN, SUBLANG_ROMANIAN_ROMANIA)},
    {L"ru",    L"Русский",     MAKELANGID(LANG_RUSSIAN, SUBLANG_RUSSIAN_RUSSIA)},
    {L"sk",    L"Slovenčina",  MAKELANGID(LANG_SLOVAK, SUBLANG_SLOVAK_SLOVAKIA)},
    {L"sv",    L"Svenska",     MAKELANGID(LANG_SWEDISH, SUBLANG_SWEDISH)},
    {L"tr",    L"Türkçe",      MAKELANGID(LANG_TURKISH, SUBLANG_TURKISH_TURKEY)},
    {L"uk",    L"Українська",  MAKELANGID(LANG_UKRAINIAN, SUBLANG_UKRAINIAN_UKRAINE)},
    {L"zh-CN", L"简体中文",     MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)},
    {L"zh-TW", L"繁體中文",     MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL)},
};

constexpr int kLanguageCount = static_cast<int>(_countof(kLanguages));

// Initialize'dan önce Str'ye gelinirse (ör. çok erken bir hata kutusu) boş metin
// göstermek yerine kendi modülümüze düşülür.
[[nodiscard]] HINSTANCE ModuleHandle() noexcept {
    return s_instance != nullptr ? s_instance : ::GetModuleHandleW(nullptr);
}

// Kullanıcının arayüz dilinin tablodaki karşılığı. Önce TAM eşleşme aranır
// (pt-BR ile pt-PT, zh-CN ile zh-TW yalnızca alt dille ayrışıyor), bulunamazsa
// aynı birincil dilin ilk satırına düşülür — "de-AT" kullanıcısı Almanca alsın.
[[nodiscard]] LANGID MatchUiLanguage() noexcept {
    const LANGID ui = ::GetUserDefaultUILanguage();
    for (const Language& lang : kLanguages) {
        if (lang.langId == ui) {
            return lang.langId;
        }
    }
    for (const Language& lang : kLanguages) {
        if (lang.langId != 0 && PRIMARYLANGID(lang.langId) == PRIMARYLANGID(ui)) {
            return lang.langId;
        }
    }
    return kEnglishUs;
}

[[nodiscard]] LANGID ResolveLangId(const std::wstring& setting) noexcept {
    for (const Language& lang : kLanguages) {
        if (lang.langId != 0 && _wcsicmp(setting.c_str(), lang.code) == 0) {
            return lang.langId;
        }
    }
    // "auto" ve tanınmayan her değer kullanıcının arayüz diline bakar.
    return MatchUiLanguage();
}

// Verilen dilin RT_STRING bloğunda id'yi arar. Bulunamazsa out'a dokunmaz.
[[nodiscard]] bool LookupInLanguage(UINT id, LANGID langId, std::wstring& out) {
    const HINSTANCE hInst = ModuleHandle();
    const HRSRC res = ::FindResourceExW(hInst, RT_STRING,
                                        MAKEINTRESOURCEW(id / kStringsPerBlock + 1), langId);
    if (res == nullptr) {
        return false;
    }

    const HGLOBAL block = ::LoadResource(hInst, res);
    if (block == nullptr) {
        return false;
    }

    // LockResource/LoadResource serbest bırakma gerektirmez; kaynaklar modül
    // imajının parçasıdır, bu yüzden RAII sarmalayıcı da yok.
    const void* data = ::LockResource(block);
    const size_t size = static_cast<size_t>(::SizeofResource(hInst, res));
    if (data == nullptr || size == 0) {
        return false;
    }

    const BYTE* const base = static_cast<const BYTE*>(data);
    const unsigned index = id % kStringsPerBlock;
    size_t offset = 0;

    for (unsigned i = 0; i < kStringsPerBlock; ++i) {
        // Bozuk/kısa blokta taşmamak için her adımda sınır kontrolü. Aritmetik
        // ofset üzerinde yapılır; işaretçiyi tamponun ötesine taşımıyoruz.
        if (offset + sizeof(WORD) > size) {
            return false;
        }
        WORD cch = 0;
        std::memcpy(&cch, base + offset, sizeof(cch));   // hizalama varsayımı yapılmıyor
        offset += sizeof(WORD);

        const size_t bytes = static_cast<size_t>(cch) * sizeof(wchar_t);
        if (offset + bytes > size) {
            return false;
        }

        if (i == index) {
            if (cch == 0) {
                // Blok var ama bu girdi bu dilde tanımsız → sıradaki dile düş.
                return false;
            }
            out.assign(reinterpret_cast<const wchar_t*>(base + offset), cch);
            return true;
        }
        offset += bytes;
    }

    return false;
}

}  // namespace

const Language* Languages() noexcept {
    return kLanguages;
}

int LanguageCount() noexcept {
    return kLanguageCount;
}

int LanguageIndex(const std::wstring& code) noexcept {
    for (int i = 0; i < kLanguageCount; ++i) {
        if (_wcsicmp(code.c_str(), kLanguages[i].code) == 0) {
            return i;
        }
    }
    return 0;   // "auto"
}

const wchar_t* LanguageCodeAt(int index) noexcept {
    if (index < 0 || index >= kLanguageCount) {
        return kLanguages[0].code;
    }
    return kLanguages[index].code;
}

bool IsKnownLanguage(const std::wstring& code) noexcept {
    for (const Language& lang : kLanguages) {
        if (_wcsicmp(code.c_str(), lang.code) == 0) {
            return true;
        }
    }
    return false;
}

void Initialize(HINSTANCE hInstance, const std::wstring& languageSetting) {
    s_instance = hInstance;
    s_langId = ResolveLangId(languageSetting);
}

void SetLanguage(const std::wstring& languageSetting) {
    s_langId = ResolveLangId(languageSetting);
}

std::wstring Str(UINT id) {
    // Deneme sırası: tam LANGID → aynı birincil dilin nötr alt dili → EN-US →
    // dil-nötr. Son ikisi, app.rc'de alt dili farklı yazılmış ya da hiç dil
    // etiketi almamış tabloları da yakalar.
    const LANGID candidates[] = {
        s_langId,
        static_cast<LANGID>(MAKELANGID(PRIMARYLANGID(s_langId), SUBLANG_NEUTRAL)),
        static_cast<LANGID>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)),
        static_cast<LANGID>(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)),
    };

    std::wstring out;
    for (const LANGID lang : candidates) {
        if (LookupInLanguage(id, lang, out)) {
            return out;
        }
    }

    // Sözleşme: bulunamayan kimlik için boş dize (asla nullptr / asla hata).
    return out;
}

std::wstring KeyTitle(LockKey key) {
    switch (key) {
        case LockKey::Caps:   return Str(IDS_KEY_CAPS);
        case LockKey::Num:    return Str(IDS_KEY_NUM);
        case LockKey::Scroll: return Str(IDS_KEY_SCROLL);
    }
    return std::wstring();
}

std::wstring StateText(bool on) {
    return Str(on ? IDS_STATE_ON : IDS_STATE_OFF);
}

LANGID CurrentLangId() {
    return s_langId;
}

}  // namespace Loc
}  // namespace kli
