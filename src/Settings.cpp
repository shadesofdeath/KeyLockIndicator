// Settings.cpp — HKCU registry oku/yaz (spec §4.6).
//
// Tasarım kararı: okuma hiçbir hâlde başarısız olmaz. Anahtar yoksa, değer
// yoksa, tipi yanlışsa ya da aralık dışıysa varsayılan korunur ve uygulama
// çalışmaya devam eder. Ayar dosyası/anahtarı bozuk diye açılmayan bir tray
// uygulaması kullanıcı için tamamen kullanılamaz hâle gelir.
#include "Settings.h"

#include "Util.h"

#include <cwchar>   // _wcsicmp

namespace kli {
namespace {

// HKCU'ya göreli kök; Autostart::kRunKeyPath ile aynı sözleşme (önek yok).
constexpr wchar_t kRoot[] = L"Software\\ShadesOfDeath\\KeyLockIndicator";

// Değer adları tek yerde tanımlı: Load ile Save'in aynı dizeyi kullandığı
// derleyici tarafından garanti edilsin, yazım farkı sessizce ayar kaybettirmesin.
constexpr wchar_t kValShowOsd[]            = L"ShowOsd";
constexpr wchar_t kValWatchCaps[]          = L"WatchCaps";
constexpr wchar_t kValWatchNum[]           = L"WatchNum";
constexpr wchar_t kValWatchScroll[]        = L"WatchScroll";
constexpr wchar_t kValOsdDurationMs[]      = L"OsdDurationMs";
constexpr wchar_t kValOsdOpacity[]         = L"OsdOpacity";
constexpr wchar_t kValOsdPosition[]        = L"OsdPosition";
constexpr wchar_t kValOsdTopMarginDip[]    = L"OsdTopMarginDip";
constexpr wchar_t kValThemeMode[]          = L"ThemeMode";
constexpr wchar_t kValSuppressFullscreen[] = L"SuppressFullscreen";
constexpr wchar_t kValPrimaryMonitorOnly[] = L"PrimaryMonitorOnly";
constexpr wchar_t kValTrayIconKey[]        = L"TrayIconKey";
constexpr wchar_t kValLanguage[]           = L"Language";
constexpr wchar_t kValOemWarningShown[]    = L"OemWarningShown";

// --- Okuma ------------------------------------------------------------------
// RRF_RT_REG_DWORD tip filtresi, kullanıcının elle REG_SZ yazdığı bir değeri
// sessizce reddeder; bu durumda varsayılan kalır.
[[nodiscard]] bool ReadDword(HKEY key, const wchar_t* name, DWORD& out) noexcept {
    DWORD data = 0;
    DWORD cb = static_cast<DWORD>(sizeof(data));
    const LSTATUS st =
        ::RegGetValueW(key, nullptr, name, RRF_RT_REG_DWORD, nullptr, &data, &cb);
    if (st != ERROR_SUCCESS) {
        return false;
    }
    out = data;
    return true;
}

void ReadBool(HKEY key, const wchar_t* name, bool& out) noexcept {
    DWORD raw = 0;
    if (ReadDword(key, name, raw)) {
        out = (raw != 0);
    }
}

void ReadUnsigned(HKEY key, const wchar_t* name, unsigned& out) noexcept {
    DWORD raw = 0;
    if (ReadDword(key, name, raw)) {
        out = static_cast<unsigned>(raw);
    }
}

// Aralık dışı ham değerin doğrudan enum'a dönüşümü uygulama tanımlıdır; bu
// yüzden önce sınır kontrolü yapılır, geçersizse varsayılana dokunulmaz.
template <typename E>
void ReadEnum(HKEY key, const wchar_t* name, E& out, unsigned maxValue) noexcept {
    DWORD raw = 0;
    if (ReadDword(key, name, raw) && raw <= maxValue) {
        out = static_cast<E>(raw);
    }
}

// Dil kodu en fazla dört karakter ("auto"); daha uzun bir değer bozuk sayılır
// ve RegGetValueW ERROR_MORE_DATA döndürerek varsayılanı korumamızı sağlar.
void ReadLanguage(HKEY key, const wchar_t* name, std::wstring& out) {
    wchar_t buf[32] = {};
    DWORD cb = static_cast<DWORD>(sizeof(buf));
    const LSTATUS st = ::RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, buf, &cb);
    if (st != ERROR_SUCCESS) {
        return;
    }
    buf[_countof(buf) - 1] = L'\0';   // RegGetValueW sonlandırır; sınır yine de kapatılıyor
    out.assign(buf);
}

// --- Yazma ------------------------------------------------------------------
[[nodiscard]] bool OpenForWrite(unique_hkey& out) noexcept {
    const LSTATUS st = ::RegCreateKeyExW(HKEY_CURRENT_USER, kRoot, 0, nullptr,
                                         REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                                         nullptr, out.put(), nullptr);
    if (st != ERROR_SUCCESS) {
        LogV(L"Settings: kayit anahtari yazma icin acilamadi (hata %ld)", st);
        return false;
    }
    return true;
}

void WriteDword(HKEY key, const wchar_t* name, DWORD value) noexcept {
    const LSTATUS st = ::RegSetValueExW(key, name, 0, REG_DWORD,
                                        reinterpret_cast<const BYTE*>(&value),
                                        static_cast<DWORD>(sizeof(value)));
    if (st != ERROR_SUCCESS) {
        LogV(L"Settings: %s yazilamadi (hata %ld)", name, st);
    }
}

void WriteBool(HKEY key, const wchar_t* name, bool value) noexcept {
    WriteDword(key, name, value ? 1u : 0u);
}

void WriteString(HKEY key, const wchar_t* name, const std::wstring& value) noexcept {
    // cbData sonlandırıcıyı içermek zorundadır (REG_SZ sözleşmesi).
    const DWORD cb = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    const LSTATUS st = ::RegSetValueExW(key, name, 0, REG_SZ,
                                        reinterpret_cast<const BYTE*>(value.c_str()), cb);
    if (st != ERROR_SUCCESS) {
        LogV(L"Settings: %s yazilamadi (hata %ld)", name, st);
    }
}

}  // namespace

const wchar_t* Settings::RegistryPath() noexcept {
    return kRoot;
}

Settings Settings::Load() {
    Settings s{};   // varsayılanlar tek yerde: Settings.h sınıf içi başlangıç değerleri

    unique_hkey key;
    const LSTATUS st =
        ::RegOpenKeyExW(HKEY_CURRENT_USER, kRoot, 0, KEY_QUERY_VALUE, key.put());
    if (st != ERROR_SUCCESS) {
        // İlk çalıştırma: anahtar henüz yok. Hata değil, beklenen durum.
        return s;
    }

    ReadBool(key.get(), kValShowOsd, s.showOsd);
    ReadBool(key.get(), kValWatchCaps, s.watchCaps);
    ReadBool(key.get(), kValWatchNum, s.watchNum);
    ReadBool(key.get(), kValWatchScroll, s.watchScroll);
    ReadUnsigned(key.get(), kValOsdDurationMs, s.osdDurationMs);
    ReadUnsigned(key.get(), kValOsdOpacity, s.osdOpacity);
    ReadEnum(key.get(), kValOsdPosition, s.osdPosition,
             static_cast<unsigned>(OsdPositionMode::Bottom));
    ReadUnsigned(key.get(), kValOsdTopMarginDip, s.osdTopMarginDip);
    ReadEnum(key.get(), kValThemeMode, s.themeMode, static_cast<unsigned>(ThemeMode::Dark));
    ReadBool(key.get(), kValSuppressFullscreen, s.suppressFullscreen);
    ReadBool(key.get(), kValPrimaryMonitorOnly, s.primaryMonitorOnly);
    ReadEnum(key.get(), kValTrayIconKey, s.trayIconKey, static_cast<unsigned>(LockKey::Scroll));
    ReadLanguage(key.get(), kValLanguage, s.language);
    ReadBool(key.get(), kValOemWarningShown, s.oemWarningShown);

    // Elle düzenlenmiş aralık dışı değerler burada normalize edilir; çağıranın
    // ayrıca Clamp çağırması gerekmez.
    s.Clamp();
    return s;
}

void Settings::Save() const {
    unique_hkey key;
    if (!OpenForWrite(key)) {
        return;
    }

    // Yalnızca spec §4.6 tablosundaki değerler yazılır. OemWarningShown burada
    // yok: onun sahibi MarkOemWarningShown'dır, ayarlar penceresinden gelen bir
    // kaydetme işlemi o durumu sıfırlamamalı ya da yeniden yazmamalı.
    WriteBool(key.get(), kValShowOsd, showOsd);
    WriteBool(key.get(), kValWatchCaps, watchCaps);
    WriteBool(key.get(), kValWatchNum, watchNum);
    WriteBool(key.get(), kValWatchScroll, watchScroll);
    WriteDword(key.get(), kValOsdDurationMs, static_cast<DWORD>(osdDurationMs));
    WriteDword(key.get(), kValOsdOpacity, static_cast<DWORD>(osdOpacity));
    WriteDword(key.get(), kValOsdPosition, static_cast<DWORD>(osdPosition));
    WriteDword(key.get(), kValOsdTopMarginDip, static_cast<DWORD>(osdTopMarginDip));
    WriteDword(key.get(), kValThemeMode, static_cast<DWORD>(themeMode));
    WriteBool(key.get(), kValSuppressFullscreen, suppressFullscreen);
    WriteBool(key.get(), kValPrimaryMonitorOnly, primaryMonitorOnly);
    WriteDword(key.get(), kValTrayIconKey, static_cast<DWORD>(trayIconKey));
    WriteString(key.get(), kValLanguage, language);
}

void Settings::Clamp() noexcept {
    // kli:: niteliği zorunlu: nitelenmemiş Clamp bu üye fonksiyonun kendisini bulur.
    osdDurationMs   = kli::Clamp(osdDurationMs, kMinDurationMs, kMaxDurationMs);
    osdOpacity      = kli::Clamp(osdOpacity, kMinOpacity, kMaxOpacity);
    osdTopMarginDip = kli::Clamp(osdTopMarginDip, 0u, kMaxTopMarginDip);

    // Enum'lar: sayısal karşılaştırma yerine tam eşleşme aranıyor; ileride araya
    // yeni bir değer eklenirse bu kontrol sessizce yanlışlanmasın.
    if (osdPosition != OsdPositionMode::Top && osdPosition != OsdPositionMode::Center &&
        osdPosition != OsdPositionMode::Bottom) {
        osdPosition = OsdPositionMode::Top;
    }
    if (themeMode != ThemeMode::System && themeMode != ThemeMode::Light &&
        themeMode != ThemeMode::Dark) {
        themeMode = ThemeMode::System;
    }
    if (trayIconKey != LockKey::Caps && trayIconKey != LockKey::Num &&
        trayIconKey != LockKey::Scroll) {
        trayIconKey = LockKey::Caps;
    }

    // Tanınmayan dil kodu "auto"ya çekilir. Kısa literal SSO'ya sığdığı için
    // noexcept bağlamında tahsis riski yoktur.
    if (_wcsicmp(language.c_str(), L"auto") != 0 && _wcsicmp(language.c_str(), L"tr") != 0 &&
        _wcsicmp(language.c_str(), L"en") != 0) {
        language = L"auto";
    }
}

void Settings::ResetToDefaults() noexcept {
    // Varsayılanlar burada tekrar yazılmaz; boş bir Settings'ten kopyalanır.
    Settings d{};
    d.oemWarningShown = oemWarningShown;   // tek seferlik OEM uyarısı tekrar çıkmasın
    *this = d;
}

void MarkOemWarningShown() {
    unique_hkey key;
    if (!OpenForWrite(key)) {
        return;
    }
    WriteDword(key.get(), kValOemWarningShown, 1u);
}

}  // namespace kli
