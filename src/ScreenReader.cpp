// ScreenReader.cpp — Kilit durumu değişiminin ekran okuyucuya duyurulması (madde 30).
//
// ============================================================================
// SEÇİLEN YOL VE GEREKÇESİ
// ============================================================================
// UI Automation'ın `UiaRaiseNotificationEvent` çağrısı kullanılıyor. Görev
// tanımındaki üç seçenek de denendi/değerlendirildi:
//
// 1) MSAA — EVENT_OBJECT_NAMECHANGE + pencere metnini değiştirmek:
//    ELENDİ. Ekran okuyucular ad değişimini yalnızca ODAKTAKİ (ya da kullanıcının
//    incelediği) öğe için seslendirir. Bizim OSD'miz tasarımı gereği hiçbir zaman
//    odak almaz (WS_EX_NOACTIVATE + WS_EX_TRANSPARENT, spec §4.3), host pencere de
//    0x0 ve hiç gösterilmiyor. Yani olay gönderilir ama hiçbir zaman okunmaz.
//    Ayrıca host pencerenin BAŞLIĞINI değiştirmek fiilen bir hata olurdu: ikinci
//    örnek ilkini FindWindowW(sınıf, BAŞLIK) ile buluyor (App::FindExistingInstance
//    Window), başlık değişince tek örnek mantığı bozulurdu.
//
// 2) MSAA — EVENT_SYSTEM_ALERT:
//    ELENDİ. Uyarı olayının seslendirilmesi için pencerenin ROLE_SYSTEM_ALERT
//    rolünü veren KENDİ IAccessible'ını sunması gerekir; varsayılan pencere
//    IAccessible'ı bunu yapmaz, dolayısıyla olay çoğu okuyucuda sessizce düşer.
//    Kendi IAccessible'ımızı yazmak, tam da kaçındığımız COM sunucu yüzeyidir.
//
// 3) UIA — UiaRaiseNotificationEvent: SEÇİLDİ.
//    Odaktan BAĞIMSIZ olarak "şu metni şimdi oku" diyebilen tek API budur;
//    zaten bu amaç için eklendi. C++/WinRT gerekmez: sağlayıcı nesnesi
//    UiaHostProviderFromHwnd ile herhangi bir HWND'den hazır alınır, kendi
//    IRawElementProviderSimple uygulamamızı yazmamız gerekmez.
//
// DİNAMİK YÜKLEME (import kitaplığı yerine): ayar VARSAYILAN OLARAK KAPALI.
// uiautomationcore.dll'i statik bağlamak, özelliği hiç kullanmayan kullanıcıda da
// süreç açılışında DLL yüklenmesine yol açardı. Dinamik yükleme ayrıca
// UiaRaiseNotificationEvent'in Windows 10 1709 tabanını da sorun olmaktan
// çıkarır: eski bir sistemde çağrı bulunamaz, duyuru sessizce yapılmaz, uygulama
// yine de açılır (statik bağlamada süreç hiç başlamazdı).
//
// DLL bilerek BIRAKILMAZ (FreeLibrary yok): bir kez yüklenir, süreç ömrü boyunca
// yaşar. Yükleme yalnızca ayar açıkken ve ilk duyuruda olur; kapanışta
// FreeLibrary çağırmak, UIA'nın kendi iş parçacıklarıyla yarışa girmek demektir
// ve hiçbir şey kazandırmaz (süreç zaten sonlanıyor).
#include "ScreenReader.h"

#include "Util.h"

#include <oleauto.h>
#include <uiautomation.h>

namespace kli {
namespace ScreenReader {
namespace {

// Aynı etkinlik kimliğiyle gönderilen duyurular birbirini iptal eder: kullanıcı
// Caps Lock'a hızlıca üç kez basarsa okuyucu üç cümleyi sıraya dizmez, yalnızca
// son durumu söyler.
constexpr wchar_t kActivityId[] = L"KeyLockIndicator.LockState";

using PfnUiaClientsAreListening = BOOL(WINAPI*)();
using PfnUiaHostProviderFromHwnd = HRESULT(WINAPI*)(HWND, IRawElementProviderSimple**);
using PfnUiaRaiseNotificationEvent = HRESULT(WINAPI*)(IRawElementProviderSimple*,
                                                      NotificationKind,
                                                      NotificationProcessing, BSTR, BSTR);

// Dosya kapsamlı, iç bağlantılı durum (spec §9 istisnası: .cpp içi static).
// Duyuru yalnızca ana iş parçacığından yapılır, kilit gerekmez.
enum class LoadState { NotTried, Ready, Unavailable };

LoadState s_state = LoadState::NotTried;
PfnUiaClientsAreListening s_clientsAreListening = nullptr;
PfnUiaHostProviderFromHwnd s_hostProviderFromHwnd = nullptr;
PfnUiaRaiseNotificationEvent s_raiseNotification = nullptr;
// "Dinleyen istemci yok" durumu SADECE BİR KEZ loglanır: her tuş basımında
// yazmak, ekran okuyucu kullanmayan bir kullanıcıda log'u boğardı.
bool s_loggedNoClients = false;

struct bstr_traits {
    using value_type = BSTR;
    static BSTR invalid() noexcept { return nullptr; }
    static void close(BSTR b) noexcept { ::SysFreeString(b); }
};
using unique_bstr = unique_res<bstr_traits>;

// Tek denemelik yükleme: başarısızlık kalıcı olarak işaretlenir, her tuş
// basımında yeniden LoadLibrary denemesi yapılmaz.
[[nodiscard]] bool EnsureLoaded() {
    if (s_state == LoadState::Ready) {
        return true;
    }
    if (s_state == LoadState::Unavailable) {
        return false;
    }
    s_state = LoadState::Unavailable;

    // LOAD_LIBRARY_SEARCH_SYSTEM32: arama yolu System32 ile sınırlanır, uygulama
    // klasörüne bırakılmış aynı adlı bir DLL yüklenemez.
    const HMODULE dll =
        ::LoadLibraryExW(L"uiautomationcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dll == nullptr) {
        LogV(L"ScreenReader: uiautomationcore.dll yüklenemedi (hata %lu)",
             ::GetLastError());
        return false;
    }

    s_clientsAreListening =
        reinterpret_cast<PfnUiaClientsAreListening>(
            ::GetProcAddress(dll, "UiaClientsAreListening"));
    s_hostProviderFromHwnd =
        reinterpret_cast<PfnUiaHostProviderFromHwnd>(
            ::GetProcAddress(dll, "UiaHostProviderFromHwnd"));
    s_raiseNotification =
        reinterpret_cast<PfnUiaRaiseNotificationEvent>(
            ::GetProcAddress(dll, "UiaRaiseNotificationEvent"));

    // UiaClientsAreListening zorunlu değil (yoksa her seferinde duyuru denenir),
    // diğer ikisi zorunlu.
    if (s_hostProviderFromHwnd == nullptr || s_raiseNotification == nullptr) {
        LogV(L"ScreenReader: UIA duyuru API'si bu Windows sürümünde yok — duyuru kapalı");
        return false;
    }

    s_state = LoadState::Ready;
    return true;
}

}  // namespace

bool Announce(HWND owner, const std::wstring& text) {
    if (owner == nullptr || text.empty()) {
        return false;
    }
    if (!EnsureLoaded()) {
        return false;
    }
    // Hiçbir erişilebilirlik istemcisi yoksa sağlayıcı kurup olay üretmek boşuna
    // iştir. Bu bir hata değildir; sessizce çıkılır.
    if (s_clientsAreListening != nullptr && s_clientsAreListening() == FALSE) {
        if (!s_loggedNoClients) {
            s_loggedNoClients = true;
            LogV(L"ScreenReader: dinleyen erişilebilirlik istemcisi yok — duyuru atlanıyor");
        }
        return false;
    }
    s_loggedNoClients = false;   // istemci geldi: bir daha ayrılırsa yine bildirilsin

    ComPtr<IRawElementProviderSimple> provider;
    const HRESULT hostHr = s_hostProviderFromHwnd(owner, provider.GetAddressOf());
    if (FAILED(hostHr) || !provider) {
        LogV(L"ScreenReader: UiaHostProviderFromHwnd başarısız (0x%08X)",
             static_cast<unsigned>(hostHr));
        return false;
    }

    // SysAllocStringLen: metin gömülü sıfır içermez ama uzunluk zaten elimizde,
    // ikinci bir wcslen taraması yapılmaz.
    const unique_bstr message(
        ::SysAllocStringLen(text.c_str(), static_cast<UINT>(text.size())));
    const unique_bstr activity(::SysAllocString(kActivityId));
    if (!message || !activity) {
        LogV(L"ScreenReader: BSTR ayrılamadı");
        return false;
    }

    // NotificationKind_Other: bir eylemin sonucu değil, bir DURUM bildirimi.
    // NotificationProcessing_MostRecent: aynı activityId ile bekleyen duyurular
    // düşürülür, yalnızca son durum okunur.
    const HRESULT hr =
        s_raiseNotification(provider.Get(), NotificationKind_Other,
                            NotificationProcessing_MostRecent, message.get(),
                            activity.get());
    if (FAILED(hr)) {
        LogV(L"ScreenReader: UiaRaiseNotificationEvent başarısız (0x%08X)",
             static_cast<unsigned>(hr));
        return false;
    }

    // Duyurunun gerçekten SESLENDİRİLDİĞİNİ program içinden doğrulamak mümkün
    // değil (okuyucu bize geri bildirim vermez); kanıtlanabilir olan tek şey
    // çağrının başarıyla döndüğüdür ve o loglanır.
    LogV(L"ScreenReader: duyuruldu (hr=0x%08X) — %s", static_cast<unsigned>(hr),
         text.c_str());
    return true;
}

}  // namespace ScreenReader
}  // namespace kli
