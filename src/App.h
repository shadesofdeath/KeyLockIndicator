// App.h — Uygulama koordinatörü + gizli mesaj penceresi (spec §2, §6).
//
// Tüm modüller arası bağlantı burada std::function callback'lerle kurulur.
// Modüller birbirini tanımaz.
#pragma once

#include "AboutDialog.h"
#include "Autostart.h"
#include "KeyMonitor.h"
#include "LayoutMonitor.h"
#include "OsdWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ThemeWatcher.h"
#include "TrayIcon.h"
#include "Util.h"

#include <windows.h>

#include <memory>
#include <vector>

namespace kli {

class App {
public:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    ~App();

    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Shutdown();

    // İkinci örneğin ilk örneği bulup ayarları açtırması için kullanılır.
    [[nodiscard]] static HWND FindExistingInstanceWindow();

private:
    static LRESULT CALLBACK HostWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateHostWindow();

    // Callback'ler
    void OnLockChanged(LockKey changed, LockState now);
    void OnLayoutChanged(const KeyboardLayoutInfo& layout);
    void OnSystemThemeChanged(AppTheme theme);
    void OnTrayCommand(int commandId);
    void OnTrayLeftClick();
    void OnSettingsApplied(const Settings& next);

    void ApplySettingsToModules(bool trayIconMayChange);
    void RefreshTray();
    void ShowSettings();
    void ShowAbout();

    // "Konumu ekrandan seç": OSD sürüklenebilir hâle gelir, bırakılan yer
    // OsdPositionMode::Custom olarak kaydedilir.
    void BeginPickPosition();

    // Monitör hedefine göre OSD pencerelerini kurar/yıkar ve hepsine güncel
    // yapılandırmayı verir. "Tüm ekranlar" dışındaki kiplerde ek pencere kalmaz.
    void RebuildOsdWindows();

    // Etkin hedefteki TÜM OSD pencerelerinde gösterir.
    void ShowOsdOnTargets(LockKey key, bool isOn);

    // Ön plandaki uygulama istisna listesindeyse true (madde 18). Liste boşken
    // hiçbir sistem çağrısı yapılmaz.
    [[nodiscard]] bool ForegroundAppExcluded() const;

    // Kalıcı rozet kipi açıksa rozeti güncel tuş durumuyla (yeniden) gösterir.
    // Kip kapalıysa hiçbir şey yapmaz.
    void RefreshPersistentBadge();

    // Sistem yüksek kontrast kipi değiştiyse OSD paletini yeniler (madde 29).
    void OnHighContrastMaybeChanged();

    // Global kısayolu ayarlara göre kaydeder/kaldırır ve sonucu Ayarlar'a bildirir.
    // Her ayar uygulamasında çağrılır; kısayol değişmediyse de yeniden kurulur
    // (RegisterHotKey ucuz, durum karşılaştırması ise sessiz hataya açık).
    void ApplyHotkey();
    void OnHotkey();

    // Proses genelinde tercih edilen uygulama modu. Tepsi menüsü TrackPopupMenu
    // ile çizilir ve hiçbir pencereye bağlı olmadığı için teması YALNIZCA bu
    // moddan gelir; bu yüzden Ayarlar penceresi hiç açılmasa da açılışta
    // kurulmak zorundadır.
    void ApplyProcessAppMode() const;

    [[nodiscard]] AppTheme EffectiveTheme() const;
    [[nodiscard]] OsdConfig BuildOsdConfig() const;

    HINSTANCE m_instance = nullptr;
    HWND m_host = nullptr;
    UINT m_taskbarCreatedMsg = 0;
    UINT m_showSettingsMsg = 0;
    bool m_sessionNotifyRegistered = false;
    // Kısayol GERÇEKTEN kayıtlı mı. Ayardaki "açık" ile aynı şey değildir:
    // kombinasyon başkasındaysa ayar açık kalır ama kayıt olmaz, kullanıcı da
    // Ayarlar'daki uyarıdan bunu görür.
    bool m_hotkeyRegistered = false;
    // Sistemin yüksek kontrast kipi (madde 29). Kullanıcının tema ayarından
    // BAĞIMSIZDIR ve onu ezer: yüksek kontrast açıkken palet sistemden gelir.
    bool m_highContrast = false;
    // Kalıcı rozetin gösterdiği tuş. Rozet tek seferde tek tuş gösterebildiği
    // için "en son değişen izlenen tuş" kuralı uygulanır; hiç değişim olmadıysa
    // kullanıcının birincil tuşu (Bildirim simgesi ayarı) gösterilir.
    LockKey m_badgeKey = LockKey::Caps;

    Settings m_settings{};
    KeyMonitor m_keyMonitor;
    // Klavye düzeni izleyicisi (madde 28). KENDİ zamanlayıcısı yoktur: Tick()
    // KeyMonitor ile AYNI TIMER_POLL dalından çağrılır.
    LayoutMonitor m_layoutMonitor;
    ThemeWatcher m_themeWatcher;
    // m_osd her zaman vardır ve tek pencereli kiplerde tek başına çalışır.
    // "Tüm ekranlar" kipinde m_osd listedeki İLK monitöre (ana ekran) sabitlenir,
    // m_extraOsd ise ikinciden itibaren her ekran için bir pencere tutar. Ham
    // new/delete yok: sahiplik unique_ptr'da (spec §9).
    OsdWindow m_osd;
    std::vector<std::unique_ptr<OsdWindow>> m_extraOsd;
    TrayIcon m_tray;
    SettingsDialog m_settingsDialog;
    AboutDialog m_aboutDialog;
};

}  // namespace kli
