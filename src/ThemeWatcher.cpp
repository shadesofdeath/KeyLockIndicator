// ThemeWatcher.cpp — Açık/koyu tema takibi (spec §4.2).
//
// İki tespit yolu birlikte kullanılır: WM_SETTINGCHANGE + "ImmersiveColorSet"
// bazı Windows sürümlerinde hiç gelmez, RegNotifyChangeKeyValue ise her zaman
// gelir ama pencere mesajı üretmez. İkisi de aynı Reevaluate'e bağlanır ve
// tema gerçekten değişmedikçe callback tetiklenmez, böylece çifte bildirim
// gereksiz yeniden çizime yol açmaz.

#include "ThemeWatcher.h"

#include "Messages.h"

#include <wchar.h>

#include <utility>

namespace kli {
namespace {

// Windows'un kişiselleştirme anahtarı. HKCU altında olduğu için yükseltme
// gerektirmez ve kullanıcı başına doğru değeri verir.
constexpr const wchar_t* kPersonalizeKeyPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr const wchar_t* kAppsUseLightTheme = L"AppsUseLightTheme";

}  // namespace

AppTheme ThemeWatcher::ReadSystemTheme() noexcept {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = ::RegGetValueW(HKEY_CURRENT_USER, kPersonalizeKeyPath,
                                          kAppsUseLightTheme, RRF_RT_REG_DWORD,
                                          nullptr, &value, &size);
    if (status != ERROR_SUCCESS) {
        // Değer hiç yazılmamış olabilir (temiz kurulum) — Windows varsayılanı
        // açık temadır.
        return AppTheme::Light;
    }
    return value == 0 ? AppTheme::Dark : AppTheme::Light;
}

ThemeWatcher::~ThemeWatcher() {
    Stop();
}

void ThemeWatcher::Start(HWND hostWnd, ThemeCallback cb) {
    if (m_running.load()) {
        Stop();
    }

    m_host = hostWnd;
    m_callback = std::move(cb);

    // Başlangıç teması callback ile DUYURULMAZ; App ilk değeri Current() ile
    // okur, böylece açılışta gereksiz bir tema geçişi yaşanmaz.
    m_theme = ReadSystemTheme();

    m_stopEvent.reset(::CreateEventW(nullptr, TRUE, FALSE, nullptr));  // manual-reset

    if (m_stopEvent && m_host != nullptr) {
        m_thread = std::thread(&ThemeWatcher::WatchThread, this);
    } else {
        // Thread kurulamadıysa uygulama çalışmaya devam eder; tema değişimi
        // yalnızca WM_SETTINGCHANGE üzerinden yakalanır.
        LogV(L"ThemeWatcher: kayıt defteri izleme thread'i kurulamadı (hata %lu)",
             ::GetLastError());
    }

    m_running.store(true);
}

void ThemeWatcher::Stop() {
    // İki kez çağrılabilir olmalı: App::Shutdown + yıkıcı.
    if (m_stopEvent) {
        ::SetEvent(m_stopEvent.get());
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    // Olay yalnızca thread bittikten sonra kapatılır; thread onu bekliyordu.
    m_stopEvent.reset();

    m_running.store(false);
    m_host = nullptr;
    m_callback = nullptr;
}

void ThemeWatcher::WatchThread() {
    // Bildirim olayı anahtardan ÖNCE bildirilir. Yerel nesneler ters sırada
    // yıkıldığı için böylece RegCloseKey önce çalışır: bekleyen bir kayıt
    // bildirimi anahtar kapanınca olayı işaretler, olay ondan önce kapatılmışsa
    // aradaki handle başka bir nesneye geri dönüştürülmüş olabilir.
    //
    // Auto-reset: her bildirimden sonra kendini sıfırlar, yeniden kaydolmadan
    // önce elle ResetEvent gerekmez.
    unique_handle notifyEvent(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
    if (!notifyEvent) {
        LogV(L"CreateEventW(notify) başarısız (hata %lu)", ::GetLastError());
        return;
    }

    unique_hkey key;
    const LSTATUS opened = ::RegOpenKeyExW(HKEY_CURRENT_USER, kPersonalizeKeyPath,
                                           0, KEY_NOTIFY, key.put());
    if (opened != ERROR_SUCCESS) {
        LogV(L"RegOpenKeyExW(Personalize) başarısız (%ld)", opened);
        return;
    }

    const HANDLE waits[2] = {m_stopEvent.get(), notifyEvent.get()};

    for (;;) {
        // Bildirim tek seferliktir; her turda yeniden kaydolunur.
        const LSTATUS reg = ::RegNotifyChangeKeyValue(key.get(), FALSE,
                                                      REG_NOTIFY_CHANGE_LAST_SET,
                                                      notifyEvent.get(), TRUE);
        if (reg != ERROR_SUCCESS) {
            LogV(L"RegNotifyChangeKeyValue başarısız (%ld)", reg);
            return;
        }

        const DWORD signaled = ::WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (signaled == WAIT_OBJECT_0) {
            return;  // Stop() çağrıldı
        }
        if (signaled != WAIT_OBJECT_0 + 1) {
            LogV(L"WaitForMultipleObjects beklenmeyen sonuç %lu (hata %lu)",
                 signaled, ::GetLastError());
            return;
        }

        // Callback bu thread'te ASLA çağrılmaz: D2D fırçaları, pencere ve tray
        // durumu UI thread'ine aittir. Sadece host pencereye haber verilir.
        ::PostMessageW(m_host, WM_KLI_THEMENOTIFY, 0, 0);
    }
}

void ThemeWatcher::OnSettingChange(LPCWSTR area) {
    if (area != nullptr && _wcsicmp(area, L"ImmersiveColorSet") == 0) {
        Reevaluate();
    }
}

void ThemeWatcher::OnRegistryNotify() {
    Reevaluate();
}

void ThemeWatcher::Reevaluate() {
    const AppTheme next = ReadSystemTheme();
    if (next == m_theme) {
        // Aynı tema: iki tespit yolu aynı değişimi bildirdiğinde ya da alakasız
        // bir kişiselleştirme değeri yazıldığında buraya düşülür.
        return;
    }
    m_theme = next;
    if (m_callback) {
        m_callback(next);
    }
}

}  // namespace kli
