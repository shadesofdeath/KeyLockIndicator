// AppFilter.cpp — Ön plandaki uygulamanın tespiti (madde 18).
//
// GetForegroundWindow → GetWindowThreadProcessId → QueryFullProcessImageNameW.
//
// NEDEN QueryFullProcessImageNameW: GetModuleFileNameExW psapi'ye bağlıdır ve
// 32/64 bit karışımında başarısız olabilir; QueryFullProcessImageNameW ise
// PROCESS_QUERY_LIMITED_INFORMATION ile çalışır — bu erişim hakkı, yükseltilmiş
// süreçler için de (bütünlük düzeyi izin verdiği ölçüde) açılabilir ve tam
// PROCESS_QUERY_INFORMATION'dan çok daha az yetki ister.
#include "AppFilter.h"

#include "Util.h"

#include <windows.h>

#include <cwchar>   // _wcsicmp

namespace kli {
namespace AppFilter {
namespace {

// Yol ayırıcısından sonrası. Yol yoksa dizenin tamamı ad sayılır.
[[nodiscard]] std::wstring FileNameOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

}  // namespace

std::wstring ForegroundExeName() {
    const HWND fg = ::GetForegroundWindow();
    if (fg == nullptr) {
        return std::wstring();   // masaüstü / kilit ekranı: kural uygulanmaz
    }

    DWORD pid = 0;
    if (::GetWindowThreadProcessId(fg, &pid) == 0 || pid == 0) {
        return std::wstring();
    }

    // FALSE: tutamaç devralınmaz. RAII sarmalayıcı zorunlu — aşağıda birden fazla
    // erken çıkış var ve elle CloseHandle çağırmak kaçak riski taşır.
    const unique_handle process(
        ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process) {
        return std::wstring();
    }

    // Uzun yol desteğiyle MAX_PATH aşılabilir; tampon buna göre seçildi.
    wchar_t buffer[1024]{};
    DWORD length = static_cast<DWORD>(_countof(buffer));
    if (::QueryFullProcessImageNameW(process.get(), 0, buffer, &length) == FALSE) {
        return std::wstring();
    }
    buffer[_countof(buffer) - 1] = L'\0';
    return FileNameOf(std::wstring(buffer, length));
}

bool Matches(const std::vector<std::wstring>& excluded, const std::wstring& exeName) {
    if (exeName.empty()) {
        return false;
    }
    for (const std::wstring& item : excluded) {
        if (_wcsicmp(item.c_str(), exeName.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

bool ForegroundExcluded(const std::vector<std::wstring>& excluded) {
    if (excluded.empty()) {
        return false;   // en sık durum: hiçbir sistem çağrısı yapılmaz
    }
    return Matches(excluded, ForegroundExeName());
}

}  // namespace AppFilter
}  // namespace kli
