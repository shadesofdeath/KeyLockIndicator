// SettingsExclude.cpp — İstisna listesi (madde 18), içe/dışa aktarma (madde 32)
// ve taşınabilir kip göstergesi (madde 25).
//
// SettingsDialog sınıfının parçasıdır; bölünme yalnızca dosya başına 400 satır
// sınırı içindir (spec §9). Üç özellik aynı dosyada çünkü üçü de aynı iki
// mekanizmayı kullanıyor: bir liste kutusu ve comdlg32 dosya seçicileri.
#include "SettingsDialog.h"

#include "Localization.h"
#include "Settings.h"
#include "Util.h"
#include "resource.h"

#include <windows.h>
#include <commdlg.h>

#include <string>
#include <vector>

namespace kli {
namespace {

// comdlg32 filtresi ÇİFT sonlandırıcılı bir dize çiftidir: "Açıklama\0*.exe\0\0".
// std::wstring gömülü '\0' taşıyabildiği için tampon yönetimi gerekmez.
[[nodiscard]] std::wstring MakeFilter(UINT descriptionId, const wchar_t* pattern) {
    std::wstring filter = Loc::Str(descriptionId);
    if (filter.empty()) {
        filter = pattern;   // dil yüklenemedi: en azından desen görünsün
    }
    filter.push_back(L'\0');
    filter.append(pattern);
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    return filter;
}

// Dosya seçicilerin ortak kurulumu. OFN_NOCHANGEDIR şart: seçici, sürecin
// çalışma dizinini kalıcı olarak değiştirebilir ve taşınabilir kipteki .ini
// yolu göreli çözülseydi bozulurdu (bizde mutlak, yine de bırakılmaz).
void InitOfn(OPENFILENAMEW& ofn, HWND owner, wchar_t* buffer, DWORD bufferChars,
             const std::wstring& filter, const wchar_t* defExt) {
    ofn.lStructSize = static_cast<DWORD>(sizeof(ofn));
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = bufferChars;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
}

void ShowError(HWND owner, UINT messageId) {
    ::MessageBoxW(owner, Loc::Str(messageId).c_str(), Loc::Str(IDS_SET_TITLE).c_str(),
                  MB_OK | MB_ICONWARNING);
}

}  // namespace

// ---------------------------------------------------------------------------
// İstisna listesi (madde 18)
// ---------------------------------------------------------------------------

void SettingsDialog::RefreshExcludeList() {
    if (!m_hwnd) {
        return;
    }
    const HWND list = GetDlgItem(m_hwnd, IDC_LST_EXCLUDE);
    if (list == nullptr) {
        return;
    }
    // Seçimi koru: kullanıcı "Kaldır"a bastıktan sonra listenin başına atlaması
    // rahatsız edici olur; aynı sıradaki öğe seçili kalır.
    const int previous = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const std::wstring& app : m_settings.excludedApps) {
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(app.c_str()));
    }
    const int count = static_cast<int>(m_settings.excludedApps.size());
    if (count > 0) {
        const int select = (previous >= 0 && previous < count) ? previous : count - 1;
        SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(select), 0);
    }
    // "Kaldır" boş listede anlamsız; pasif bir buton kullanıcıya durumu anlatır.
    if (const HWND del = GetDlgItem(m_hwnd, IDC_BTN_EXCL_DEL)) {
        EnableWindow(del, count > 0 ? TRUE : FALSE);
    }
}

void SettingsDialog::OnAddExcluded() {
    if (!m_hwnd) {
        return;
    }
    const std::wstring filter = MakeFilter(IDS_FILTER_EXE, L"*.exe");
    wchar_t path[MAX_PATH * 2]{};

    OPENFILENAMEW ofn{};
    InitOfn(ofn, m_hwnd, path, static_cast<DWORD>(_countof(path)), filter, L"exe");
    ofn.Flags |= OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (::GetOpenFileNameW(&ofn) == FALSE) {
        return;   // iptal ya da hata: sessiz, kullanıcı zaten vazgeçti
    }

    // Settings yalnızca exe ADINI saklar (tam yolu değil); dönüşümü o yapar.
    if (!m_settings.AddExcludedApp(path)) {
        return;   // yinelenen ya da liste dolu — sessizce yok sayılır
    }
    RefreshExcludeList();
    Emit();
}

void SettingsDialog::OnRemoveExcluded() {
    if (!m_hwnd) {
        return;
    }
    const HWND list = GetDlgItem(m_hwnd, IDC_LST_EXCLUDE);
    if (list == nullptr) {
        return;
    }
    const int sel = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
    if (sel < 0) {
        return;   // LB_ERR
    }
    if (!m_settings.RemoveExcludedApp(static_cast<size_t>(sel))) {
        return;
    }
    RefreshExcludeList();
    Emit();
}

// ---------------------------------------------------------------------------
// İçe / dışa aktarma (madde 32)
// ---------------------------------------------------------------------------

void SettingsDialog::OnExportSettings() {
    if (!m_hwnd) {
        return;
    }
    const std::wstring filter = MakeFilter(IDS_FILTER_INI, L"*.ini");
    wchar_t path[MAX_PATH * 2] = L"KeyLockIndicator.ini";   // önerilen ad

    OPENFILENAMEW ofn{};
    InitOfn(ofn, m_hwnd, path, static_cast<DWORD>(_countof(path)), filter, L"ini");
    ofn.Flags |= OFN_OVERWRITEPROMPT;
    if (::GetSaveFileNameW(&ofn) == FALSE) {
        return;
    }

    // Kontrollerdeki EN GÜNCEL değerler yazılır: kullanıcı bir kaydırıcıyı
    // oynattıysa o değişiklik de dosyaya girmeli.
    PushFromControls();
    if (!m_settings.ExportToFile(path)) {
        ShowError(m_hwnd, IDS_EXPORT_FAILED);
    }
}

void SettingsDialog::OnImportSettings() {
    if (!m_hwnd) {
        return;
    }
    const std::wstring filter = MakeFilter(IDS_FILTER_INI, L"*.ini");
    wchar_t path[MAX_PATH * 2]{};

    OPENFILENAMEW ofn{};
    InitOfn(ofn, m_hwnd, path, static_cast<DWORD>(_countof(path)), filter, L"ini");
    ofn.Flags |= OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (::GetOpenFileNameW(&ofn) == FALSE) {
        return;
    }

    // Eksik/bozuk dosyada Settings varsayılana düşer ve true döner (bkz.
    // Settings::ImportFromFile); false yalnızca dosya hiç okunamadığında gelir,
    // o hâlde mevcut ayarlar KORUNUR.
    Settings imported = m_settings;
    if (!Settings::ImportFromFile(path, imported)) {
        ShowError(m_hwnd, IDS_IMPORT_FAILED);
        return;
    }
    m_settings = imported;
    // Kontroller anında yenilenir ve değişiklik App'e iletilir — "içe aktarınca
    // ayarlar anında uygulanır" sözü budur.
    LoadControls();
    UpdateValueLabels();
    RefreshExcludeList();
    Emit();
}

// ---------------------------------------------------------------------------
// Taşınabilir kip göstergesi (madde 25)
// ---------------------------------------------------------------------------

void SettingsDialog::UpdateStorageLabel() {
    if (!m_hwnd) {
        return;
    }
    if (!Settings::PortableMode()) {
        SetDlgItemTextW(m_hwnd, IDC_LBL_STORAGE, Loc::Str(IDS_STORAGE_REGISTRY).c_str());
        return;
    }
    // Dosyanın TAM YOLU yazılır: kullanıcı hangi kopyanın hangi ayarları
    // kullandığını (birden fazla taşınabilir kopya olabilir) buradan görür.
    // %s yer tutucusu çeviride bulunmuyorsa metin yine de anlamlı kalır.
    const std::wstring format = Loc::Str(IDS_STORAGE_PORTABLE);
    const std::wstring file = Settings::PortableFilePath();
    wchar_t buffer[600];
    _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, format.c_str(), file.c_str());
    SetDlgItemTextW(m_hwnd, IDC_LBL_STORAGE, buffer);
}

}  // namespace kli
