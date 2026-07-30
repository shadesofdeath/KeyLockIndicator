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

// Initialize'dan önce Str'ye gelinirse (ör. çok erken bir hata kutusu) boş metin
// göstermek yerine kendi modülümüze düşülür.
[[nodiscard]] HINSTANCE ModuleHandle() noexcept {
    return s_instance != nullptr ? s_instance : ::GetModuleHandleW(nullptr);
}

[[nodiscard]] LANGID ResolveLangId(const std::wstring& setting) noexcept {
    if (_wcsicmp(setting.c_str(), L"tr") == 0) {
        return static_cast<LANGID>(MAKELANGID(LANG_TURKISH, SUBLANG_TURKISH_TURKEY));
    }
    if (_wcsicmp(setting.c_str(), L"en") == 0) {
        return static_cast<LANGID>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    }

    // "auto" ve tanınmayan her değer: kullanıcının arayüz dili. Alt dil (tr-TR
    // mi tr-CY mi) önemsizdir, yalnızca birincil kimlik bakılır.
    const LANGID ui = ::GetUserDefaultUILanguage();
    if (PRIMARYLANGID(ui) == LANG_TURKISH) {
        return static_cast<LANGID>(MAKELANGID(LANG_TURKISH, SUBLANG_TURKISH_TURKEY));
    }
    return static_cast<LANGID>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
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
