// Settings.h — HKCU registry oku/yaz (spec §4.6).
#pragma once

#include "LockTypes.h"
#include "Theme.h"

#include <windows.h>   // MOD_CONTROL / MOD_ALT — kısayol varsayılanı için

#include <string>
#include <vector>

namespace kli {

// OSD'nin aktif monitördeki yeri: 3x3 ızgara (dikey üst/orta/alt x yatay
// sol/orta/sağ).
//
// GERİYE UYUM — SEÇİLEN YOL: "eski değerleri yerinde bırak", GÖÇ KODU YOK.
// Sürüm 1.0'da bu enum yalnızca 0=Üst, 1=Orta, 2=Alt idi ve üçü de YATAY OLARAK
// ORTALIYDI. Yeni ızgarada bu üç konumun karşılığı sırasıyla üst-orta, orta-orta
// ve alt-orta'dır; yani eski 0/1/2 kaydı yeni şemada da BİREBİR AYNI yeri
// gösterir. Bu yüzden Settings::Load'da hiçbir dönüştürme yapılmaz — güncelleyen
// kullanıcının OSD'si tam olarak eskisi gibi çıkar. Altı yeni köşe/kenar konumu
// 3..8, serbest sürüklenen konum ise 9 numaradan devam eder.
//
// Açılır listedeki OKUMA SIRASI bu sayısal sıradan farklıdır (sol üst → sağ alt);
// eşleme SettingsFields.h içindeki kPositionOrder tablosuyla yapılır.
enum class OsdPositionMode {
    TopCenter = 0,        // 1.0'daki "Üst"
    Center = 1,           // 1.0'daki "Orta"
    BottomCenter = 2,     // 1.0'daki "Alt"
    TopLeft = 3,
    TopRight = 4,
    MiddleLeft = 5,
    MiddleRight = 6,
    BottomLeft = 7,
    BottomRight = 8,
    Custom = 9,           // kullanıcının sürükleyip bıraktığı serbest konum
};

enum class ThemeMode {
    System = 0,
    Light = 1,
    Dark = 2,
};

// OSD kartının görünüm kipi (madde 13–15). Sayısal değerler kli::OsdView ile
// BİREBİR aynıdır; çeviriyi App::BuildOsdConfig yapar (spec §2: OsdWindow ve
// OsdRenderer Settings'i tanıyamaz) ve orada static_assert ile kilitlenir.
//
// GERİYE UYUM: 0 = 1.0'ın tek görünümüdür ve VARSAYILANDIR. Kayıt defterinde
// değer yoksa 0 okunur; güncelleyen kullanıcı hiçbir fark görmez.
enum class OsdViewMode {
    IconText = 0,   // ikon + başlık + durum (varsayılan)
    IconOnly = 1,   // yalnızca ikon, küçük kare kart
    Bar = 2,        // minimal çubuk: solda ikon, sağında tek satır
};

// OSD'nin hangi ekran(lar)da çıkacağı.
//
// GERİYE UYUM: 1.0'da bu ayar "PrimaryMonitorOnly" adlı bir DWORD bayraktı
// (0 = imleçteki monitör, 1 = ana monitör). Settings::Load o değeri hâlâ okur ve
// 1 ise Primary'ye GÖÇÜRÜR; Save ise yeni "OsdMonitorTarget" değerinin yanında
// eski bayrağı da yazmaya devam eder, böylece 1.0'a geri dönen kullanıcı ayarını
// kaybetmez. Varsayılan Cursor'dur = eski varsayılan (bayrak 0) ile aynı davranış.
enum class MonitorTarget {
    Cursor = 0,    // imlecin bulunduğu monitör
    Primary = 1,   // her zaman ana monitör
    All = 2,       // tüm monitörlerde aynı anda
};

// Tepsi ikonuna sol tıklamanın eylemi. Hiçbir seçenek kilit tuşunu DEĞİŞTİRMEZ
// (spec §4.5); yalnızca ne gösterileceği seçilir.
enum class TrayClickAction {
    ShowOsd = 0,        // anlık OSD (mevcut ve varsayılan davranış)
    OpenSettings = 1,
    None = 2,
};

struct Settings {
    bool showOsd = true;
    bool watchCaps = true;
    bool watchNum = true;
    bool watchScroll = false;
    unsigned osdDurationMs = 1400;               // 800 – 4000
    unsigned osdOpacity = 82;                    // 60 – 100 (yüzde)
    unsigned osdScalePercent = 100;              // 75 – 150 (kart boyutu yüzdesi)
    OsdPositionMode osdPosition = OsdPositionMode::TopCenter;
    // OsdPositionMode::Custom için: kartın çalışma alanı içindeki oransal konumu
    // (0 = sol/üst kenar, kMaxCustomPos = sağ/alt kenar). Piksel değil oran
    // saklanır ki çözünürlük ya da monitör değişince konum makul yerde kalsın.
    // Varsayılan ORTA'dır ve varsayılan konum kipi Custom olmadığı için mevcut
    // davranışı hiçbir şekilde değiştirmez.
    unsigned osdCustomX = kMaxCustomPos / 2;     // 0 – kMaxCustomPos
    unsigned osdCustomY = kMaxCustomPos / 2;
    unsigned osdTopMarginDip = 72;               // 0 – 400
    OsdViewMode osdViewMode = OsdViewMode::IconText;
    // Kalıcı rozet kipi (madde 16): OSD kaybolmaz, seçilen konumda durup tuş
    // durumunu canlı gösterir. VARSAYILAN KAPALI — açık gelseydi, güncelleyen
    // kullanıcının ekranında bir anda hiç istemediği kalıcı bir kart belirirdi.
    bool persistentBadge = false;
    // Durum değişimini ekran okuyucuya duyur (madde 30). VARSAYILAN KAPALI:
    // ekran okuyucu kullanmayan kullanıcı için görünmez ama boşuna bir iş, ve
    // duyuru API'sinin DLL'i yalnızca bu ayar açıkken yüklenir.
    bool announceToScreenReader = false;
    ThemeMode themeMode = ThemeMode::System;
    bool suppressFullscreen = true;
    MonitorTarget monitorTarget = MonitorTarget::Cursor;
    // true: OSD yalnızca tuş AÇIK konuma geçtiğinde görünür, kapatılırken değil.
    bool showOnlyWhenTurnedOn = false;
    LockKey trayIconKey = LockKey::Caps;
    TrayClickAction trayLeftClick = TrayClickAction::ShowOsd;

    // Global kısayol. VARSAYILAN KAPALIDIR: açık gelen bir kısayol, güncelleyen
    // kullanıcının başka bir uygulamadaki kısayolunu sessizce çalabilirdi.
    // Değiştirici tuşlar MOD_* biçimindedir (RegisterHotKey ile aynı); Ayarlar'daki
    // hotkey kontrolü HOTKEYF_* kullandığı için dönüştürme SettingsFields.h'dedir.
    bool hotkeyEnabled = false;
    unsigned hotkeyMods = MOD_CONTROL | MOD_ALT;
    unsigned hotkeyVk = 'L';
    // "auto" ya da Loc::Languages() tablosundaki bir kod ("tr", "pt-BR", "zh-TW").
    std::wstring language = L"auto";

    // Klavye düzeni değişimini OSD ile bildir (madde 28). VARSAYILAN KAPALI:
    // tek düzenle çalışan kullanıcı için hiç tetiklenmez ama açık gelseydi
    // düzen değiştiren herkes bir anda yeni bir kart görürdü.
    bool watchKeyboardLayout = false;

    // Ön plandayken OSD'nin gösterilmeyeceği uygulamalar (madde 18). Yalnızca
    // exe ADI tutulur ("game.exe"), tam yol değil: aynı program farklı klasörden
    // çalıştırıldığında da kural geçerli olsun. Karşılaştırma büyük/küçük harf
    // duyarsızdır. VARSAYILAN BOŞ — mevcut davranış birebir korunur.
    std::vector<std::wstring> excludedApps;

    // Ayar tablosunda olmayan, ama aynı kök altında saklanan durum bilgisi.
    bool oemWarningShown = false;

    // Kök: HKCU\Software\ShadesOfDeath\KeyLockIndicator
    static const wchar_t* RegistryPath() noexcept;

    // Eksik değerler varsayılanla doldurulur; hiçbir hâlde başarısız olmaz.
    // Taşınabilir kipte (exe'nin yanında KeyLockIndicator.ini varsa) kayıt
    // defteri yerine o dosya okunur — bkz. SettingsStore.
    [[nodiscard]] static Settings Load();

    // Yalnızca tabloda tanımlı değerleri yazar. Depo Load ile aynıdır.
    void Save() const;

    // --- İçe / dışa aktarma (madde 32) --------------------------------------
    // Taşınabilir kiple AYNI biçim ve AYNI kod yolu; tek fark dosya yolunun
    // kullanıcıdan gelmesi. Dışa aktarma tüm ayarları yazar.
    [[nodiscard]] bool ExportToFile(const std::wstring& path) const;

    // Hatalı/eksik dosyada varsayılana düşer ve yine true döner: yarım bir dosya
    // "çökme" değil, "eksik alanlar varsayılan" anlamına gelir. false yalnızca
    // dosya hiç okunamadığında (yok / erişim yok) döner ve çağıran mevcut
    // ayarlarını korur.
    [[nodiscard]] static bool ImportFromFile(const std::wstring& path, Settings& out);

    // Ayarların .ini dosyasından okunduğu taşınabilir kip etkin mi (madde 25).
    [[nodiscard]] static bool PortableMode();

    // Taşınabilir kipteki ayar dosyasının tam yolu (gösterim amaçlı).
    [[nodiscard]] static std::wstring PortableFilePath();

    // Aralık dışı değerleri spec §4.6'daki aralıklara çeker.
    // noexcept DEĞİL: istisna listesi std::vector<std::wstring> tutar ve
    // temizleme sırasında yeniden tahsis edebilir.
    void Clamp();

    // "Varsayılanlara dön" — oemWarningShown korunur.
    void ResetToDefaults();

    // İstisna listesine exe adı ekler (yinelenenler ve boş ad atlanır).
    // true dönerse liste gerçekten değişti.
    bool AddExcludedApp(const std::wstring& exeNameOrPath);

    // Listedeki index'i siler. Aralık dışıysa etkisizdir.
    bool RemoveExcludedApp(size_t index);

    // İstisna listesinin üst sınırı. Sonsuz büyüyen bir ayar hem depoyu hem
    // ön plan kontrolünü gereksiz yere yavaşlatırdı.
    static constexpr size_t kMaxExcludedApps = 64;

    [[nodiscard]] bool Watches(LockKey k) const noexcept {
        switch (k) {
            case LockKey::Caps:   return watchCaps;
            case LockKey::Num:    return watchNum;
            case LockKey::Scroll: return watchScroll;
        }
        return false;
    }

    // ThemeMode != System ise sistem teması yok sayılır (spec §4.2).
    [[nodiscard]] AppTheme ResolveTheme(AppTheme systemTheme) const noexcept {
        switch (themeMode) {
            case ThemeMode::Light: return AppTheme::Light;
            case ThemeMode::Dark:  return AppTheme::Dark;
            case ThemeMode::System:
            default:               return systemTheme;
        }
    }

    // Koyu temanın varsayılan kart alfası 0.82, açık temanın 0.85'tir. Kullanıcı
    // varsayılandan saptığında (osdOpacity != 82) ayar değeri her iki temada da
    // aynen uygulanır; dokunulmadıysa temaya özgü varsayılan kullanılır.
    [[nodiscard]] float CardAlphaFor(AppTheme t) const noexcept {
        if (osdOpacity == kDefaultOpacityPercent) {
            return t == AppTheme::Dark ? 0.82f : 0.85f;
        }
        return OpacityPercentToAlpha(osdOpacity);
    }

    // OSD kartının boyut çarpanı. OsdWindow yüzey boyutunu, OsdRenderer de tüm
    // çizimi bununla ölçekler; metin de ölçeklendiği için net kalır.
    [[nodiscard]] float OsdScale() const noexcept {
        return static_cast<float>(osdScalePercent) / 100.0f;
    }

    static constexpr unsigned kDefaultOpacityPercent = 82;
    static constexpr unsigned kMinDurationMs = 800;
    static constexpr unsigned kMaxDurationMs = 4000;
    static constexpr unsigned kMinOpacity = 60;
    static constexpr unsigned kMaxOpacity = 100;
    static constexpr unsigned kMinScalePercent = 75;
    static constexpr unsigned kMaxScalePercent = 150;
    static constexpr unsigned kMaxTopMarginDip = 400;

    // Izgaradaki en büyük geçerli değer; Load ve Clamp aynı sınırı kullansın.
    static constexpr unsigned kMaxPositionMode =
        static_cast<unsigned>(OsdPositionMode::Custom);

    // Serbest konum oranının ölçeği. OsdWindow.h'deki kCustomRangeMax ile AYNI
    // sayı olmak zorundadır; iki başlık birbirini tanıyamadığı için (spec §2)
    // değer tekrarlanır ve çeviriyi App::BuildOsdConfig yapar.
    static constexpr unsigned kMaxCustomPos = 10000;

    // RegisterHotKey'in kabul ettiği değiştirici bitleri. MOD_NOREPEAT saklanmaz;
    // kayıt anında eklenir (basılı tutmak OSD'yi tekrar tekrar tetiklemesin).
    static constexpr unsigned kHotkeyModMask = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
};

// OEM uyarısının gösterildiğini kalıcı işaretler (spec §5).
void MarkOemWarningShown();

}  // namespace kli
