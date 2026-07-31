// LayoutMonitor.cpp — Aktif klavye düzeninin okunması ve adlandırılması (madde 28).
//
// DÜZEN ADI NEREDEN GELİYOR
// GetKeyboardLayoutNameW yalnızca ÇAĞIRAN iş parçacığının düzenini verir; bize
// gereken ise ön plandaki pencerenin düzenidir ve elimizde yalnızca HKL var.
// HKL → KLID dönüşümü doğrudan bir API ile yapılamadığı için düzen listesi
// HKLM\...\Keyboard Layouts altından çözülür:
//   * HKL'in yüksek word'ü 0xFxxx ise bu bir "Layout Id"dir (aynı dil için ikinci
//     bir düzen — Türkçe F, Dvorak, US-International...). Alt anahtarlar taranıp
//     "Layout Id" değeri eşleşen KLID bulunur.
//   * Değilse KLID doğrudan düşük word'ün sekiz haneli hex hâlidir (0000041F).
// Bulunan anahtarda önce "Layout Display Name" (MUI dolaylı dizesi, sistemin
// dilinde) denenir, olmazsa düz "Layout Text" alınır.
//
// Bu yol OLMASAYDI "TR-Q" ile "TR-F" ayırt edilemezdi; ikisinin de dil kimliği
// 0x041F'tir ve yalnızca KLID'leri farklıdır.
#include "LayoutMonitor.h"

#include "Util.h"

#include <shlwapi.h>   // SHLoadIndirectString

#include <cstdlib>   // wcstoul
#include <utility>

namespace kli {
namespace {

constexpr wchar_t kLayoutsKey[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";

// KLID sekiz hex hane + sonlandırıcı.
constexpr DWORD kKlidChars = 9;

[[nodiscard]] std::wstring ReadStringValue(HKEY key, const wchar_t* name) {
    wchar_t buf[512]{};
    DWORD cb = static_cast<DWORD>(sizeof(buf));
    const LSTATUS st =
        ::RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, buf, &cb);
    if (st != ERROR_SUCCESS) {
        return std::wstring();
    }
    buf[_countof(buf) - 1] = L'\0';
    return std::wstring(buf);
}

void FormatKlid(DWORD value, wchar_t (&out)[kKlidChars]) noexcept {
    constexpr wchar_t kHex[] = L"0123456789ABCDEF";
    for (int i = 7; i >= 0; --i) {
        out[i] = kHex[value & 0xFu];
        value >>= 4;
    }
    out[8] = L'\0';
}

// "Layout Id" değeri eşleşen KLID'i arar. Alt anahtar sayısı ~200'dür ve bu
// arama yalnızca düzen DEĞİŞTİĞİNDE bir kez yapılır.
[[nodiscard]] bool FindKlidByLayoutId(WORD layoutId, WORD langId, std::wstring& out) {
    unique_hkey root;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, kLayoutsKey, 0, KEY_ENUMERATE_SUB_KEYS,
                        root.put()) != ERROR_SUCCESS) {
        return false;
    }
    for (DWORD index = 0;; ++index) {
        wchar_t name[kKlidChars]{};
        DWORD nameChars = kKlidChars;
        const LSTATUS st = ::RegEnumKeyExW(root.get(), index, name, &nameChars, nullptr,
                                           nullptr, nullptr, nullptr);
        if (st == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (st != ERROR_SUCCESS) {
            continue;   // ad tampona sığmadı: bizim aradığımız KLID olamaz
        }
        // Aynı dil kimliğine sahip olmayan düzenler baştan elenir; "Layout Id"
        // değerleri diller arasında tekrar edebilir.
        const DWORD klid = ::wcstoul(name, nullptr, 16);
        if (static_cast<WORD>(klid & 0xFFFFu) != langId) {
            continue;
        }
        unique_hkey sub;
        if (::RegOpenKeyExW(root.get(), name, 0, KEY_QUERY_VALUE, sub.put()) !=
            ERROR_SUCCESS) {
            continue;
        }
        const std::wstring idText = ReadStringValue(sub.get(), L"Layout Id");
        if (idText.empty()) {
            continue;
        }
        if (static_cast<WORD>(::wcstoul(idText.c_str(), nullptr, 16)) == layoutId) {
            out.assign(name);
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::wstring KlidFromHkl(HKL hkl) {
    const DWORD raw = static_cast<DWORD>(reinterpret_cast<UINT_PTR>(hkl));
    const WORD low = LOWORD(raw);
    const WORD high = HIWORD(raw);

    if ((high & 0xF000u) == 0xF000u) {
        std::wstring klid;
        if (FindKlidByLayoutId(static_cast<WORD>(high & 0x0FFFu), low, klid)) {
            return klid;
        }
        // Bulunamadı: dilin birincil düzenine düş (ad yaklaşık olur ama boş kalmaz).
    }
    wchar_t buf[kKlidChars]{};
    FormatKlid(low, buf);
    return std::wstring(buf);
}

// "@%SystemRoot%\system32\input.dll,-5091" biçimindeki dolaylı MUI dizesini
// çözer. Düz metin gelirse olduğu gibi döner.
[[nodiscard]] std::wstring ResolveIndirect(const std::wstring& text) {
    if (text.empty() || text[0] != L'@') {
        return text;
    }
    wchar_t resolved[256]{};
    if (SUCCEEDED(::SHLoadIndirectString(text.c_str(), resolved,
                                         static_cast<UINT>(_countof(resolved)), nullptr))) {
        resolved[_countof(resolved) - 1] = L'\0';
        if (resolved[0] != L'\0') {
            return std::wstring(resolved);
        }
    }
    return std::wstring();
}

[[nodiscard]] std::wstring LayoutDisplayName(const std::wstring& klid) {
    if (klid.empty()) {
        return std::wstring();
    }
    std::wstring path(kLayoutsKey);
    path.push_back(L'\\');
    path.append(klid);

    unique_hkey key;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_QUERY_VALUE, key.put()) !=
        ERROR_SUCCESS) {
        return std::wstring();
    }
    const std::wstring display =
        ResolveIndirect(ReadStringValue(key.get(), L"Layout Display Name"));
    if (!display.empty()) {
        return display;
    }
    // Yedek: yerelleştirilmemiş ama daima var olan düz ad ("Turkish Q").
    return ReadStringValue(key.get(), L"Layout Text");
}

// İki harfli ISO 639 kodu, büyük harfe çevrilmiş. Başarısızlıkta boş dize.
[[nodiscard]] std::wstring IsoCodeOf(LANGID langId) {
    wchar_t buf[16]{};
    const LCID lcid = MAKELCID(langId, SORT_DEFAULT);
    if (::GetLocaleInfoW(lcid, LOCALE_SISO639LANGNAME, buf,
                         static_cast<int>(_countof(buf))) == 0) {
        return std::wstring();
    }
    std::wstring code(buf);
    for (wchar_t& ch : code) {
        if (ch >= L'a' && ch <= L'z') {
            ch = static_cast<wchar_t>(ch - L'a' + L'A');
        }
    }
    return code;
}

// Düzen adı hiç bulunamazsa dilin kendi adına düşülür ("Türkçe", "English").
[[nodiscard]] std::wstring NativeLanguageName(LANGID langId) {
    wchar_t buf[128]{};
    const LCID lcid = MAKELCID(langId, SORT_DEFAULT);
    if (::GetLocaleInfoW(lcid, LOCALE_SNATIVELANGUAGENAME, buf,
                         static_cast<int>(_countof(buf))) == 0) {
        return std::wstring();
    }
    return std::wstring(buf);
}

}  // namespace

KeyboardLayoutInfo LayoutMonitor::Read() {
    KeyboardLayoutInfo info;

    // Ön plandaki pencerenin İŞ PARÇACIĞI sorulur: klavye düzeni Windows'ta iş
    // parçacığı başınadır, süreç başına değil.
    DWORD threadId = 0;
    if (const HWND fg = ::GetForegroundWindow()) {
        threadId = ::GetWindowThreadProcessId(fg, nullptr);
    }
    info.hkl = ::GetKeyboardLayout(threadId);
    if (info.hkl == nullptr) {
        return info;
    }

    const LANGID langId =
        static_cast<LANGID>(LOWORD(reinterpret_cast<UINT_PTR>(info.hkl)));
    info.code = IsoCodeOf(langId);
    info.name = LayoutDisplayName(KlidFromHkl(info.hkl));
    if (info.name.empty()) {
        info.name = NativeLanguageName(langId);
    }
    return info;
}

void LayoutMonitor::Start(ChangeCallback cb) {
    m_callback = std::move(cb);
    // Açılış durumu okunur ama callback TETİKLENMEZ (KeyMonitor ile aynı kural).
    m_current = Read();
    m_running = true;
}

void LayoutMonitor::Stop() {
    m_running = false;
    m_callback = nullptr;
}

void LayoutMonitor::Resync() {
    m_current = Read();
}

void LayoutMonitor::Tick() {
    if (!m_running) {
        return;
    }
    const KeyboardLayoutInfo now = Read();
    if (now.hkl == nullptr || now.hkl == m_current.hkl) {
        return;
    }
    // Durum callback'ten ÖNCE yazılır: geri çağrı içinde Current() sorulursa
    // gösterilen değerle tutarlı olmalı.
    m_current = now;
    if (!m_callback) {
        return;
    }
    // Yerel kopya: geri çağrı içinden Stop() çağrılırsa m_callback sıfırlanır ve
    // o an çalışan hedef yıkılırdı (KeyMonitor::Tick ile aynı gerekçe).
    const ChangeCallback cb = m_callback;
    cb(now);
}

}  // namespace kli
