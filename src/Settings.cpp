// Settings.cpp — Ayarların okunması/yazılması (spec §4.6 + madde 25, 32).
//
// Tasarım kararı: okuma hiçbir hâlde başarısız olmaz. Anahtar yoksa, değer
// yoksa, tipi yanlışsa ya da aralık dışıysa varsayılan korunur ve uygulama
// çalışmaya devam eder. Ayar dosyası/anahtarı bozuk diye açılmayan bir tray
// uygulaması kullanıcı için tamamen kullanılamaz hâle gelir.
//
// DEPO SEÇİMİ (madde 25): exe'nin yanında KeyLockIndicator.ini varsa ayarlar o
// dosyadan okunur/oraya yazılır, yoksa HKCU altındaki anahtar kullanılır. Seçimi
// SettingsStore yapar; buradaki alan tablosu iki arka uçta da AYNIDIR — tek bir
// LoadFrom/SaveTo çifti hem taşınabilir kipe hem içe/dışa aktarmaya hizmet eder.
#include "Settings.h"

#include "Localization.h"
#include "SettingsStore.h"
#include "Util.h"

#include <cwchar>   // _wcsicmp

namespace kli {
namespace {

// HKCU'ya göreli kök; SettingsStore.cpp'deki kRegRoot ile aynı yol.
constexpr wchar_t kRoot[] = L"Software\\ShadesOfDeath\\KeyLockIndicator";

// Değer adları tek yerde tanımlı: Load ile Save'in aynı dizeyi kullandığı
// derleyici tarafından garanti edilsin, yazım farkı sessizce ayar kaybettirmesin.
constexpr wchar_t kValShowOsd[]            = L"ShowOsd";
constexpr wchar_t kValWatchCaps[]          = L"WatchCaps";
constexpr wchar_t kValWatchNum[]           = L"WatchNum";
constexpr wchar_t kValWatchScroll[]        = L"WatchScroll";
constexpr wchar_t kValWatchLayout[]        = L"WatchKeyboardLayout";
constexpr wchar_t kValOsdDurationMs[]      = L"OsdDurationMs";
constexpr wchar_t kValOsdOpacity[]         = L"OsdOpacity";
constexpr wchar_t kValOsdScalePercent[]    = L"OsdScalePercent";
constexpr wchar_t kValOsdPosition[]        = L"OsdPosition";
constexpr wchar_t kValOsdCustomX[]         = L"OsdCustomX";
constexpr wchar_t kValOsdCustomY[]         = L"OsdCustomY";
constexpr wchar_t kValOsdTopMarginDip[]    = L"OsdTopMarginDip";
constexpr wchar_t kValOsdViewMode[]        = L"OsdViewMode";
constexpr wchar_t kValPersistentBadge[]    = L"PersistentBadge";
constexpr wchar_t kValAnnounceSr[]         = L"AnnounceToScreenReader";
constexpr wchar_t kValThemeMode[]          = L"ThemeMode";
constexpr wchar_t kValSuppressFullscreen[] = L"SuppressFullscreen";
constexpr wchar_t kValPrimaryMonitorOnly[] = L"PrimaryMonitorOnly";   // 1.0 mirası
constexpr wchar_t kValMonitorTarget[]      = L"OsdMonitorTarget";
constexpr wchar_t kValShowOnlyWhenOn[]     = L"ShowOnlyWhenTurnedOn";
constexpr wchar_t kValTrayIconKey[]        = L"TrayIconKey";
constexpr wchar_t kValTrayLeftClick[]      = L"TrayLeftClickAction";
constexpr wchar_t kValHotkeyEnabled[]      = L"HotkeyEnabled";
constexpr wchar_t kValHotkeyMods[]         = L"HotkeyMods";
constexpr wchar_t kValHotkeyVk[]           = L"HotkeyVk";
constexpr wchar_t kValLanguage[]           = L"Language";
constexpr wchar_t kValExcludedApps[]       = L"ExcludedApps";
constexpr wchar_t kValOemWarningShown[]    = L"OemWarningShown";

// Aralık dışı ham değerin doğrudan enum'a dönüşümü uygulama tanımlıdır; bu
// yüzden önce sınır kontrolü yapılır, geçersizse varsayılana dokunulmaz.
template <typename E>
void ReadEnum(const SettingsStore& store, const wchar_t* name, E& out,
              unsigned maxValue) {
    unsigned raw = 0;
    if (store.ReadUnsigned(name, raw) && raw <= maxValue) {
        out = static_cast<E>(raw);
    }
}

// Yol içeren bir girdiden yalnızca dosya adını alır ("C:\x\game.exe" → "game.exe").
// Kullanıcı dosya seçiciden tam yol getirir; kural ise ada göre çalışır.
[[nodiscard]] std::wstring FileNameOf(const std::wstring& pathOrName) {
    const size_t slash = pathOrName.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return pathOrName;
    }
    return pathOrName.substr(slash + 1);
}

// Tüm alanların okunması. Depo hangisi olursa olsun aynı tablo geçerlidir.
void LoadFrom(const SettingsStore& store, Settings& s) {
    store.ReadBool(kValShowOsd, s.showOsd);
    store.ReadBool(kValWatchCaps, s.watchCaps);
    store.ReadBool(kValWatchNum, s.watchNum);
    store.ReadBool(kValWatchScroll, s.watchScroll);
    store.ReadBool(kValWatchLayout, s.watchKeyboardLayout);
    store.ReadUnsigned(kValOsdDurationMs, s.osdDurationMs);
    store.ReadUnsigned(kValOsdOpacity, s.osdOpacity);
    store.ReadUnsigned(kValOsdScalePercent, s.osdScalePercent);
    // Eski sürümün 0/1/2 değerleri yeni ızgarada da aynı yeri gösterdiği için
    // göç kodu yoktur (bkz. Settings.h OsdPositionMode notu).
    ReadEnum(store, kValOsdPosition, s.osdPosition, Settings::kMaxPositionMode);
    store.ReadUnsigned(kValOsdCustomX, s.osdCustomX);
    store.ReadUnsigned(kValOsdCustomY, s.osdCustomY);
    store.ReadUnsigned(kValOsdTopMarginDip, s.osdTopMarginDip);
    ReadEnum(store, kValOsdViewMode, s.osdViewMode,
             static_cast<unsigned>(OsdViewMode::Bar));
    store.ReadBool(kValPersistentBadge, s.persistentBadge);
    store.ReadBool(kValAnnounceSr, s.announceToScreenReader);
    ReadEnum(store, kValThemeMode, s.themeMode, static_cast<unsigned>(ThemeMode::Dark));
    store.ReadBool(kValSuppressFullscreen, s.suppressFullscreen);

    // Monitör hedefi: önce 1.0'ın bayrağı okunur ve göçürülür, SONRA yeni değer.
    // Sıra önemlidir — yeni değer varsa eskisini geçersiz kılmalıdır, aksi hâlde
    // "Tüm monitörler" seçen kullanıcı her açılışta ana monitöre geri düşerdi.
    bool legacyPrimaryOnly = false;
    store.ReadBool(kValPrimaryMonitorOnly, legacyPrimaryOnly);
    if (legacyPrimaryOnly) {
        s.monitorTarget = MonitorTarget::Primary;
    }
    ReadEnum(store, kValMonitorTarget, s.monitorTarget,
             static_cast<unsigned>(MonitorTarget::All));
    store.ReadBool(kValShowOnlyWhenOn, s.showOnlyWhenTurnedOn);
    ReadEnum(store, kValTrayIconKey, s.trayIconKey, static_cast<unsigned>(LockKey::Scroll));
    ReadEnum(store, kValTrayLeftClick, s.trayLeftClick,
             static_cast<unsigned>(TrayClickAction::None));
    store.ReadBool(kValHotkeyEnabled, s.hotkeyEnabled);
    store.ReadUnsigned(kValHotkeyMods, s.hotkeyMods);
    store.ReadUnsigned(kValHotkeyVk, s.hotkeyVk);
    store.ReadString(kValLanguage, s.language);
    store.ReadList(kValExcludedApps, s.excludedApps);
    store.ReadBool(kValOemWarningShown, s.oemWarningShown);
}

// Tüm alanların yazılması. OemWarningShown BURADA YOK: onun sahibi
// MarkOemWarningShown'dır, ayarlar penceresinden gelen bir kaydetme o durumu
// sıfırlamamalı ya da yeniden yazmamalıdır.
void SaveTo(const SettingsStore& store, const Settings& s) {
    store.WriteBool(kValShowOsd, s.showOsd);
    store.WriteBool(kValWatchCaps, s.watchCaps);
    store.WriteBool(kValWatchNum, s.watchNum);
    store.WriteBool(kValWatchScroll, s.watchScroll);
    store.WriteBool(kValWatchLayout, s.watchKeyboardLayout);
    store.WriteUnsigned(kValOsdDurationMs, s.osdDurationMs);
    store.WriteUnsigned(kValOsdOpacity, s.osdOpacity);
    store.WriteUnsigned(kValOsdScalePercent, s.osdScalePercent);
    store.WriteUnsigned(kValOsdPosition, static_cast<unsigned>(s.osdPosition));
    store.WriteUnsigned(kValOsdCustomX, s.osdCustomX);
    store.WriteUnsigned(kValOsdCustomY, s.osdCustomY);
    store.WriteUnsigned(kValOsdTopMarginDip, s.osdTopMarginDip);
    store.WriteUnsigned(kValOsdViewMode, static_cast<unsigned>(s.osdViewMode));
    store.WriteBool(kValPersistentBadge, s.persistentBadge);
    store.WriteBool(kValAnnounceSr, s.announceToScreenReader);
    store.WriteUnsigned(kValThemeMode, static_cast<unsigned>(s.themeMode));
    store.WriteBool(kValSuppressFullscreen, s.suppressFullscreen);
    store.WriteUnsigned(kValMonitorTarget, static_cast<unsigned>(s.monitorTarget));
    // 1.0 mirası bayrak da yazılmaya devam eder: sürüm düşüren kullanıcı "ana
    // monitör" tercihini kaybetmesin. "Hepsi" 1.0'da yoktu, orada imleç davranışı
    // (0) en yakın karşılıktır.
    store.WriteBool(kValPrimaryMonitorOnly, s.monitorTarget == MonitorTarget::Primary);
    store.WriteBool(kValShowOnlyWhenOn, s.showOnlyWhenTurnedOn);
    store.WriteUnsigned(kValTrayIconKey, static_cast<unsigned>(s.trayIconKey));
    store.WriteUnsigned(kValTrayLeftClick, static_cast<unsigned>(s.trayLeftClick));
    store.WriteBool(kValHotkeyEnabled, s.hotkeyEnabled);
    store.WriteUnsigned(kValHotkeyMods, s.hotkeyMods);
    store.WriteUnsigned(kValHotkeyVk, s.hotkeyVk);
    store.WriteString(kValLanguage, s.language);
    store.WriteList(kValExcludedApps, s.excludedApps);
}

}  // namespace

const wchar_t* Settings::RegistryPath() noexcept {
    return kRoot;
}

bool Settings::PortableMode() {
    return SettingsStore::PortableModeActive();
}

std::wstring Settings::PortableFilePath() {
    return SettingsStore::PortableIniPath();
}

Settings Settings::Load() {
    Settings s{};   // varsayılanlar tek yerde: Settings.h sınıf içi başlangıç değerleri
    LoadFrom(SettingsStore::ForApp(), s);
    // Elle düzenlenmiş aralık dışı değerler burada normalize edilir; çağıranın
    // ayrıca Clamp çağırması gerekmez.
    s.Clamp();
    return s;
}

void Settings::Save() const {
    const SettingsStore store = SettingsStore::ForApp();
    if (!store.Prepare()) {
        return;
    }
    SaveTo(store, *this);
    store.Flush();
}

bool Settings::ExportToFile(const std::wstring& path) const {
    if (path.empty()) {
        return false;
    }
    const SettingsStore store = SettingsStore::ForFile(path);
    if (!store.Prepare()) {
        return false;
    }
    SaveTo(store, *this);
    store.Flush();
    return true;
}

bool Settings::ImportFromFile(const std::wstring& path, Settings& out) {
    if (path.empty()) {
        return false;
    }
    const DWORD attrs = ::GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        LogV(L"Ayar dosyasi okunamadi: %s", path.c_str());
        return false;
    }

    // Dosya BOZUK ya da eksik olabilir: her alan varsayılanından başlar ve
    // yalnızca dosyada geçerli bir karşılığı olan alanlar üzerine yazılır.
    // Yani "eksik dosya" = "varsayılana düş", çökme yok.
    Settings loaded{};
    LoadFrom(SettingsStore::ForFile(path), loaded);
    loaded.Clamp();
    // OEM uyarısı kullanıcı kararıdır ve dosyayla taşınmaz.
    loaded.oemWarningShown = out.oemWarningShown;
    out = loaded;
    return true;
}

bool Settings::AddExcludedApp(const std::wstring& exeNameOrPath) {
    std::wstring name = FileNameOf(exeNameOrPath);
    if (name.empty() || name.size() > MAX_PATH) {
        return false;
    }
    if (excludedApps.size() >= kMaxExcludedApps) {
        LogV(L"Istisna listesi dolu (%zu); '%s' eklenmedi", kMaxExcludedApps, name.c_str());
        return false;
    }
    for (const std::wstring& existing : excludedApps) {
        if (_wcsicmp(existing.c_str(), name.c_str()) == 0) {
            return false;   // zaten var
        }
    }
    excludedApps.push_back(std::move(name));
    return true;
}

bool Settings::RemoveExcludedApp(size_t index) {
    if (index >= excludedApps.size()) {
        return false;
    }
    excludedApps.erase(excludedApps.begin() + static_cast<ptrdiff_t>(index));
    return true;
}

void Settings::Clamp() {
    // kli:: niteliği zorunlu: nitelenmemiş Clamp bu üye fonksiyonun kendisini bulur.
    osdDurationMs    = kli::Clamp(osdDurationMs, kMinDurationMs, kMaxDurationMs);
    osdOpacity       = kli::Clamp(osdOpacity, kMinOpacity, kMaxOpacity);
    osdScalePercent  = kli::Clamp(osdScalePercent, kMinScalePercent, kMaxScalePercent);
    osdTopMarginDip  = kli::Clamp(osdTopMarginDip, 0u, kMaxTopMarginDip);
    osdCustomX       = kli::Clamp(osdCustomX, 0u, kMaxCustomPos);
    osdCustomY       = kli::Clamp(osdCustomY, 0u, kMaxCustomPos);

    // Kısayol: tanınmayan değiştirici bitleri atılır, sanal tuş kodu tek bayta
    // sığmalıdır. Değiştiricisiz ya da tuşsuz bir kısayolun KAYDEDİLMESİ engellenmez
    // (kullanıcı kutuyu boşaltmış olabilir); kaydı App reddeder ve uyarı gösterir.
    hotkeyMods       = hotkeyMods & kHotkeyModMask;
    hotkeyVk         = kli::Clamp(hotkeyVk, 0u, 255u);

    // Konum ızgarası bitişik 0..9 aralığıdır; on ayrı karşılaştırma yerine
    // sınır kontrolü yapılır (yeni bir konum eklenirse kMaxPositionMode büyür).
    if (static_cast<unsigned>(osdPosition) > kMaxPositionMode) {
        osdPosition = OsdPositionMode::TopCenter;
    }

    // Enum'lar: sayısal karşılaştırma yerine tam eşleşme aranıyor; ileride araya
    // yeni bir değer eklenirse bu kontrol sessizce yanlışlanmasın.
    if (themeMode != ThemeMode::System && themeMode != ThemeMode::Light &&
        themeMode != ThemeMode::Dark) {
        themeMode = ThemeMode::System;
    }
    if (trayIconKey != LockKey::Caps && trayIconKey != LockKey::Num &&
        trayIconKey != LockKey::Scroll) {
        trayIconKey = LockKey::Caps;
    }
    if (osdViewMode != OsdViewMode::IconText && osdViewMode != OsdViewMode::IconOnly &&
        osdViewMode != OsdViewMode::Bar) {
        osdViewMode = OsdViewMode::IconText;
    }
    if (monitorTarget != MonitorTarget::Cursor && monitorTarget != MonitorTarget::Primary &&
        monitorTarget != MonitorTarget::All) {
        monitorTarget = MonitorTarget::Cursor;
    }
    if (trayLeftClick != TrayClickAction::ShowOsd &&
        trayLeftClick != TrayClickAction::OpenSettings &&
        trayLeftClick != TrayClickAction::None) {
        trayLeftClick = TrayClickAction::ShowOsd;
    }

    // Tanınmayan dil kodu "auto"ya çekilir. Geçerli kodların tek doğruluk
    // kaynağı Loc::Languages() tablosudur (spec §7).
    if (!Loc::IsKnownLanguage(language)) {
        language = L"auto";
    }

    // İstisna listesi: yol parçaları atılır, boş/yinelenen girdiler ayıklanır,
    // üst sınır uygulanır. Elle düzenlenmiş bir .ini buraya çöp getirebilir.
    std::vector<std::wstring> cleaned;
    cleaned.reserve(excludedApps.size());
    for (const std::wstring& raw : excludedApps) {
        std::wstring name = FileNameOf(raw);
        if (name.empty() || name.size() > MAX_PATH) {
            continue;
        }
        bool duplicate = false;
        for (const std::wstring& kept : cleaned) {
            if (_wcsicmp(kept.c_str(), name.c_str()) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        cleaned.push_back(std::move(name));
        if (cleaned.size() >= kMaxExcludedApps) {
            break;
        }
    }
    excludedApps.swap(cleaned);
}

void Settings::ResetToDefaults() {
    // Varsayılanlar burada tekrar yazılmaz; boş bir Settings'ten kopyalanır.
    Settings d{};
    d.oemWarningShown = oemWarningShown;   // tek seferlik OEM uyarısı tekrar çıkmasın
    *this = d;
}

void MarkOemWarningShown() {
    const SettingsStore store = SettingsStore::ForApp();
    if (!store.Prepare()) {
        return;
    }
    store.WriteBool(kValOemWarningShown, true);
    store.Flush();
}

}  // namespace kli
