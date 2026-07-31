// SettingsStore.h — Ayarların saklandığı yer: kayıt defteri ya da .ini dosyası
// (madde 25 taşınabilir kip, madde 32 içe/dışa aktarma).
//
// TASARIM: sınıf hiyerarşisi YOKTUR. İki arka uç arasındaki fark tek bir enum
// alanıdır; her okuma/yazma fonksiyonu o alana bakıp iki koldan birini seçer.
// Sanal fonksiyon, fabrika, taban sınıf hiçbiri gerekmiyor — arka uç sayısı iki
// ve ikisi de aynı "ad → değer" modelini paylaşıyor.
//
// Depo DURUMSUZDUR: kayıt defteri tarafında da açık bir HKEY tutulmaz, çünkü
// RegGetValueW/RegSetKeyValueW anahtar yolunu doğrudan kabul eder. Böylece aynı
// nesne hem okuma hem yazma için kullanılabilir ve yarım açık anahtar kalmaz.
//
// Aynı kod yolu iki yerde kullanılır (görev tanımının istediği gibi): taşınabilir
// kipte uygulamanın kendi ayar dosyası, dışa/içe aktarmada kullanıcının seçtiği
// dosya. İkisi arasındaki tek fark, dosya yolunun nereden geldiğidir.
#pragma once

#include <string>
#include <vector>

#include <windows.h>

namespace kli {

class SettingsStore {
public:
    enum class Backend {
        Registry,   // HKCU\Software\ShadesOfDeath\KeyLockIndicator
        IniFile,    // GetPrivateProfileStringW / WritePrivateProfileStringW
    };

    // Exe'nin YANINDA KeyLockIndicator.ini varsa taşınabilir kip etkindir.
    // Dosyanın var olması yeter; içi boş olabilir (o hâlde tüm varsayılanlar
    // geçerlidir ve ilk kaydetmede dosya dolar).
    [[nodiscard]] static bool PortableModeActive();

    // Exe'nin yanındaki .ini yolu. Dosya yoksa da yol döner (yazma için gerekli).
    [[nodiscard]] static std::wstring PortableIniPath();

    // Uygulamanın etkin deposu: taşınabilir kipte .ini, değilse kayıt defteri.
    [[nodiscard]] static SettingsStore ForApp();

    // Belirli bir .ini dosyası — dışa/içe aktarma.
    [[nodiscard]] static SettingsStore ForFile(std::wstring path);

    [[nodiscard]] Backend Kind() const noexcept { return m_backend; }
    [[nodiscard]] const std::wstring& Path() const noexcept { return m_path; }

    // --- Okuma -------------------------------------------------------------
    // Değer yoksa, tipi yanlışsa ya da okunamıyorsa out'a DOKUNULMAZ; çağıranın
    // varsayılanı korunur. Dönüş değeri "değer bulundu mu" bilgisidir — çağıran
    // çoğunlukla yok sayar, ama bool okuması buna ihtiyaç duyar (0 hem geçerli
    // bir değer hem de "yok" karşılığı olurdu).
    bool ReadBool(const wchar_t* name, bool& out) const;
    bool ReadUnsigned(const wchar_t* name, unsigned& out) const;
    bool ReadString(const wchar_t* name, std::wstring& out) const;

    // Dize listesi. Kayıt defterinde REG_MULTI_SZ, .ini'de tek satırda '|' ile
    // ayrılmış liste (bkz. .cpp'deki ayraç gerekçesi). Değer yoksa out'a
    // dokunulmaz; değer VARSA ama boşsa out temizlenir (kullanıcı listeyi
    // gerçekten boşaltmış olabilir).
    void ReadList(const wchar_t* name, std::vector<std::wstring>& out) const;

    // --- Yazma -------------------------------------------------------------
    // Yazmadan önce bir kez çağrılır: kayıt defterinde anahtarı, .ini kipinde
    // dosyayı oluşturur. false dönerse hiçbir yazma denenmemelidir.
    [[nodiscard]] bool Prepare() const;

    bool WriteBool(const wchar_t* name, bool value) const;
    bool WriteUnsigned(const wchar_t* name, unsigned value) const;
    bool WriteString(const wchar_t* name, const std::wstring& value) const;
    bool WriteList(const wchar_t* name, const std::vector<std::wstring>& values) const;

    // .ini kipinde önbelleğe alınmış yazmaları diske indirir. Kayıt defterinde
    // etkisizdir. Dışa aktarmadan sonra dosyanın gerçekten dolu olması için şart.
    void Flush() const;

    // .ini'de liste öğelerini ayıran karakter. Windows dosya adlarında yasaktır,
    // bu yüzden exe adlarıyla çakışamaz.
    static constexpr wchar_t kListSeparator = L'|';

private:
    SettingsStore(Backend backend, std::wstring path) noexcept
        : m_backend(backend), m_path(std::move(path)) {}

    Backend m_backend = Backend::Registry;
    std::wstring m_path;   // yalnızca IniFile kipinde doludur
};

}  // namespace kli
