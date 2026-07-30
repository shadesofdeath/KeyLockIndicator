// main.cpp — WinMain: DPI, tek örnek kilidi, COM apartmanı, App yaşam döngüsü
// (spec §6). Burada global durum tutulmaz; App yığında yaşar.
#include "App.h"
#include "Messages.h"
#include "Util.h"

#include <windows.h>
#include <commctrl.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Manifest'teki PerMonitorV2 bildiriminin yedeği. Manifest zaten uyguladıysa
    // bu çağrı ERROR_ACCESS_DENIED ile başarısız olur; beklenen durumdur, sonuç
    // bilinçli olarak yok sayılır.
    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Common Controls 6 sınıflarını (trackbar, up-down, TaskDialogIndirect)
    // kullanacağız; aktivasyon bağlamı manifest'ten gelir, sınıf kaydı buradan.
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_UPDOWN_CLASS;
    ::InitCommonControlsEx(&icc);

    // Kilit süreç ömrü boyunca yığında tutulur; yıkıcı CloseHandle çağırır.
    const kli::unique_handle instanceLock{
        ::CreateMutexW(nullptr, TRUE, kli::kSingleInstanceMutex)};
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        // İkinci örnek: ilk örneğe ayarları açtırıp sessizce çekilir. Mesaj
        // RegisterWindowMessage ile alındığı için süreçler arası çakışmaz.
        const HWND existing = kli::App::FindExistingInstanceWindow();
        const UINT showSettings = ::RegisterWindowMessageW(kli::kShowSettingsMessageName);
        if (existing != nullptr && showSettings != 0) {
            ::PostMessageW(existing, showSettings, 0, 0);
        } else {
            kli::LogV(L"Mevcut örnek bulunamadı (pencere=%p, mesaj=%u)",
                      static_cast<void*>(existing), showSettings);
        }
        return 0;
    }

    // STA: Shell_NotifyIcon, TaskDialog ve WIC/DComp tüketicileri bunu bekler.
    kli::ComApartment com;
    HR_LOG(com.Initialize());

    kli::App app;
    if (!app.Initialize(hInstance)) {
        kli::LogV(L"App::Initialize başarısız — çıkılıyor");
        return 1;
    }
    return app.Run();
}
