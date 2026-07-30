// Util.h — RAII yardımcıları, HRESULT kontrolü, log. Bağımlılığı olmayan yaprak modül.
#pragma once

#include <windows.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>
#include <utility>

namespace kli {

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// ---------------------------------------------------------------------------
// Log — release'de de derlenir ama yalnızca debugger'a yazar. Ucuz olması için
// biçimlendirme yığın üzerinde yapılır, tahsis yok.
// ---------------------------------------------------------------------------
inline void LogV(const wchar_t* fmt, ...) {
    wchar_t buf[512];
    va_list args;
    va_start(args, fmt);
    const int n = _vsnwprintf_s(buf, _TRUNCATE, fmt, args);
    va_end(args);
    if (n < 0) {
        buf[_countof(buf) - 1] = L'\0';
    }
    OutputDebugStringW(L"[KeyLockIndicator] ");
    OutputDebugStringW(buf);
    OutputDebugStringW(L"\n");
}

inline void ReportHr(HRESULT hr, const char* expr, const char* file, int line) {
    LogV(L"HR FAIL 0x%08X  %hs  (%hs:%d)", static_cast<unsigned>(hr), expr, file, line);
#if defined(_DEBUG)
    if (IsDebuggerPresent()) {
        __debugbreak();
    }
#endif
}

// HR(expr): HRESULT'ı kontrol eder, hatada loglar ve hr'yi döndürür (erken çıkış).
// Fonksiyon HRESULT döndürmelidir. Exception fırlatılmaz (bkz. spec §9).
#define HR(expr)                                                    \
    do {                                                            \
        const HRESULT _hr_ = (expr);                                \
        if (FAILED(_hr_)) {                                         \
            ::kli::ReportHr(_hr_, #expr, __FILE__, __LINE__);       \
            return _hr_;                                            \
        }                                                           \
    } while (0)

// HR_BOOL(expr): aynısı ama bool döndüren fonksiyonlar için.
#define HR_BOOL(expr)                                               \
    do {                                                            \
        const HRESULT _hr_ = (expr);                                \
        if (FAILED(_hr_)) {                                         \
            ::kli::ReportHr(_hr_, #expr, __FILE__, __LINE__);       \
            return false;                                           \
        }                                                           \
    } while (0)

// HR_LOG(expr): hatayı loglar, akışı kesmez. Temizlik yollarında kullanılır.
#define HR_LOG(expr)                                                \
    do {                                                            \
        const HRESULT _hr_ = (expr);                                \
        if (FAILED(_hr_)) {                                         \
            ::kli::ReportHr(_hr_, #expr, __FILE__, __LINE__);       \
        }                                                           \
    } while (0)

// ---------------------------------------------------------------------------
// unique_handle — HANDLE / HKEY / HICON / HMENU için ham new/delete'siz RAII.
// ---------------------------------------------------------------------------
template <typename Traits>
class unique_res {
public:
    using value_type = typename Traits::value_type;

    unique_res() noexcept : m_value(Traits::invalid()) {}
    explicit unique_res(value_type v) noexcept : m_value(v) {}

    unique_res(const unique_res&) = delete;
    unique_res& operator=(const unique_res&) = delete;

    unique_res(unique_res&& other) noexcept : m_value(other.release()) {}
    unique_res& operator=(unique_res&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    ~unique_res() noexcept { reset(); }

    [[nodiscard]] value_type get() const noexcept { return m_value; }
    [[nodiscard]] bool valid() const noexcept { return m_value != Traits::invalid(); }
    explicit operator bool() const noexcept { return valid(); }

    value_type release() noexcept {
        value_type v = m_value;
        m_value = Traits::invalid();
        return v;
    }

    void reset(value_type v = Traits::invalid()) noexcept {
        if (m_value != Traits::invalid()) {
            Traits::close(m_value);
        }
        m_value = v;
    }

    // Çıktı parametresi alan API'ler için: &out
    value_type* put() noexcept {
        reset();
        return &m_value;
    }

private:
    value_type m_value;
};

struct handle_traits {
    using value_type = HANDLE;
    static HANDLE invalid() noexcept { return nullptr; }
    static void close(HANDLE h) noexcept { ::CloseHandle(h); }
};

struct file_handle_traits {
    using value_type = HANDLE;
    static HANDLE invalid() noexcept { return INVALID_HANDLE_VALUE; }
    static void close(HANDLE h) noexcept { ::CloseHandle(h); }
};

struct hkey_traits {
    using value_type = HKEY;
    static HKEY invalid() noexcept { return nullptr; }
    static void close(HKEY h) noexcept { ::RegCloseKey(h); }
};

struct hicon_traits {
    using value_type = HICON;
    static HICON invalid() noexcept { return nullptr; }
    static void close(HICON h) noexcept { ::DestroyIcon(h); }
};

struct hmenu_traits {
    using value_type = HMENU;
    static HMENU invalid() noexcept { return nullptr; }
    static void close(HMENU h) noexcept { ::DestroyMenu(h); }
};

struct hbrush_traits {
    using value_type = HBRUSH;
    static HBRUSH invalid() noexcept { return nullptr; }
    static void close(HBRUSH h) noexcept { ::DeleteObject(h); }
};

struct hfont_traits {
    using value_type = HFONT;
    static HFONT invalid() noexcept { return nullptr; }
    static void close(HFONT h) noexcept { ::DeleteObject(h); }
};

using unique_handle      = unique_res<handle_traits>;
using unique_file_handle = unique_res<file_handle_traits>;
using unique_hkey        = unique_res<hkey_traits>;
using unique_hicon       = unique_res<hicon_traits>;
using unique_hmenu       = unique_res<hmenu_traits>;
using unique_hbrush      = unique_res<hbrush_traits>;
using unique_hfont       = unique_res<hfont_traits>;

// COM apartment RAII
class ComApartment {
public:
    ComApartment() = default;
    ComApartment(const ComApartment&) = delete;
    ComApartment& operator=(const ComApartment&) = delete;

    HRESULT Initialize() {
        const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        m_owned = SUCCEEDED(hr);
        return hr;
    }
    ~ComApartment() {
        if (m_owned) {
            ::CoUninitialize();
        }
    }

private:
    bool m_owned = false;
};

// ---------------------------------------------------------------------------
// DPI yardımcıları. Tüm tasarım ölçüleri DIP; piksele çevirim tek yerden.
// ---------------------------------------------------------------------------
constexpr UINT kDefaultDpi = 96;

[[nodiscard]] inline float DpiScale(UINT dpi) noexcept {
    return static_cast<float>(dpi) / static_cast<float>(kDefaultDpi);
}

// Yarım piksel kaymasını önlemek için yuvarlanmış tam sayı piksel.
[[nodiscard]] inline int DipToPx(float dip, UINT dpi) noexcept {
    return static_cast<int>(dip * DpiScale(dpi) + 0.5f);
}

[[nodiscard]] inline float PxToDip(float px, UINT dpi) noexcept {
    return px / DpiScale(dpi);
}

template <typename T>
[[nodiscard]] constexpr T Clamp(T v, T lo, T hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Modül yolu (portable autostart ve OEM kontrolü için)
[[nodiscard]] std::wstring ModulePath();

}  // namespace kli
