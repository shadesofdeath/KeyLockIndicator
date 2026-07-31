// SettingsStore.cpp — kayıt defteri / .ini arka uçları (madde 25, 32).
//
// İki arka uç da aynı sözleşmeyi paylaşır: okuma HİÇBİR hâlde başarısız olmaz,
// yalnızca "değer vardı mı" bilgisini taşır. Bozuk bir ayar dosyası yüzünden
// açılmayan bir tepsi uygulaması kullanıcı için tamamen kullanılamaz olurdu.
#include "SettingsStore.h"

#include "Util.h"

#include <cstdlib>   // wcstoul
#include <utility>

namespace kli {
namespace {

// HKCU'ya göreli kök; Settings.cpp ile AYNI yol olmak zorunda (orada da
// Settings::RegistryPath() ile dışarıya verilir).
constexpr wchar_t kRegRoot[] = L"Software\\ShadesOfDeath\\KeyLockIndicator";

// .ini bölüm adı. Tek bölüm kullanılır: ayar adları zaten benzersiz ve
// kullanıcının dosyayı elle düzenlemesi kolay olsun.
constexpr wchar_t kIniSection[] = L"KeyLockIndicator";

// Taşınabilir kipi tetikleyen dosyanın adı (exe'nin YANINDA).
constexpr wchar_t kPortableIniName[] = L"KeyLockIndicator.ini";

// GetPrivateProfileStringW "değer yok" durumunu ayrı bildirmez; varsayılan
// olarak asla yazılamayacak bir sentinel verilir ve dönen dize onunla
// karşılaştırılır. \x01 bir ayar değerinde bulunamaz.
constexpr wchar_t kMissingSentinel[] = L"\x01";

// Sayısal ve kısa dize değerleri için yeterli; liste değerleri ayrı okunur.
constexpr DWORD kScalarBufChars = 512;

// Liste değeri tamponu: 64 exe adı x ~64 karakter payı. Aşan kısım kırpılır,
// çünkü sınırsız büyüyen bir ayar zaten kullanıcı hatasıdır.
constexpr DWORD kListBufChars = 8192;

[[nodiscard]] std::wstring ExeDirectory() {
    std::wstring path = ModulePath();
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return std::wstring();
    }
    path.resize(slash + 1);   // ayırıcı korunur
    return path;
}

// --- Kayıt defteri ---------------------------------------------------------

[[nodiscard]] bool RegReadDword(const wchar_t* name, DWORD& out) noexcept {
    DWORD data = 0;
    DWORD cb = static_cast<DWORD>(sizeof(data));
    // RRF_RT_REG_DWORD tip filtresi: kullanıcının elle REG_SZ yazdığı bir değer
    // sessizce reddedilir ve varsayılan korunur.
    const LSTATUS st = ::RegGetValueW(HKEY_CURRENT_USER, kRegRoot, name,
                                      RRF_RT_REG_DWORD, nullptr, &data, &cb);
    if (st != ERROR_SUCCESS) {
        return false;
    }
    out = data;
    return true;
}

[[nodiscard]] bool RegWriteValue(const wchar_t* name, DWORD type, const void* data,
                                 DWORD cb) noexcept {
    const LSTATUS st = ::RegSetKeyValueW(HKEY_CURRENT_USER, kRegRoot, name, type, data, cb);
    if (st != ERROR_SUCCESS) {
        LogV(L"SettingsStore: %s yazilamadi (kayit defteri, hata %ld)", name, st);
        return false;
    }
    return true;
}

// --- .ini ------------------------------------------------------------------

[[nodiscard]] bool IniRead(const std::wstring& file, const wchar_t* name, wchar_t* buf,
                           DWORD chars) noexcept {
    const DWORD n = ::GetPrivateProfileStringW(kIniSection, name, kMissingSentinel, buf,
                                               chars, file.c_str());
    if (n == 0) {
        return false;   // boş değer: yok sayılır, varsayılan korunur
    }
    return buf[0] != kMissingSentinel[0];
}

[[nodiscard]] bool IniWrite(const std::wstring& file, const wchar_t* name,
                            const wchar_t* value) noexcept {
    if (::WritePrivateProfileStringW(kIniSection, name, value, file.c_str()) != FALSE) {
        return true;
    }
    LogV(L"SettingsStore: %s yazilamadi (ini, hata %lu)", name, ::GetLastError());
    return false;
}

// Sayı → dize. _snwprintf_s yerine elle: tahsis yok, hata yolu yok.
void UnsignedToText(unsigned value, wchar_t (&buf)[16]) noexcept {
    int i = static_cast<int>(_countof(buf)) - 1;
    buf[i] = L'\0';
    do {
        --i;
        buf[i] = static_cast<wchar_t>(L'0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && i > 0);
    // Baştaki boşluğu kapatmak için sola kaydır.
    int w = 0;
    for (int r = i; buf[r] != L'\0'; ++r, ++w) {
        buf[w] = buf[r];
    }
    buf[w] = L'\0';
}

}  // namespace

// ---------------------------------------------------------------------------
// Kuruluş
// ---------------------------------------------------------------------------

std::wstring SettingsStore::PortableIniPath() {
    const std::wstring dir = ExeDirectory();
    if (dir.empty()) {
        return std::wstring();
    }
    return dir + kPortableIniName;
}

bool SettingsStore::PortableModeActive() {
    // Süreç ömrü boyunca değişmez sayılır: kullanıcı çalışırken .ini'yi yaratıp
    // depoyu ortadan değiştirse, yarısı kayıt defterinde yarısı dosyada bir ayar
    // kümesi oluşurdu. Karar açılışta bir kez verilir.
    static const bool portable = []() noexcept {
        const std::wstring path = PortableIniPath();
        if (path.empty()) {
            return false;
        }
        const DWORD attrs = ::GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES &&
               (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }();
    return portable;
}

SettingsStore SettingsStore::ForApp() {
    if (PortableModeActive()) {
        return SettingsStore(Backend::IniFile, PortableIniPath());
    }
    return SettingsStore(Backend::Registry, std::wstring());
}

SettingsStore SettingsStore::ForFile(std::wstring path) {
    return SettingsStore(Backend::IniFile, std::move(path));
}

// ---------------------------------------------------------------------------
// Okuma
// ---------------------------------------------------------------------------

bool SettingsStore::ReadUnsigned(const wchar_t* name, unsigned& out) const {
    if (m_backend == Backend::Registry) {
        DWORD raw = 0;
        if (!RegReadDword(name, raw)) {
            return false;
        }
        out = static_cast<unsigned>(raw);
        return true;
    }
    wchar_t buf[kScalarBufChars]{};
    if (!IniRead(m_path, name, buf, kScalarBufChars)) {
        return false;
    }
    wchar_t* end = nullptr;
    const unsigned long value = ::wcstoul(buf, &end, 10);
    // Sayı olmayan bir metin ("abc") 0 döndürür; end == buf ise hiç rakam yoktu.
    if (end == buf) {
        return false;
    }
    out = static_cast<unsigned>(value);
    return true;
}

bool SettingsStore::ReadBool(const wchar_t* name, bool& out) const {
    unsigned raw = 0;
    if (!ReadUnsigned(name, raw)) {
        return false;
    }
    out = (raw != 0u);
    return true;
}

bool SettingsStore::ReadString(const wchar_t* name, std::wstring& out) const {
    if (m_backend == Backend::Registry) {
        wchar_t buf[kScalarBufChars]{};
        DWORD cb = static_cast<DWORD>(sizeof(buf));
        const LSTATUS st = ::RegGetValueW(HKEY_CURRENT_USER, kRegRoot, name,
                                          RRF_RT_REG_SZ, nullptr, buf, &cb);
        if (st != ERROR_SUCCESS) {
            return false;
        }
        buf[kScalarBufChars - 1] = L'\0';
        out.assign(buf);
        return true;
    }
    wchar_t buf[kScalarBufChars]{};
    if (!IniRead(m_path, name, buf, kScalarBufChars)) {
        return false;
    }
    out.assign(buf);
    return true;
}

void SettingsStore::ReadList(const wchar_t* name, std::vector<std::wstring>& out) const {
    if (m_backend == Backend::Registry) {
        // REG_MULTI_SZ: art arda sonlandırılmış dizeler, sonunda fazladan bir
        // sonlandırıcı. RegGetValueW eksik sonlandırıcıyı kendisi tamamlar.
        std::vector<wchar_t> buf(kListBufChars);
        DWORD cb = static_cast<DWORD>(buf.size() * sizeof(wchar_t));
        const LSTATUS st = ::RegGetValueW(HKEY_CURRENT_USER, kRegRoot, name,
                                          RRF_RT_REG_MULTI_SZ, nullptr, buf.data(), &cb);
        if (st != ERROR_SUCCESS) {
            return;
        }
        out.clear();
        const size_t chars = cb / sizeof(wchar_t);
        size_t i = 0;
        while (i < chars && buf[i] != L'\0') {
            const size_t start = i;
            while (i < chars && buf[i] != L'\0') {
                ++i;
            }
            out.emplace_back(buf.data() + start, i - start);
            if (i < chars) {
                ++i;   // sonlandırıcıyı atla
            }
        }
        return;
    }

    std::vector<wchar_t> buf(kListBufChars);
    if (!IniRead(m_path, name, buf.data(), static_cast<DWORD>(buf.size()))) {
        return;
    }
    out.clear();
    const std::wstring joined(buf.data());
    size_t start = 0;
    while (start <= joined.size()) {
        const size_t sep = joined.find(kListSeparator, start);
        const size_t end = (sep == std::wstring::npos) ? joined.size() : sep;
        if (end > start) {
            out.emplace_back(joined, start, end - start);
        }
        if (sep == std::wstring::npos) {
            break;
        }
        start = sep + 1;
    }
}

// ---------------------------------------------------------------------------
// Yazma
// ---------------------------------------------------------------------------

bool SettingsStore::Prepare() const {
    if (m_backend == Backend::Registry) {
        unique_hkey key;
        const LSTATUS st =
            ::RegCreateKeyExW(HKEY_CURRENT_USER, kRegRoot, 0, nullptr,
                              REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                              key.put(), nullptr);
        if (st != ERROR_SUCCESS) {
            LogV(L"SettingsStore: kayit anahtari acilamadi (hata %ld)", st);
            return false;
        }
        return true;
    }
    if (m_path.empty()) {
        return false;
    }
    // WritePrivateProfileStringW dosyayı kendisi oluşturur; yol geçerliyse ilk
    // yazma başarılı olur. Yine de burada bir kez denenir ki dışa aktarmada
    // "yazılamıyor" hatası kullanıcıya ilk anda bildirilebilsin.
    return IniWrite(m_path, L"Format", L"1");
}

bool SettingsStore::WriteUnsigned(const wchar_t* name, unsigned value) const {
    if (m_backend == Backend::Registry) {
        const DWORD dw = static_cast<DWORD>(value);
        return RegWriteValue(name, REG_DWORD, &dw, static_cast<DWORD>(sizeof(dw)));
    }
    wchar_t buf[16]{};
    UnsignedToText(value, buf);
    return IniWrite(m_path, name, buf);
}

bool SettingsStore::WriteBool(const wchar_t* name, bool value) const {
    return WriteUnsigned(name, value ? 1u : 0u);
}

bool SettingsStore::WriteString(const wchar_t* name, const std::wstring& value) const {
    if (m_backend == Backend::Registry) {
        // cbData sonlandırıcıyı İÇERMEK zorundadır (REG_SZ sözleşmesi).
        const DWORD cb = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        return RegWriteValue(name, REG_SZ, value.c_str(), cb);
    }
    return IniWrite(m_path, name, value.c_str());
}

bool SettingsStore::WriteList(const wchar_t* name,
                              const std::vector<std::wstring>& values) const {
    if (m_backend == Backend::Registry) {
        // REG_MULTI_SZ: her öğe kendi sonlandırıcısıyla, sonda fazladan bir tane.
        // BOŞ liste tek bir sonlandırıcıyla yazılır — sıfır baytlık bir değer
        // bazı API'lerde "bozuk" sayılır.
        std::vector<wchar_t> blob;
        for (const std::wstring& item : values) {
            blob.insert(blob.end(), item.begin(), item.end());
            blob.push_back(L'\0');
        }
        blob.push_back(L'\0');
        const DWORD cb = static_cast<DWORD>(blob.size() * sizeof(wchar_t));
        return RegWriteValue(name, REG_MULTI_SZ, blob.data(), cb);
    }

    std::wstring joined;
    for (const std::wstring& item : values) {
        if (!joined.empty()) {
            joined.push_back(kListSeparator);
        }
        joined.append(item);
    }
    return IniWrite(m_path, name, joined.c_str());
}

void SettingsStore::Flush() const {
    if (m_backend != Backend::IniFile) {
        return;
    }
    // Profil API'si yazmaları önbellekler; nullptr'lı çağrı önbelleği diske
    // indirir. Dışa aktarmadan hemen sonra dosyanın dolu olması buna bağlıdır.
    (void)::WritePrivateProfileStringW(nullptr, nullptr, nullptr, m_path.c_str());
}

}  // namespace kli
