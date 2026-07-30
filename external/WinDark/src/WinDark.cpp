// WinDark.cpp — Windows 10 1809+ koyu tema API'lerinin bağımlılıksız sarmalayıcısı.
//
// Buradaki uxtheme fonksiyonlarının hiçbiri belgelenmemiştir; yalnızca ordinal
// ile ihraç edilirler, ada göre çözüm mümkün değildir. Bu yüzden her çağrı
// "çözülemedi → sessizce no-op" ilkesiyle korunur: eksik bir ordinal uygulamayı
// çalışmaz hâle getirmez, yalnızca pencere açık temada kalır.
//
// Not: bu hedef src/ altını görmez — Util.h'deki HR()/ComPtr yardımcıları burada
// kullanılamaz; WinDark.h bağımlılığı "yalnızca Win32 + uxtheme + dwmapi" olarak
// sabitler. Dönüş değeri yok sayılan çağrılar bilerek sessizdir: tema uygulaması
// "iyimser" bir işlemdir, başarısızlığı akışı kesmemelidir.

#include "WinDark/WinDark.h"

#include <dwmapi.h>
#include <uxtheme.h>

#include <cwchar>  // _wcsicmp

namespace WinDark {
namespace {

// --- Sürüm eşikleri ---------------------------------------------------------
// Koyu tema API'lerinin geldiği ilk derleme (Windows 10 1809). Üst sınır yok.
constexpr DWORD kMinBuild = 17763;
// 1903'te aynı ordinal (135) AllowDarkModeForApp yerine SetPreferredAppMode
// oldu; imzaları uyumsuz olduğundan derlemeye göre ayrışmak zorunludur.
constexpr DWORD kPreferredAppModeBuild = 18362;
// 19H2'ye kadar DWMWA_USE_IMMERSIVE_DARK_MODE 19 numaralıydı, sonra 20 oldu.
constexpr DWORD kDwmAttributeStableBuild = 18985;

// SDK sürümüne göre bu sabitler dwmapi.h'de olabilir de olmayabilir de —
// numarayı kendimiz tanımlayıp DWORD aşırı yüklemesine geçiyoruz.
constexpr DWORD kDwmUseImmersiveDarkMode = 20;
constexpr DWORD kDwmUseImmersiveDarkModeLegacy = 19;

// --- uxtheme ordinal'leri ---------------------------------------------------
constexpr WORD kOrdRefreshImmersiveColorPolicyState = 104;
constexpr WORD kOrdAllowDarkModeForWindow = 133;
constexpr WORD kOrdAppMode = 135;
constexpr WORD kOrdFlushMenuThemes = 136;

// Gerçek imzalar: 1 baytlık `bool` döndürürler, `BOOL` değil. Dönüş değerini
// kullanmıyoruz ama imzayı doğru yazmak çağrı sözleşmesini bozmamak için önemli.
using FnAllowDarkModeForWindow = bool(WINAPI*)(HWND, BOOL);
using FnAllowDarkModeForApp = bool(WINAPI*)(BOOL);
using FnSetPreferredAppMode = int(WINAPI*)(int);
using FnRefreshImmersiveColorPolicyState = void(WINAPI*)();
using FnFlushMenuThemes = void(WINAPI*)();
using FnRtlGetNtVersionNumbers = void(NTAPI*)(DWORD*, DWORD*, DWORD*);

// Modül durumu. Spec §9 genel değişkeni yasaklar; WinDark yaprak bir modül olduğu
// için fonksiyon-içi static önbellek serbesttir (tek noktadan erişim, sıfır-başlatma).
struct State {
    bool initialized = false;
    bool supported = false;
    DWORD build = 0;
    FnAllowDarkModeForWindow allowDarkModeForWindow = nullptr;
    FnAllowDarkModeForApp allowDarkModeForApp = nullptr;
    FnSetPreferredAppMode setPreferredAppMode = nullptr;
    FnRefreshImmersiveColorPolicyState refreshColorPolicy = nullptr;
    FnFlushMenuThemes flushMenuThemes = nullptr;
    HBRUSH backgroundBrush = nullptr;
    HBRUSH controlBrush = nullptr;
};

[[nodiscard]] State& S() noexcept {
    static State state;
    return state;
}

// GetProcAddress ordinal biçimi: yüksek word'ü sıfır olan "sahte" işaretçi.
// MAKEINTRESOURCEA yerine bu, `-A` sonlu sembole dokunmadan aynı işi yapar (§9).
[[nodiscard]] LPCSTR Ordinal(WORD n) noexcept {
    return reinterpret_cast<LPCSTR>(static_cast<ULONG_PTR>(n));
}

// GetVersionEx manifest'e göre kırpılmış değer döndürüp koyu tema eşiklerini yanlış
// tarafa düşürür; RtlGetNtVersionNumbers çekirdekten gerçek numarayı verir.
[[nodiscard]] DWORD QueryBuildNumber() noexcept {
    const HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll == nullptr) {
        return 0;
    }
    const auto fn = reinterpret_cast<FnRtlGetNtVersionNumbers>(
        ::GetProcAddress(ntdll, "RtlGetNtVersionNumbers"));
    if (fn == nullptr) {
        return 0;
    }

    DWORD major = 0;
    DWORD minor = 0;
    DWORD build = 0;
    fn(&major, &minor, &build);

    // Üst nibble'lar bayrak taşır (0xF0000000), derleme numarası alt bitlerdedir.
    build &= 0x0FFFFFFFu;
    if (major < 10) {
        return 0;  // Windows 10 öncesi: koyu tema yok, eşik kontrolü gereksiz.
    }
    return build;
}

// Koyu tema alt-sınıf adı. nullptr, daha önce yazılmış tema geçersiz kılmasını
// siler ve kontrolü sistem temasına geri döndürür.
[[nodiscard]] const wchar_t* DarkThemeName(bool dark) noexcept {
    return dark ? L"DarkMode_Explorer" : nullptr;
}

// Fırçalar tembel oluşturulur (açık temada hiç); temizlik ReleaseCachedObjects'te.
[[nodiscard]] HBRUSH CachedBrush(HBRUSH& slot, COLORREF color) noexcept {
    if (slot == nullptr) {
        slot = ::CreateSolidBrush(color);
    }
    return slot;
}

// Tek bir kontrole sınıfına uygun koyu temayı uygular.
void ApplyToControl(HWND hwnd, bool dark) noexcept {
    constexpr int kClassNameMax = 64;
    wchar_t cls[kClassNameMax] = {};
    if (::GetClassNameW(hwnd, cls, kClassNameMax) == 0) {
        return;
    }

    // CFD ("combobox / edit field") koyu KENARLIK veren tema sınıfıdır.
    // DarkMode_Explorer bu iki sınıfta gövdeyi koyultur ama çerçeveyi parlak
    // bırakır — kullanıcının "kalın beyaz border" olarak gördüğü şey buydu.
    const bool isCfd = _wcsicmp(cls, L"ComboBox") == 0 || _wcsicmp(cls, L"Edit") == 0;
    const bool isCommonControl = isCfd || _wcsicmp(cls, L"Button") == 0 ||
                                 _wcsicmp(cls, L"ScrollBar") == 0 ||
                                 _wcsicmp(cls, L"ListBox") == 0;
    const bool isSliderOrSpin = _wcsicmp(cls, L"msctls_trackbar32") == 0 ||
                                _wcsicmp(cls, L"msctls_updown32") == 0;

    if (isCommonControl) {
        // Bu sınıflar kendi koyu çizimlerini yalnızca pencere başına izin
        // verildiğinde açar; tema adı tek başına yetmez.
        if (S().allowDarkModeForWindow != nullptr) {
            S().allowDarkModeForWindow(hwnd, dark ? TRUE : FALSE);
        }
        const wchar_t* theme = dark ? (isCfd ? L"DarkMode_CFD" : L"DarkMode_Explorer") : nullptr;
        ::SetWindowTheme(hwnd, theme, nullptr);
    } else if (isSliderOrSpin) {
        ::SetWindowTheme(hwnd, DarkThemeName(dark), nullptr);
    }

    // Kontrol, tema değişimini ancak bu mesajla önbelleğine yansıtır.
    ::SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
}

BOOL CALLBACK ApplyChildProc(HWND hwnd, LPARAM lParam) {
    const bool dark = (lParam != 0);
    ApplyToControl(hwnd, dark);
    // EnumChildWindows torunları da gezer; bu özyineleme yalnızca enumerasyon
    // sırasında yeniden parent'lanan kontroller için güvence (işlemler idempotent).
    ::EnumChildWindows(hwnd, ApplyChildProc, lParam);
    return TRUE;
}

}  // namespace

// ---------------------------------------------------------------------------
// Başlatma
// ---------------------------------------------------------------------------
void Initialize() {
    State& s = S();
    if (s.initialized) {
        return;
    }
    // Başarısızlıkta da bir daha denenmez: eksik ordinal kalıcı bir durumdur.
    s.initialized = true;

    s.build = QueryBuildNumber();
    if (s.build < kMinBuild) {
        return;
    }

    // uxtheme.dll statik import edilir (SetWindowTheme burada kullanılıyor), yani
    // süreçte zaten yüklü — LoadLibrary ile ek referans tutmaya gerek yok.
    const HMODULE ux = ::GetModuleHandleW(L"uxtheme.dll");
    if (ux == nullptr) {
        return;
    }

    s.allowDarkModeForWindow = reinterpret_cast<FnAllowDarkModeForWindow>(
        ::GetProcAddress(ux, Ordinal(kOrdAllowDarkModeForWindow)));
    s.refreshColorPolicy = reinterpret_cast<FnRefreshImmersiveColorPolicyState>(
        ::GetProcAddress(ux, Ordinal(kOrdRefreshImmersiveColorPolicyState)));
    // FlushMenuThemes destek ölçütüne girmez: çözülemezse menüler açık temada
    // kalır ama pencere/kontrol temalandırması bundan etkilenmez.
    s.flushMenuThemes = reinterpret_cast<FnFlushMenuThemes>(
        ::GetProcAddress(ux, Ordinal(kOrdFlushMenuThemes)));

    const FARPROC appMode = ::GetProcAddress(ux, Ordinal(kOrdAppMode));
    if (s.build < kPreferredAppModeBuild) {
        s.allowDarkModeForApp = reinterpret_cast<FnAllowDarkModeForApp>(appMode);
    } else {
        s.setPreferredAppMode = reinterpret_cast<FnSetPreferredAppMode>(appMode);
    }

    // ShouldAppsUseDarkMode (132) ne çözülür ne şart sayılır: sistem temasını kayıt
    // defterinden okuyoruz. Destek ölçüsü yalnızca gerçekten çağırdığımız işlevler.
    s.supported = s.allowDarkModeForWindow != nullptr && s.refreshColorPolicy != nullptr &&
                  (s.setPreferredAppMode != nullptr || s.allowDarkModeForApp != nullptr);
}

bool IsSupported() {
    Initialize();
    return S().supported;
}

bool IsSystemDarkMode() {
    // Kayıt defteri okuması her sürümde çalışır ve ThemeWatcher ile aynı kaynaktır.
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                        KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD value = 1;  // Değer yoksa Windows açık tema varsayar.
    DWORD type = 0;
    DWORD size = static_cast<DWORD>(sizeof(value));
    const LSTATUS status = ::RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type,
                                              reinterpret_cast<LPBYTE>(&value), &size);
    ::RegCloseKey(key);

    return status == ERROR_SUCCESS && type == REG_DWORD && value == 0;
}

void SetAppMode(AppMode mode) {
    Initialize();
    const State& s = S();
    if (!s.supported) {
        return;
    }

    if (s.setPreferredAppMode != nullptr) {
        s.setPreferredAppMode(static_cast<int>(mode));
    } else if (s.allowDarkModeForApp != nullptr) {
        // 1809'da yalnızca ikili bir anahtar var; Force* modları "izin ver"e
        // indirgenir, gerçek zorlama pencere başına ApplyToWindow ile yapılır.
        const bool wantDark = (mode == AppMode::AllowDark || mode == AppMode::ForceDark);
        s.allowDarkModeForApp(wantDark ? TRUE : FALSE);
    }

    // Politika durumu tazelenmeden yeni mod yalnızca sonraki pencerelere işler.
    if (s.refreshColorPolicy != nullptr) {
        s.refreshColorPolicy();
    }
    // Menü tema önbelleği ayrı tutulur: boşaltılmazsa TrackPopupMenu yeni moddan
    // habersiz kalır ve tepsi menüsü açık temada çizilir.
    if (s.flushMenuThemes != nullptr) {
        s.flushMenuThemes();
    }
}

void RefreshImmersiveColorPolicyState() {
    Initialize();
    const State& s = S();
    if (s.refreshColorPolicy != nullptr) {
        s.refreshColorPolicy();
    }
}

void FlushMenuThemes() {
    Initialize();
    const State& s = S();
    if (s.flushMenuThemes != nullptr) {
        s.flushMenuThemes();
    }
}

// ---------------------------------------------------------------------------
// Pencere / kontrol uygulaması
// ---------------------------------------------------------------------------
void ApplyToWindow(HWND hwnd, bool dark) {
    Initialize();
    const State& s = S();
    if (!s.supported || hwnd == nullptr) {
        return;
    }

    if (s.allowDarkModeForWindow != nullptr) {
        s.allowDarkModeForWindow(hwnd, dark ? TRUE : FALSE);
    }

    // Başlık çubuğu ve çerçeve rengi DWM'nin işi; uxtheme'i etkilemez.
    BOOL value = dark ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &value,
                            static_cast<DWORD>(sizeof(value)));
    if (s.build < kDwmAttributeStableBuild) {
        // 1809–19H2: aynı özellik 19 numaralıydı. İkisini de yazmak, hangisinin
        // geçerli olduğunu ayırt etmekten hem ucuz hem güvenli.
        ::DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkModeLegacy, &value,
                                static_cast<DWORD>(sizeof(value)));
    }

    ::SetWindowTheme(hwnd, DarkThemeName(dark), nullptr);
}

void ApplyToChildren(HWND parent, bool dark) {
    Initialize();
    if (!S().supported || parent == nullptr) {
        return;
    }
    ::EnumChildWindows(parent, ApplyChildProc, dark ? 1 : 0);
}

void RefreshWindow(HWND hwnd) {
    if (hwnd == nullptr) {
        return;
    }
    // SWP_FRAMECHANGED, DWM'nin yeni başlık çubuğu rengini hesaplaması için
    // WM_NCCALCSIZE tetikler; RedrawWindow istemci alanını ve alt kontrolleri
    // aynı karede tazeler (aksi hâlde eski renkler bir an görünür).
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_FRAMECHANGED | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    ::RedrawWindow(hwnd, nullptr, nullptr,
                   RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_ERASE);
}

// ---------------------------------------------------------------------------
// Dialog boyama
// ---------------------------------------------------------------------------
HBRUSH OnCtlColor(UINT msg, WPARAM wParam, LPARAM lParam, bool dark) {
    if (!dark) {
        return nullptr;  // Açık temada sistem varsayılanı bozulmaz.
    }

    HDC hdc = reinterpret_cast<HDC>(wParam);
    State& s = S();

    switch (msg) {
        case WM_CTLCOLORDLG:
            ::SetTextColor(hdc, TextColor(true));
            ::SetBkColor(hdc, BackgroundColor(true));
            return CachedBrush(s.backgroundBrush, BackgroundColor(true));

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            // Devre dışı etiket/onay kutusu koyu temada da soluk görünmelidir:
            // kendi metin rengimizi yazdığımız için sistemin COLOR_GRAYTEXT'i
            // devreye girmez, gri tonu bizim vermemiz gerekir.
            const bool enabled = (::IsWindowEnabled(reinterpret_cast<HWND>(lParam)) != FALSE);
            ::SetTextColor(hdc, enabled ? TextColor(true) : DisabledTextColor(true));
            ::SetBkColor(hdc, BackgroundColor(true));
            // Etiket/onay kutusu metninin arkasına dikdörtgen blok çizilmesin;
            // zemini döndürdüğümüz fırça boyar.
            ::SetBkMode(hdc, TRANSPARENT);
            return CachedBrush(s.backgroundBrush, BackgroundColor(true));
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            // Giriş alanları diyalog zemininden bir ton açık olmalı, yoksa
            // kenarlıksız görünüp "tıklanabilir" hissini kaybediyorlar.
            ::SetTextColor(hdc, TextColor(true));
            ::SetBkColor(hdc, ControlBackgroundColor(true));
            return CachedBrush(s.controlBrush, ControlBackgroundColor(true));

        default:
            return nullptr;
    }
}

void ReleaseCachedObjects() {
    State& s = S();
    if (s.backgroundBrush != nullptr) {
        ::DeleteObject(s.backgroundBrush);
        s.backgroundBrush = nullptr;
    }
    if (s.controlBrush != nullptr) {
        ::DeleteObject(s.controlBrush);
        s.controlBrush = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Renkler — koyu değerler sabit, açık değerler sistemden (yüksek kontrast temalar).
// ---------------------------------------------------------------------------
COLORREF TextColor(bool dark) {
    return dark ? RGB(224, 224, 224) : ::GetSysColor(COLOR_WINDOWTEXT);
}

COLORREF DisabledTextColor(bool dark) {
    return dark ? RGB(128, 128, 128) : ::GetSysColor(COLOR_GRAYTEXT);
}

COLORREF BackgroundColor(bool dark) {
    return dark ? RGB(32, 32, 32) : ::GetSysColor(COLOR_3DFACE);
}

COLORREF ControlBackgroundColor(bool dark) {
    return dark ? RGB(45, 45, 45) : ::GetSysColor(COLOR_WINDOW);
}

}  // namespace WinDark
