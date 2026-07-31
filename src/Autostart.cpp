// Autostart.cpp — Windows ile başlat (spec §4.7).
//
// Portable derleme HKCU\...\Run değeri kullanır; MSIX derlemesinde bu anahtar
// konteynerde sanallaştırıldığı için StartupTask WinRT API'si zorunludur. Hangi
// yolun geçerli olduğu derleme zamanı KLI_PACKAGED + çalışma zamanı
// GetCurrentPackageFullName() doğrulamasıyla belirlenir.
#include "Autostart.h"

#include "Util.h"

#include <windows.h>

#include <appmodel.h>

#include <string>

#ifdef KLI_PACKAGED
// Ham WinRT ABI: C++/WinRT bağımlılığı YASAK olduğu için üretilmiş ABI
// başlıkları ve RoGetActivationFactory doğrudan kullanılır.
#pragma message( \
    "KLI_PACKAGED: StartupTask yolu ham WinRT ABI ile derleniyor; " \
    "IAsyncOperation beklemesi IAsyncInfo durum yoklamasiyla yapilir.")
#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.applicationmodel.h>
#include <wchar.h>
#endif

namespace kli {
namespace Autostart {
namespace {

// Görev Yöneticisi "Başlangıç" sekmesi kullanıcı kararını buraya yazar; Run
// anahtarındaki değer silinmez, yalnızca burada işaretlenir.
constexpr const wchar_t* kStartupApprovedPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

enum class Backend {
    RunKey,        // HKCU\...\Run değeri
    StartupTask,   // Windows.ApplicationModel.StartupTask
    Unavailable,   // Bu derlemede karşılığı yok
};

// TAŞINABİLİR KİP (madde 25) VE OTOMATİK BAŞLATMA — SEÇİLEN YOL: Run anahtarı
// KULLANILMAYA DEVAM EDER, özellik devre dışı BIRAKILMAZ.
//
// Gerekçe: "taşınabilir" olan ayarların saklandığı yerdir, işletim sisteminin
// oturum açılışında ne çalıştıracağı değil. Windows'ta bir uygulamayı oturumla
// başlatmanın kullanıcı düzeyindeki tek yolu HKCU\...\Run'dır (paketli
// derlemede StartupTask); "portable" bir exe için bunun dosya tabanlı bir
// karşılığı yoktur. Özelliği kapatmak, USB'den çalıştırdığı programı her açılışta
// elle başlatmak zorunda kalan kullanıcıya hiçbir şey kazandırmazdı — kayıt
// defterine tek bir değer yazmak zaten geri alınabilir bir işlemdir ve
// "Windows ile başlat" kutusu kapatıldığında değer silinir.
//
// Kullanıcı bunu bilmeli: Ayarlar penceresindeki depo satırı (IDC_LBL_STORAGE)
// taşınabilir kipte otomatik başlatmanın yine kayıt defterini kullandığını
// açıkça yazar.
[[nodiscard]] Backend SelectBackend() {
#ifdef KLI_PACKAGED
    // Paketli derleme paketsiz çalıştırılabilir (geliştirme/CI); o hâlde Run
    // anahtarı hâlâ doğru araçtır.
    return IsPackaged() ? Backend::StartupTask : Backend::RunKey;
#else
    // Paketsiz derleme paketli çalışıyorsa Run yazımı sanallaştırılır ve hiçbir
    // etkisi olmaz; kullanıcıya yanlış "etkin" göstermemek için devre dışı.
    return IsPackaged() ? Backend::Unavailable : Backend::RunKey;
#endif
}

// Yol boşluk içerebilir; Run anahtarındaki komut satırı tırnaklı olmalı.
[[nodiscard]] std::wstring QuotedModulePath() {
    std::wstring cmd;
    cmd.reserve(MAX_PATH + 2);
    cmd.push_back(L'"');
    cmd.append(ModulePath());
    cmd.push_back(L'"');
    return cmd;
}

[[nodiscard]] bool RunValueExists() {
    unique_hkey key;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, key.put()) !=
        ERROR_SUCCESS) {
        return false;
    }
    return ::RegQueryValueExW(key.get(), kRunValueName, nullptr, nullptr, nullptr,
                              nullptr) == ERROR_SUCCESS;
}

[[nodiscard]] bool WriteRunValue() {
    unique_hkey key;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr, 0, KEY_SET_VALUE,
                          nullptr, key.put(), nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const std::wstring cmd = QuotedModulePath();
    const DWORD bytes = static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t));
    const LSTATUS rc =
        ::RegSetValueExW(key.get(), kRunValueName, 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(cmd.c_str()), bytes);
    if (rc != ERROR_SUCCESS) {
        LogV(L"RegSetValueExW(Run) başarısız: %ld", rc);
        return false;
    }
    return true;
}

[[nodiscard]] bool DeleteRunValue() {
    unique_hkey key;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_SET_VALUE, key.put()) !=
        ERROR_SUCCESS) {
        return false;
    }
    const LSTATUS rc = ::RegDeleteValueW(key.get(), kRunValueName);
    // Zaten yoksa hedef durum sağlanmış sayılır.
    return rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND;
}

// StartupApproved\Run: REG_BINARY 12 bayt; ilk bayt 0x02/0x03 kullanıcının
// kapattığını, 0x00/0x06 etkin olduğunu gösterir.
[[nodiscard]] bool RunValueBlockedByUser() {
    unique_hkey key;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kStartupApprovedPath, 0, KEY_QUERY_VALUE,
                        key.put()) != ERROR_SUCCESS) {
        return false;
    }
    BYTE data[16]{};
    DWORD size = sizeof(data);
    DWORD type = REG_NONE;
    if (::RegQueryValueExW(key.get(), kRunValueName, nullptr, &type, data, &size) !=
        ERROR_SUCCESS) {
        return false;
    }
    if (type != REG_BINARY || size == 0) {
        return false;
    }
    return data[0] == 0x02 || data[0] == 0x03;
}

#ifdef KLI_PACKAGED

using ABI::Windows::ApplicationModel::IStartupTask;
using ABI::Windows::ApplicationModel::IStartupTaskStatics;
using ABI::Windows::ApplicationModel::StartupTask;
using ABI::Windows::ApplicationModel::StartupTaskState;
using ABI::Windows::Foundation::AsyncStatus;
using ABI::Windows::Foundation::IAsyncInfo;
using ABI::Windows::Foundation::IAsyncOperation;

// StartupTaskState kapsamsız (unscoped) bir enum; sabitleri tip adıyla değil
// çevreleyen ad alanıyla gelir, bu yüzden tek tek içeri alınır.
using ABI::Windows::ApplicationModel::StartupTaskState_Disabled;
using ABI::Windows::ApplicationModel::StartupTaskState_DisabledByPolicy;
using ABI::Windows::ApplicationModel::StartupTaskState_DisabledByUser;
using ABI::Windows::ApplicationModel::StartupTaskState_Enabled;
using ABI::Windows::ApplicationModel::StartupTaskState_EnabledByPolicy;

// Yığın üzerinde HSTRING referansı: tahsis yok, yıkım gerekmez.
class HStringRef {
public:
    explicit HStringRef(const wchar_t* text) noexcept {
        if (FAILED(::WindowsCreateStringReference(
                text, static_cast<UINT32>(::wcslen(text)), &m_header, &m_value))) {
            m_value = nullptr;
        }
    }
    HStringRef(const HStringRef&) = delete;
    HStringRef& operator=(const HStringRef&) = delete;

    [[nodiscard]] HSTRING get() const noexcept { return m_value; }

private:
    HSTRING_HEADER m_header{};
    HSTRING m_value = nullptr;
};

// STA üzerinde kısa senkron bekleme. Completed handler sınıfı yazmamak için
// IAsyncInfo durumu yoklanır; StartupTask sorguları milisaniyeler sürer ve bu
// yol yalnızca kullanıcı etkileşimiyle (menü/ayarlar) tetiklenir.
[[nodiscard]] HRESULT AwaitAsync(IInspectable* operation) {
    ComPtr<IAsyncInfo> info;
    HR(operation->QueryInterface(__uuidof(IAsyncInfo),
                                 reinterpret_cast<void**>(info.GetAddressOf())));
    for (int spin = 0; spin < 500; ++spin) {  // en fazla ~5 sn
        AsyncStatus status = AsyncStatus::Started;
        HR(info->get_Status(&status));
        if (status == AsyncStatus::Completed) {
            return S_OK;
        }
        if (status != AsyncStatus::Started) {
            HRESULT code = E_FAIL;
            HR_LOG(info->get_ErrorCode(&code));
            return FAILED(code) ? code : E_FAIL;
        }
        ::Sleep(10);
    }
    return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
}

[[nodiscard]] HRESULT AcquireStartupTask(ComPtr<IStartupTask>& out) {
    const HStringRef className(RuntimeClass_Windows_ApplicationModel_StartupTask);
    ComPtr<IStartupTaskStatics> statics;
    HR(::RoGetActivationFactory(className.get(), __uuidof(IStartupTaskStatics),
                                reinterpret_cast<void**>(statics.GetAddressOf())));

    const HStringRef taskId(kStartupTaskId);
    ComPtr<IAsyncOperation<StartupTask*>> operation;
    HR(statics->GetAsync(taskId.get(), operation.GetAddressOf()));
    HR(AwaitAsync(operation.Get()));
    return operation->GetResults(out.GetAddressOf());
}

[[nodiscard]] StartupTaskState QueryStartupTaskState() {
    ComPtr<IStartupTask> task;
    if (FAILED(AcquireStartupTask(task)) || !task) {
        return StartupTaskState_Disabled;
    }
    StartupTaskState state = StartupTaskState_Disabled;
    HR_LOG(task->get_State(&state));
    return state;
}

[[nodiscard]] bool RequestStartupTask(bool enable) {
    ComPtr<IStartupTask> task;
    if (FAILED(AcquireStartupTask(task)) || !task) {
        return false;
    }
    if (!enable) {
        return SUCCEEDED(task->Disable());
    }
    ComPtr<IAsyncOperation<StartupTaskState>> operation;
    if (FAILED(task->RequestEnableAsync(operation.GetAddressOf())) || !operation) {
        return false;
    }
    if (FAILED(AwaitAsync(operation.Get()))) {
        return false;
    }
    StartupTaskState state = StartupTaskState_Disabled;
    if (FAILED(operation->GetResults(&state))) {
        return false;
    }
    // Kullanıcı reddederse DisabledByUser döner; bu bir hata değil, karardır.
    return state == StartupTaskState_Enabled || state == StartupTaskState_EnabledByPolicy;
}

#endif  // KLI_PACKAGED

}  // namespace

bool IsPackaged() {
    // Süreç ömrü boyunca değişmez; her sorguda API çağırmak gereksiz.
    static const bool packaged = []() noexcept {
        UINT32 length = 0;
        return ::GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
    }();
    return packaged;
}

bool IsEnabled() {
    switch (SelectBackend()) {
        case Backend::RunKey:
            return RunValueExists();
#ifdef KLI_PACKAGED
        case Backend::StartupTask: {
            const StartupTaskState state = QueryStartupTaskState();
            return state == StartupTaskState_Enabled ||
                   state == StartupTaskState_EnabledByPolicy;
        }
#endif
        default:
            return false;
    }
}

bool SetEnabled(bool enable) {
    switch (SelectBackend()) {
        case Backend::RunKey:
            return enable ? WriteRunValue() : DeleteRunValue();
#ifdef KLI_PACKAGED
        case Backend::StartupTask:
            return RequestStartupTask(enable);
#endif
        default:
            LogV(L"Autostart arka ucu bu derlemede kullanılamıyor");
            return false;
    }
}

bool IsDisabledByPolicy() {
    switch (SelectBackend()) {
        case Backend::RunKey:
            return RunValueBlockedByUser();
#ifdef KLI_PACKAGED
        case Backend::StartupTask: {
            const StartupTaskState state = QueryStartupTaskState();
            return state == StartupTaskState_DisabledByUser ||
                   state == StartupTaskState_DisabledByPolicy;
        }
#endif
        default:
            // Değiştirilemiyorsa onay kutusu pasifleştirilmeli.
            return true;
    }
}

}  // namespace Autostart
}  // namespace kli
