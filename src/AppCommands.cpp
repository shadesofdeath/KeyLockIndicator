// AppCommands.cpp — App'in kullanıcı eylemleri: tepsi komutları, ayarların
// modüllere uygulanması, alt pencerelerin açılması.
//
// App sınıfının parçasıdır; bölünme yalnızca dosya başına 400 satır sınırı
// içindir (spec §9). App.cpp pencere/yaşam döngüsü/mesaj yönlendirmeyi tutar,
// bu dosya "kullanıcı ne istedi" tarafını. Yeni bir katman değildir.
#include "App.h"

#include "Localization.h"
#include "Messages.h"
#include "MonitorUtil.h"
#include "resource.h"

#include <utility>

namespace kli {

// ---------------------------------------------------------------------------
// Tepsi komutları
// ---------------------------------------------------------------------------

void App::OnTrayCommand(int commandId) {
    switch (commandId) {
        case 0:
            // Sol tık HİÇBİR eylemde tuşu değiştirmez (spec §4.5); yalnızca ne
            // gösterileceği ayardan gelir.
            OnTrayLeftClick();
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

void App::OnTrayLeftClick() {
    switch (m_settings.trayLeftClick) {
        case TrayClickAction::ShowOsd:
            ShowOsdOnTargets(m_settings.trayIconKey,
                             m_keyMonitor.Current().Get(m_settings.trayIconKey));
            break;
        case TrayClickAction::OpenSettings:
            ShowSettings();
            break;
        case TrayClickAction::None:
            break;
    }
}

void App::OnSettingsApplied(const Settings& next) {
    // Karşılaştırmalar m_settings ÜZERİNE YAZILMADAN önce yapılmalı.
    const bool languageChanged = (next.language != m_settings.language);
    const AppTheme themeBefore = EffectiveTheme();

    if (languageChanged) {
        Loc::SetLanguage(next.language);
    }
    m_settings = next;
    m_settings.Save();
    ApplySettingsToModules(true);

    // Açık pencerelerin kendisi ApplySettingsToModules'ün kapsamında değil: o
    // yalnızca OSD/tepsi/app modunu günceller. Tema ve dil ise pencerenin kendi
    // çerçevesini ve etiketlerini de değiştirir, bu yüzden burada bildirilir.
    //
    // Tema YALNIZCA gerçekten değiştiyse uygulanır: her kaydırıcı adımı Emit
    // ürettiği için koşulsuz çağrı sürükleme boyunca tüm diyaloğu yeniden boyar.
    const AppTheme themeAfter = EffectiveTheme();
    if (themeAfter != themeBefore) {
        m_settingsDialog.OnThemeChanged(themeAfter);
        m_aboutDialog.OnThemeChanged(themeAfter);
    }
    if (languageChanged) {
        m_settingsDialog.OnLanguageChanged();
    }
}

// ---------------------------------------------------------------------------
// Ayarların modüllere uygulanması
// ---------------------------------------------------------------------------

void App::ApplySettingsToModules(bool trayIconMayChange) {
    // RefreshTray koşulsuz tazeler; bayrak yalnızca çağıranın niyetini belgeler.
    (void)trayIconMayChange;

    // Tema kipi ayarlardan da değişebilir (Sistem/Açık/Koyu); menü buna bağlı.
    ApplyProcessAppMode();
    // Pencere kümesi + yapılandırma tek yerden: monitör hedefi de bir ayardır ve
    // "anında etki" sözü pencere sayısının da anında değişmesini gerektirir.
    RebuildOsdWindows();

    const AppTheme theme = EffectiveTheme();
    m_osd.OnThemeChanged(theme);
    if (!m_settings.showOsd) {
        m_osd.HideImmediate();
    }
    for (const auto& win : m_extraOsd) {
        if (!win) {
            continue;
        }
        win->OnThemeChanged(theme);
        if (!m_settings.showOsd) {
            win->HideImmediate();
        }
    }
    ApplyHotkey();
    RefreshTray();
    // Rozet kipi yeni açıldıysa ya da kip açıkken bir ayar değiştiyse kartın
    // hemen görünmesi/tazelenmesi gerekir; hiçbir tuş basımı beklenmez.
    RefreshPersistentBadge();
}

void App::RefreshPersistentBadge() {
    if (!m_settings.showOsd || !m_settings.persistentBadge) {
        return;   // kip kapalı: OsdWindow::SetConfig zaten rozeti indirdi
    }
    // İstisna listesi rozet kipinde de geçerlidir (madde 18). Rozet kendiliğinden
    // inmediği için, dışlanan uygulama ön plandayken açıkça gizlenir; kullanıcı
    // o uygulamadan çıkıp bir tuşa bastığında OnLockChanged rozeti geri getirir.
    if (ForegroundAppExcluded()) {
        m_osd.HideImmediate();
        for (const auto& win : m_extraOsd) {
            if (win) {
                win->HideImmediate();
            }
        }
        return;
    }
    // İzlenmeyen bir tuşu kalıcı olarak göstermek yanıltıcı olurdu (kullanıcı o
    // tuşu takip etmek istemediğini söylemiş); birincil tuşa düşülür.
    if (!m_settings.Watches(m_badgeKey)) {
        m_badgeKey = m_settings.trayIconKey;
    }
    ShowOsdOnTargets(m_badgeKey, m_keyMonitor.Current().Get(m_badgeKey));
}

void App::RebuildOsdWindows() {
    OsdConfig cfg = BuildOsdConfig();

    if (m_settings.monitorTarget != MonitorTarget::All) {
        // Tek pencereli kipler: ek pencereler yıkılır, sabitleme kalkar ve konum
        // yine imleç/ana monitör mantığından gelir.
        m_extraOsd.clear();
        m_osd.SetConfig(cfg);
        return;
    }

    const std::vector<HMONITOR> monitors = MonitorUtil::All();   // en az bir öğe

    // Önce fazlalık atılır: monitör çıkarıldığında artık var olmayan bir ekrana
    // çakılı pencere kalmasın.
    if (m_extraOsd.size() + 1 > monitors.size()) {
        m_extraOsd.resize(monitors.size() - 1);
    }
    while (m_extraOsd.size() + 1 < monitors.size()) {
        auto win = std::make_unique<OsdWindow>();
        if (!win->Create(m_instance)) {
            LogV(L"Ek OSD penceresi oluşturulamadı — kalan monitörler atlanıyor");
            break;
        }
        // Create cihaz zincirini hemen kurar; HideImmediate boşta bırakma
        // sayacını başlatır, böylece kullanılmayan ekranların sürücü yığınları
        // birkaç saniye içinde geri verilir (bkz. Messages.h ölçümleri).
        win->HideImmediate();
        m_extraOsd.push_back(std::move(win));
    }

    // Her pencere kendi ekranına çakılır; hiçbiri imlece bakmaz.
    cfg.pinnedMonitor = monitors[0];
    m_osd.SetConfig(cfg);
    for (size_t i = 0; i < m_extraOsd.size(); ++i) {
        OsdConfig extra = cfg;
        extra.pinnedMonitor = monitors[i + 1];
        m_extraOsd[i]->SetConfig(extra);
    }
}

void App::ShowOsdOnTargets(LockKey key, bool isOn) {
    m_osd.Show(key, isOn);
    for (const auto& win : m_extraOsd) {
        if (win) {
            win->Show(key, isOn);
        }
    }
}

// ---------------------------------------------------------------------------
// Global kısayol
// ---------------------------------------------------------------------------

void App::ApplyHotkey() {
    if (m_host == nullptr) {
        return;
    }
    // Önce KOŞULSUZ kaldırılır: kombinasyon değişmiş olabilir ve eski kayıt
    // sistemde kalırsa hem tuşu kaçırır hem yeni kayıt kendi kendine çakışır.
    if (m_hotkeyRegistered) {
        ::UnregisterHotKey(m_host, kHotkeyId);
        m_hotkeyRegistered = false;
    }

    if (!m_settings.hotkeyEnabled) {
        m_settingsDialog.SetHotkeyWarning(false);
        return;
    }
    // Değiştiricisiz kısayol tüm sistemde o tuşu bize çeker; kaydedilmez.
    if (m_settings.hotkeyVk == 0 || m_settings.hotkeyMods == 0) {
        LogV(L"Kısayol eksik (mods=%u vk=%u) — kaydedilmedi", m_settings.hotkeyMods,
             m_settings.hotkeyVk);
        m_settingsDialog.SetHotkeyWarning(true);
        return;
    }

    // MOD_NOREPEAT: tuş basılı tutulunca OSD saniyede onlarca kez tetiklenmesin.
    const UINT mods = m_settings.hotkeyMods | MOD_NOREPEAT;
    m_hotkeyRegistered =
        ::RegisterHotKey(m_host, kHotkeyId, mods, m_settings.hotkeyVk) != FALSE;
    if (!m_hotkeyRegistered) {
        // En yaygın hata ERROR_HOTKEY_ALREADY_REGISTERED'dır: kombinasyonu başka
        // bir uygulama tutuyor. Sessiz kalmak yerine Ayarlar'da görünür uyarı çıkar.
        LogV(L"RegisterHotKey başarısız (hata %lu)", ::GetLastError());
    }
    m_settingsDialog.SetHotkeyWarning(!m_hotkeyRegistered);
}

void App::OnHotkey() {
    // OSD kartı tasarımı gereği tek seferde TEK tuş gösterir; kısayol da bu yüzden
    // kullanıcının birincil tuşunu ("Bildirim simgesi" ayarı) gösterir — tepsiye
    // sol tıklamakla aynı davranış. showOsd bilerek sorgulanmaz: kısayol açık bir
    // kullanıcı isteğidir, tıpkı tepsi tıklaması gibi.
    ShowOsdOnTargets(m_settings.trayIconKey,
                     m_keyMonitor.Current().Get(m_settings.trayIconKey));
}

void App::RefreshTray() {
    m_tray.SetMenuState(m_settings.showOsd, Autostart::IsEnabled(),
                        Autostart::IsDisabledByPolicy());
    m_tray.Update(m_keyMonitor.Current(), m_settings.trayIconKey, EffectiveTheme());
}

OsdConfig App::BuildOsdConfig() const {
    // Serbest konum oranı iki başlıkta ayrı ayrı tanımlıdır (spec §2: OsdWindow
    // Settings'i tanıyamaz); ölçekler ayrışırsa kart yanlış yere düşer.
    static_assert(Settings::kMaxCustomPos == kCustomRangeMax,
                  "Serbest konum ölçeği Settings.h ve OsdWindow.h'de aynı olmalı");
    // Görünüm kipi de iki başlıkta ayrı tanımlı (aynı gerekçe); sayısal eşlik
    // burada kilitlenir, ileride araya bir değer eklenirse derleme durur.
    static_assert(static_cast<int>(OsdViewMode::IconText) == static_cast<int>(OsdView::IconText) &&
                      static_cast<int>(OsdViewMode::IconOnly) == static_cast<int>(OsdView::IconOnly) &&
                      static_cast<int>(OsdViewMode::Bar) == static_cast<int>(OsdView::Bar),
                  "OsdViewMode ile OsdView sayısal olarak birebir olmalı");

    OsdConfig cfg{};
    cfg.durationMs = m_settings.osdDurationMs;
    // OsdPlacement ile OsdPositionMode sayısal olarak birebir; çeviri App'te (spec §2).
    cfg.placement = static_cast<OsdPlacement>(static_cast<int>(m_settings.osdPosition));
    cfg.topMarginDip = m_settings.osdTopMarginDip;
    cfg.customX = m_settings.osdCustomX;
    cfg.customY = m_settings.osdCustomY;
    // OsdWindow yalnızca "imleç mi, ana monitör mü" ikilisini bilir; "hepsi"
    // kipi pencere SAYISIYLA çözülür (RebuildOsdWindows), pencerenin içinde değil.
    cfg.primaryMonitorOnly = (m_settings.monitorTarget == MonitorTarget::Primary);
    cfg.suppressFullscreen = m_settings.suppressFullscreen;
    cfg.cardAlpha = m_settings.CardAlphaFor(EffectiveTheme());
    cfg.scale = m_settings.OsdScale();
    cfg.view = static_cast<OsdView>(static_cast<int>(m_settings.osdViewMode));
    // showOsd ile birleştirilir: OSD tamamen kapalıyken "kalıcı" kipin açık
    // kalması, OsdWindow'un rozeti geri getirmesine yol açardı.
    cfg.persistent = m_settings.persistentBadge && m_settings.showOsd;
    cfg.highContrast = m_highContrast;
    return cfg;
}

void App::BeginPickPosition() {
    if (m_osd.Handle() == nullptr) {
        LogV(L"Konum seçme kipi: OSD penceresi yok");
        return;
    }
    m_settingsDialog.SetPickingHint(true);
    m_osd.BeginPositionPicking([this](bool committed, unsigned x, unsigned y) {
        m_settingsDialog.SetPickingHint(false);
        if (!committed) {
            return;   // Esc / sağ tık: konum değişmez
        }
        // Sürükleme kendiliğinden "Özel" konum kipine geçirir; kullanıcının ayrıca
        // açılır listeden seçmesi gerekmez (seçtiği yer zaten budur).
        m_settings.osdPosition = OsdPositionMode::Custom;
        m_settings.osdCustomX = x;
        m_settings.osdCustomY = y;
        m_settings.Clamp();
        m_settings.Save();
        ApplySettingsToModules(false);
        if (m_settingsDialog.IsOpen()) {
            m_settingsDialog.SyncFrom(m_settings);
        }
    });
}

// ---------------------------------------------------------------------------
// Alt pencereler
// ---------------------------------------------------------------------------

void App::ShowSettings() {
    m_settingsDialog.Show(m_instance, m_host, m_settings,
                          [this](const Settings& s) { OnSettingsApplied(s); });
    m_settingsDialog.OnThemeChanged(EffectiveTheme());
    // Uyarı, kaydın gerçek durumundan gelir: pencere kapalıyken oluşmuş bir
    // çakışma da açılışta görünür olmalı.
    m_settingsDialog.SetHotkeyWarning(m_settings.hotkeyEnabled && !m_hotkeyRegistered);

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
    // Sahip, açıksa ayarlar penceresidir: modal pencere onu devre dışı bırakır ve
    // üzerinde ortalanır. Kapalıysa 0x0 host penceresine düşülür (AboutDialog o
    // durumda aktif monitörde ortalar).
    const HWND owner = m_settingsDialog.IsOpen() ? m_settingsDialog.Handle() : m_host;
    m_aboutDialog.Show(m_instance, owner, EffectiveTheme());
}

}  // namespace kli
