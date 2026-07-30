// OemCheck.cpp — OEM çakışma kontrolü (spec §5).
//
// Üreticiler kendi (çoğu tray'siz) tuş göstergesi ajanlarını önyüklü gönderir;
// iki OSD aynı anda çıkarsa kullanıcı bu uygulamayı bozuk sanar. Bu yüzden ilk
// çalıştırmada bir kez bilgilendirilir ve OSD'yi kapatabileceği söylenir.
#include "OemCheck.h"

#include "Localization.h"
#include "Settings.h"
#include "Util.h"
#include "resource.h"

#include <windows.h>

#include <commctrl.h>
#include <tlhelp32.h>

#include <string.h>
#include <wchar.h>

namespace kli {
namespace OemCheck {
namespace {

struct VendorPattern {
    const wchar_t* needle;  // küçük harfle yazılır; kısmi eşleşme aranır
    const wchar_t* vendor;
};

// Spec §5 tablosu. Sıra anlamlıdır: daha uzun/özel desen daha kısa olanından
// önce gelir, yoksa "ATKOSD" eşleşip "ATKOSD2" hiç görünmez.
constexpr VendorPattern kPatterns[] = {
    {L"atkosd2", L"ASUS"},
    {L"atkosd", L"ASUS"},
    {L"hcontroluser", L"ASUS"},
    {L"asusosd", L"ASUS"},
    {L"lenovoutilityservice", L"Lenovo"},
    {L"lenovoutility", L"Lenovo"},
    {L"utility.exe", L"Lenovo"},
    {L"hpsystemeventutility", L"HP"},
    {L"hpsysinfo", L"HP"},
    {L"hotkeyserviceuwp", L"HP"},
    {L"quickaccess.exe", L"Acer"},
    {L"dell.quickset", L"Dell"},
    {L"quickset.exe", L"Dell"},
};

}  // namespace

Result Detect() {
    Result result;

    // TH32CS_SNAPPROCESS başarısızlıkta INVALID_HANDLE_VALUE döner.
    const unique_file_handle snapshot{::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)};
    if (!snapshot) {
        LogV(L"CreateToolhelp32Snapshot başarısız: %lu", ::GetLastError());
        return result;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (::Process32FirstW(snapshot.get(), &entry) == FALSE) {
        return result;
    }

    do {
        // Karşılaştırma küçük harf üzerinde yapılır; kaynak alan bozulmasın diye
        // kopya üzerinde çalışılır (raporlanan ad özgün yazımıyla kalır).
        wchar_t lowered[MAX_PATH];
        const errno_t copyResult = ::wcsncpy_s(lowered, entry.szExeFile, _TRUNCATE);
        if (copyResult != 0 && copyResult != STRUNCATE) {
            continue;
        }
        if (::_wcslwr_s(lowered, _countof(lowered)) != 0) {
            continue;
        }
        for (const VendorPattern& pattern : kPatterns) {
            if (::wcsstr(lowered, pattern.needle) == nullptr) {
                continue;
            }
            result.found = true;
            result.vendor = pattern.vendor;
            result.processName = entry.szExeFile;
            return result;  // ilk eşleşmede dur
        }
    } while (::Process32NextW(snapshot.get(), &entry) != FALSE);

    return result;
}

void WarnOnce(HWND owner, bool alreadyShown) {
    if (alreadyShown) {
        return;
    }
    const Result detected = Detect();
    if (!detected.found) {
        return;
    }
    LogV(L"OEM tuş göstergesi bulundu: %s (%s)", detected.vendor.c_str(),
         detected.processName.c_str());

    const std::wstring title = Loc::Str(IDS_OEM_TITLE);
    const std::wstring instruction = Loc::Str(IDS_OEM_MAIN);
    const std::wstring bodyFormat = Loc::Str(IDS_OEM_BODY);
    const std::wstring dontShow = Loc::Str(IDS_OEM_DONTSHOW);

    // Üretici adı gövde metnindeki %s yerine geçer. swprintf_s taşmada geçersiz
    // parametre işleyicisini çağırıp süreci sonlandırır; _TRUNCATE'li sürüm
    // yalnızca keser, bu yüzden kaynak metin uzasa da güvenli.
    //
    // Sıfırla başlatılır: biçimlendirme hiç yazamadan başarısız olursa (bozuk
    // kaynak metni) tampon yine sonlandırılmış kalır — hem TaskDialog hem de
    // MessageBox yolu bu diziyi doğrudan gösterir.
    wchar_t body[768]{};
    if (::_snwprintf_s(body, _countof(body), _TRUNCATE, bodyFormat.c_str(),
                       detected.vendor.c_str()) < 0) {
        LogV(L"OEM gövde metni kesildi veya biçimlendirilemedi");
    }

    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = title.c_str();
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszMainInstruction = instruction.c_str();
    config.pszContent = body;
    config.pszVerificationText = dontShow.c_str();

    BOOL dontShowChecked = FALSE;
    const HRESULT hr = ::TaskDialogIndirect(&config, nullptr, nullptr, &dontShowChecked);
    if (SUCCEEDED(hr)) {
        if (dontShowChecked != FALSE) {
            MarkOemWarningShown();
        }
        return;
    }

    // Common Controls 6 aktivasyon bağlamı yoksa TaskDialogIndirect başarısız
    // olur. Düz kutuda onay kutusu sunulamadığı için uyarı koşulsuz olarak bir
    // daha gösterilmez — tekrar tekrar rahatsız etmek daha kötü bir sonuç.
    ReportHr(hr, "TaskDialogIndirect(OEM)", __FILE__, __LINE__);
    ::MessageBoxW(owner, body, title.c_str(), MB_OK | MB_ICONINFORMATION);
    MarkOemWarningShown();
}

}  // namespace OemCheck
}  // namespace kli
