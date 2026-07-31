// App.cpp — Uygulama koordinatörü (spec §2, §6).
//
// Modüller birbirini tanımaz; tüm bağlar burada std::function callback'lerle
// kurulur. Host pencere durumu GWLP_USERDATA üzerinden taşınır (global yok, §9).
#include "App.h"

#include "Localization.h"
#include "Messages.h"
#include "OemCheck.h"
#include "resource.h"

#include <WinDark/WinDark.h>

#include <wtsapi32.h>

#include <cwchar>   // _wcsicmp

namespace kli {

App::~App() {
    Shutdown();
}

// ---------------------------------------------------------------------------
// Host pencere
// ---------------------------------------------------------------------------

LRESULT CALLBACK App::HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* self = static_cast<App*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) {
            // CreateWindowExW henüz dönmedi; sonraki mesajlar m_host'a ihtiyaç duyar.
            self->m_host = hwnd;
        }
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    auto* self = reinterpret_cast<App*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr) {
        return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->HandleMessage(msg, wParam, lParam);
}

bool App::CreateHostWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::HostWndProc;
    wc.hInstance = m_instance;
    wc.lpszClassName = kHostWindowClass;
    if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        LogV(L"RegisterClassExW(host) başarısız: %lu", ::GetLastError());
        return false;
    }

    // HWND_MESSAGE DEĞİL: mesaj-yalnızca pencereler yayın mesajlarını
    // (WM_SETTINGCHANGE / TaskbarCreated) almaz, tema değişimi kaçar (spec §4.2);
    // yerine 0x0 boyutlu, hiç gösterilmeyen bir üst düzey pencere kullanılır.
    // WS_EX_NOACTIVATE de bilinçli YOK: spec §4.5 TrackPopupMenu öncesi
    // SetForegroundWindow şart koşar. Pencere hiç gösterilmediği için odak çalmaz;
    // Alt+Tab/görev çubuğundan gizlemeye WS_EX_TOOLWINDOW tek başına yeter.
    m_host = ::CreateWindowExW(WS_EX_TOOLWINDOW, kHostWindowClass, kHostWindowTitle,
                               WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, m_instance, this);
    if (m_host == nullptr) {
        LogV(L"CreateWindowExW(host) başarısız: %lu", ::GetLastError());
        return false;
    }
    return true;
}

HWND App::FindExistingInstanceWindow() {
    return ::FindWindowW(kHostWindowClass, kHostWindowTitle);
}

// ---------------------------------------------------------------------------
// Yaşam döngüsü
// ---------------------------------------------------------------------------

bool App::Initialize(HINSTANCE hInstance) {
    m_instance = hInstance;

    m_settings = Settings::Load();
    m_settings.Clamp();

    // Yüksek kontrast, OSD paleti kurulmadan ÖNCE okunmalı: BuildOsdConfig ilk
    // çağrısında bu bayrağı kullanıyor (madde 29).
    m_highContrast = HighContrastActive();
    // Rozetin başlangıç tuşu kullanıcının birincil tuşudur; ilk tuş değişiminde
    // OnLockChanged bunu değişen tuşla günceller.
    m_badgeKey = m_settings.trayIconKey;

    // Dil, kaynaklardan metin okuyan her şeyden önce kurulmalı.
    Loc::Initialize(hInstance, m_settings.language);

    if (!CreateHostWindow()) {
        return false;
    }

    m_taskbarCreatedMsg = TrayIcon::TaskbarCreatedMessage();
    m_showSettingsMsg = ::RegisterWindowMessageW(kShowSettingsMessageName);

    m_themeWatcher.Start(m_host, [this](AppTheme t) { OnSystemThemeChanged(t); });

    // Tepsi ikonu eklenmeden ÖNCE, sistem teması okunduktan hemen SONRA: menü
    // teması yalnızca app modundan gelir ve ilk sağ tık Ayarlar'dan önce olabilir.
    WinDark::Initialize();
    ApplyProcessAppMode();
    WinDark::ApplyToWindow(m_host, EffectiveTheme() == AppTheme::Dark);

    // OSD zinciri açılışta hazırlanır; ilk tuş basımında ~200 ms D3D kurulum
    // gecikmesi fark edilir (spec §6). Kurulamazsa uygulama tray modunda sürer.
    if (!m_osd.Create(hInstance)) {
        LogV(L"OsdWindow::Create başarısız — yalnızca tray modunda devam ediliyor");
    }
    // Yapılandırma ve (gerekiyorsa) monitör başına ek pencereler.
    RebuildOsdWindows();
    m_osd.OnThemeChanged(EffectiveTheme());
    for (const auto& win : m_extraOsd) {
        if (win) {
            win->OnThemeChanged(EffectiveTheme());
        }
    }

    if (!m_tray.Add(hInstance, m_host, [this](int c) { OnTrayCommand(c); })) {
        LogV(L"TrayIcon::Add başarısız");
    }
    RefreshTray();

    m_sessionNotifyRegistered =
        ::WTSRegisterSessionNotification(m_host, NOTIFY_FOR_THIS_SESSION) != FALSE;

    m_keyMonitor.Start(m_host, [this](LockKey k, LockState s) { OnLockChanged(k, s); });
    // Klavye düzeni izleyicisi kendi zamanlayıcısını KURMAZ; aynı TIMER_POLL
    // tick'inde yoklanır (madde 28). Açılışta yalnızca mevcut düzeni okur.
    m_layoutMonitor.Start([this](const KeyboardLayoutInfo& l) { OnLayoutChanged(l); });

    // Kısayol varsayılan olarak KAPALIDIR; açıksa burada kaydedilir.
    ApplyHotkey();

    // Kalıcı rozet açıksa açılışta HEMEN belirmeli: kullanıcı bir tuşa basana
    // kadar beklemek "sürekli durur" sözünü tutmazdı (madde 16).
    RefreshPersistentBadge();

    // En son: modal olduğu için kullanıcı akışını bloklamaması gerekir (spec §5).
    OemCheck::WarnOnce(m_host, m_settings.oemWarningShown);
    return true;
}

int App::Run() {
    MSG msg{};
    for (;;) {
        const BOOL got = ::GetMessageW(&msg, nullptr, 0, 0);
        if (got == 0) {
            break;  // WM_QUIT
        }
        if (got == -1) {
            // Hata; döngü kırılmamalı, aksi hâlde uygulama sessizce ölür.
            LogV(L"GetMessageW hatası: %lu", ::GetLastError());
            continue;
        }
        // Modeless ayar penceresinin Tab/Ok/Esc gezinmesi buradan geçer.
        const HWND dlg = m_settingsDialog.Handle();
        if (dlg != nullptr && ::IsDialogMessageW(dlg, &msg) != FALSE) {
            continue;
        }
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    Shutdown();
    return static_cast<int>(msg.wParam);
}

void App::Shutdown() {
    // İki kez çağrılabilir: Run() sonunda ve yıkıcıda.
    m_keyMonitor.Stop();
    m_layoutMonitor.Stop();
    m_themeWatcher.Stop();
    if (m_hotkeyRegistered) {
        if (m_host != nullptr && ::IsWindow(m_host) != FALSE) {
            ::UnregisterHotKey(m_host, kHotkeyId);
        }
        m_hotkeyRegistered = false;
    }
    m_settingsDialog.Close();
    m_tray.Remove();
    m_extraOsd.clear();   // unique_ptr yıkıcıları Destroy'u çağırır
    m_osd.Destroy();
    if (m_sessionNotifyRegistered) {
        if (m_host != nullptr && ::IsWindow(m_host) != FALSE) {
            ::WTSUnRegisterSessionNotification(m_host);
        }
        m_sessionNotifyRegistered = false;
    }
}

// ---------------------------------------------------------------------------
// Mesaj yönlendirme
// ---------------------------------------------------------------------------

LRESULT App::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    // RegisterWindowMessage kimlikleri çalışma zamanı değeri: switch dışında.
    if (m_taskbarCreatedMsg != 0 && msg == m_taskbarCreatedMsg) {
        m_tray.OnTaskbarCreated();
        RefreshTray();
        return 0;
    }
    if (m_showSettingsMsg != 0 && msg == m_showSettingsMsg) {
        ShowSettings();
        return 0;
    }

    switch (msg) {
        case WM_TIMER:
            if (wParam == TIMER_POLL) {
                m_keyMonitor.Tick();
                // AYNI tick, YENİ İŞ PARÇACIĞI YOK (madde 28). Ayar kapalıyken
                // izleyici hiç sorgulanmaz: kullanmayan kullanıcı bedel ödemez.
                if (m_settings.watchKeyboardLayout) {
                    m_layoutMonitor.Tick();
                }
                return 0;
            }
            break;

        case WM_SETTINGCHANGE: {
            m_themeWatcher.OnSettingChange(reinterpret_cast<LPCWSTR>(lParam));
            // Yüksek kontrast açılıp kapandığında sistem SPI_SETHIGHCONTRAST
            // yayınlar (madde 29). Bazı sürümler eylem kodu yerine yalnızca
            // "HighContrast" alan adını gönderdiği için ikisi de kabul edilir.
            const auto* area = reinterpret_cast<const wchar_t*>(lParam);
            const bool hcBroadcast =
                (wParam == static_cast<WPARAM>(SPI_SETHIGHCONTRAST)) ||
                (area != nullptr && _wcsicmp(area, L"HighContrast") == 0);
            if (hcBroadcast) {
                OnHighContrastMaybeChanged();
            }
            break;
        }

        case WM_KLI_THEMENOTIFY:
            m_themeWatcher.OnRegistryNotify();
            return 0;

        case WM_KLI_TRAYICON:
            m_tray.OnTrayMessage(wParam, lParam);
            return 0;

        case WM_KLI_SHOWSETTINGS:
            ShowSettings();
            return 0;

        case WM_HOTKEY:
            if (wParam == static_cast<WPARAM>(kHotkeyId)) {
                OnHotkey();
                return 0;
            }
            break;

        case WM_KLI_RESYNC:
        case WM_WTSSESSION_CHANGE:
            // Sessiz senkron: OSD gösterilmez, yoksa oturum açılışında sahte OSD çıkar.
            m_keyMonitor.Resync();
            m_layoutMonitor.Resync();
            RefreshTray();
            return 0;

        case WM_POWERBROADCAST:
            // static_cast: PBT_* int, wParam işaretsiz — /W4 uyarısı çıkmasın.
            if (wParam == static_cast<WPARAM>(PBT_APMRESUMEAUTOMATIC) ||
                wParam == static_cast<WPARAM>(PBT_APMRESUMESUSPEND)) {
                m_keyMonitor.Resync();
                m_layoutMonitor.Resync();
                RefreshTray();
            }
            return TRUE;

        case WM_DISPLAYCHANGE:
            // Monitör eklenmiş/çıkarılmış olabilir: "tüm ekranlar" kipinde pencere
            // sayısı ve sabitlenen tutamaçlar yeniden hesaplanmak zorunda, yoksa
            // artık var olmayan bir ekrana çakılı pencere kalır.
            RebuildOsdWindows();
            m_osd.OnDpiOrMonitorChanged();
            for (const auto& win : m_extraOsd) {
                if (win) {
                    win->OnDpiOrMonitorChanged();
                }
            }
            break;

        case WM_COMMAND:
            // Autostart durumu registry/StartupTask gerçeğidir; karar App'te.
            if (LOWORD(wParam) == kCmdAutostart) {
                Autostart::SetEnabled(!Autostart::IsEnabled());
                const HWND dlg = m_settingsDialog.Handle();
                if (dlg != nullptr) {
                    const HWND chk = ::GetDlgItem(dlg, IDC_CHK_AUTOSTART);
                    if (chk != nullptr) {
                        ::SendMessageW(chk, BM_SETCHECK,
                                       Autostart::IsEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
                    }
                }
                RefreshTray();
                return 0;
            }
            if (LOWORD(wParam) == kCmdPickPosition) {
                BeginPickPosition();
                return 0;
            }
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(m_host, msg, wParam, lParam);
}

// Callback bölümü (OnLockChanged / OnLayoutChanged / ForegroundAppExcluded /
// OnHighContrastMaybeChanged / OnSystemThemeChanged) AppEvents.cpp içindedir
// (400 satır sınırı, spec §9).

// OnTrayCommand / OnTrayLeftClick / OnSettingsApplied / ApplySettingsToModules /
// RefreshTray / BuildOsdConfig / ShowSettings / ShowAbout AppCommands.cpp
// içindedir (400 satır sınırı, spec §9).

AppTheme App::EffectiveTheme() const {
    return m_settings.ResolveTheme(m_themeWatcher.Current());
}

void App::ApplyProcessAppMode() const {
    WinDark::SetAppMode(EffectiveTheme() == AppTheme::Dark ? WinDark::AppMode::ForceDark
                                                           : WinDark::AppMode::ForceLight);
}

}  // namespace kli
