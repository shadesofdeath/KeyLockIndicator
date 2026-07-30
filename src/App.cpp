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

namespace kli {
namespace {

// Proses genelinde tercih edilen uygulama modu. Tepsi menüsü TrackPopupMenu ile
// çizilir ve hiçbir pencereye bağlı olmadığı için tema yalnızca bu moddan gelir;
// bu yüzden Ayarlar penceresi hiç açılmasa da açılışta kurulmak zorundadır.
void ApplyProcessAppMode(bool dark) {
    WinDark::SetAppMode(dark ? WinDark::AppMode::ForceDark : WinDark::AppMode::ForceLight);
}

}  // namespace

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
    const bool dark = (EffectiveTheme() == AppTheme::Dark);
    ApplyProcessAppMode(dark);
    WinDark::ApplyToWindow(m_host, dark);

    // OSD zinciri açılışta hazırlanır; ilk tuş basımında ~200 ms D3D kurulum
    // gecikmesi fark edilir (spec §6). Kurulamazsa uygulama tray modunda sürer.
    if (!m_osd.Create(hInstance)) {
        LogV(L"OsdWindow::Create başarısız — yalnızca tray modunda devam ediliyor");
    }
    m_osd.SetConfig(BuildOsdConfig());
    m_osd.OnThemeChanged(EffectiveTheme());

    if (!m_tray.Add(hInstance, m_host, [this](int c) { OnTrayCommand(c); })) {
        LogV(L"TrayIcon::Add başarısız");
    }
    RefreshTray();

    m_sessionNotifyRegistered =
        ::WTSRegisterSessionNotification(m_host, NOTIFY_FOR_THIS_SESSION) != FALSE;

    m_keyMonitor.Start(m_host, [this](LockKey k, LockState s) { OnLockChanged(k, s); });

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
    m_themeWatcher.Stop();
    m_settingsDialog.Close();
    m_tray.Remove();
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
                return 0;
            }
            break;

        case WM_SETTINGCHANGE:
            m_themeWatcher.OnSettingChange(reinterpret_cast<LPCWSTR>(lParam));
            break;

        case WM_KLI_THEMENOTIFY:
            m_themeWatcher.OnRegistryNotify();
            return 0;

        case WM_KLI_TRAYICON:
            m_tray.OnTrayMessage(wParam, lParam);
            return 0;

        case WM_KLI_SHOWSETTINGS:
            ShowSettings();
            return 0;

        case WM_KLI_RESYNC:
        case WM_WTSSESSION_CHANGE:
            // Sessiz senkron: OSD gösterilmez, yoksa oturum açılışında sahte OSD çıkar.
            m_keyMonitor.Resync();
            RefreshTray();
            return 0;

        case WM_POWERBROADCAST:
            // static_cast: PBT_* int, wParam işaretsiz — /W4 uyarısı çıkmasın.
            if (wParam == static_cast<WPARAM>(PBT_APMRESUMEAUTOMATIC) ||
                wParam == static_cast<WPARAM>(PBT_APMRESUMESUSPEND)) {
                m_keyMonitor.Resync();
                RefreshTray();
            }
            return TRUE;

        case WM_DISPLAYCHANGE:
            m_osd.OnDpiOrMonitorChanged();
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
            break;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return ::DefWindowProcW(m_host, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Callback'ler
// ---------------------------------------------------------------------------

void App::OnLockChanged(LockKey changed, LockState now) {
    RefreshTray();
    if (!m_settings.showOsd || !m_settings.Watches(changed)) {
        return;
    }
    // Aynı tick'te birden fazla tuş değişebilir; OsdWindow son geleni gösterir.
    m_osd.Show(changed, now.Get(changed));
}

void App::OnSystemThemeChanged(AppTheme theme) {
    if (m_settings.themeMode != ThemeMode::System) {
        LogV(L"Sistem teması değişti (%d) ama ThemeMode sabit — yok sayılıyor",
             static_cast<int>(theme));
        return;
    }
    ApplyProcessAppMode(theme == AppTheme::Dark);  // tepsi menüsü de anında dönsün
    m_osd.OnThemeChanged(theme);
    RefreshTray();
    m_settingsDialog.OnThemeChanged(theme);
}

void App::OnTrayCommand(int commandId) {
    switch (commandId) {
        case 0:
            // Sol tık tuşu değiştirmez, yalnızca anlık OSD gösterir (spec §4.5).
            m_osd.Show(m_settings.trayIconKey,
                       m_keyMonitor.Current().Get(m_settings.trayIconKey));
            break;

        case kCmdSettings:
            ShowSettings();
            break;

        case kCmdAutostart:
            Autostart::SetEnabled(!Autostart::IsEnabled());
            RefreshTray();
            break;

        case kCmdToggleOsd:
            m_settings.showOsd = !m_settings.showOsd;
            m_settings.Save();
            ApplySettingsToModules(false);
            if (m_settingsDialog.IsOpen()) {
                m_settingsDialog.SyncFrom(m_settings);
            }
            break;

        case kCmdAbout:
            ShowAbout();
            break;

        case kCmdExit:
            // WM_DESTROY → PostQuitMessage; temizlik Run() sonunda.
            ::DestroyWindow(m_host);
            break;

        default:
            LogV(L"Bilinmeyen tray komutu: %d", commandId);
            break;
    }
}

void App::OnSettingsApplied(const Settings& next) {
    if (next.language != m_settings.language) {
        Loc::SetLanguage(next.language);
    }
    m_settings = next;
    m_settings.Save();
    ApplySettingsToModules(true);
}

// ---------------------------------------------------------------------------
// Yardımcılar
// ---------------------------------------------------------------------------

void App::ApplySettingsToModules(bool trayIconMayChange) {
    // RefreshTray koşulsuz tazeler; bayrak yalnızca çağıranın niyetini belgeler.
    (void)trayIconMayChange;

    // Tema kipi ayarlardan da değişebilir (Sistem/Açık/Koyu); menü buna bağlı.
    ApplyProcessAppMode(EffectiveTheme() == AppTheme::Dark);
    m_osd.SetConfig(BuildOsdConfig());
    m_osd.OnThemeChanged(EffectiveTheme());
    if (!m_settings.showOsd) {
        m_osd.HideImmediate();
    }
    RefreshTray();
}

void App::RefreshTray() {
    m_tray.SetMenuState(m_settings.showOsd, Autostart::IsEnabled(),
                        Autostart::IsDisabledByPolicy());
    m_tray.Update(m_keyMonitor.Current(), m_settings.trayIconKey, EffectiveTheme());
}

void App::ShowSettings() {
    m_settingsDialog.Show(m_instance, m_host, m_settings,
                          [this](const Settings& s) { OnSettingsApplied(s); });
    m_settingsDialog.OnThemeChanged(EffectiveTheme());

    // Autostart Settings'te saklanmaz; kutu gerçek kaynaktan senkronlanır.
    const HWND dlg = m_settingsDialog.Handle();
    const HWND chk = (dlg != nullptr) ? ::GetDlgItem(dlg, IDC_CHK_AUTOSTART) : nullptr;
    if (chk == nullptr) {
        return;
    }
    ::SendMessageW(chk, BM_SETCHECK,
                   Autostart::IsEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    ::EnableWindow(chk, Autostart::IsDisabledByPolicy() ? FALSE : TRUE);
}

void App::ShowAbout() {
    ::MessageBoxW(m_host, Loc::Str(IDS_ABOUT_TEXT).c_str(),
                  Loc::Str(IDS_ABOUT_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
}

AppTheme App::EffectiveTheme() const {
    return m_settings.ResolveTheme(m_themeWatcher.Current());
}

OsdConfig App::BuildOsdConfig() const {
    OsdConfig cfg{};
    cfg.durationMs = m_settings.osdDurationMs;
    // OsdPlacement ile OsdPositionMode sayısal olarak birebir; çeviri App'te (spec §2).
    cfg.placement = static_cast<OsdPlacement>(static_cast<int>(m_settings.osdPosition));
    cfg.topMarginDip = m_settings.osdTopMarginDip;
    cfg.primaryMonitorOnly = m_settings.primaryMonitorOnly;
    cfg.suppressFullscreen = m_settings.suppressFullscreen;
    cfg.cardAlpha = m_settings.CardAlphaFor(EffectiveTheme());
    return cfg;
}

}  // namespace kli
