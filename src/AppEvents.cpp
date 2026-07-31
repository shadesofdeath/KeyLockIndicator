// AppEvents.cpp — App'in izleyici ve sistem callback'leri.
//
// App sınıfının parçasıdır; bölünme yalnızca dosya başına 400 satır sınırı
// içindir (spec §9). App.cpp pencere/yaşam döngüsü/mesaj yönlendirmeyi,
// AppCommands.cpp "kullanıcı ne istedi" tarafını, bu dosya da "dışarıda ne
// değişti" tarafını tutar: tuş durumu, klavye düzeni, yüksek kontrast, tema.
// Yeni bir katman değildir.
//
// ÜÇ CALLBACK DE AYNI FİLTRE SIRASINI PAYLAŞIR ve sıra bilinçlidir:
//   1. tepsi ikonu / rozet takibi  — her zaman
//   2. ekran okuyucu duyurusu      — OSD ayarlarından BAĞIMSIZ
//   3. OSD filtreleri              — showOsd, "yalnızca açılırken", istisna listesi
#include "App.h"

#include "AppFilter.h"
#include "Localization.h"
#include "ScreenReader.h"

#include <string>

namespace kli {

void App::OnLockChanged(LockKey changed, LockState now) {
    RefreshTray();
    if (!m_settings.Watches(changed)) {
        return;
    }
    const bool isOn = now.Get(changed);
    // Rozet "en son değişen izlenen tuşu" gösterir; OSD kapalı olsa bile bu
    // takip sürer ki kip sonradan açıldığında rozet doğru tuşla belirsin.
    m_badgeKey = changed;

    // Duyuru, OSD'den BAĞIMSIZ bir kanaldır (madde 30): kullanıcı OSD'yi kapatmış
    // ya da "yalnızca açılırken göster" seçmiş olsa bile ekran okuyucu kullanan
    // biri için durum değişiminin duyulması gerekir. Bu yüzden aşağıdaki OSD
    // filtrelerinden ÖNCE yapılır.
    if (m_settings.announceToScreenReader) {
        std::wstring text = Loc::KeyTitle(changed);
        text += L": ";
        text += Loc::StateText(isOn);
        (void)ScreenReader::Announce(m_host, text);
    }

    if (!m_settings.showOsd) {
        return;
    }
    // "Yalnızca açılırken göster": kapatma sessiz geçer. Tepsi ikonu her iki
    // yönde de güncellendiği için durum bilgisi kaybolmaz.
    //
    // Kalıcı rozet kipinde bu filtre UYGULANMAZ: rozet zaten ekranda duruyor ve
    // kapatılan tuşta bayat bir "Açık" yazısıyla kalması düpedüz yanlış bilgi olur.
    if (m_settings.showOnlyWhenTurnedOn && !isOn && !m_settings.persistentBadge) {
        return;
    }
    // İstisna listesi (madde 18): kart gösterilmeden HEMEN ÖNCE, ön plandaki
    // uygulama sorulur. Duyuru ve tepsi ikonu bu filtreden ETKİLENMEZ — kullanıcı
    // "bu uygulamada kart çıkmasın" dedi, "durumu hiç bilmeyeyim" demedi.
    if (ForegroundAppExcluded()) {
        return;
    }
    // Aynı tick'te birden fazla tuş değişebilir; OsdWindow son geleni gösterir.
    ShowOsdOnTargets(changed, isOn);
}

// Klavye düzeni değişimi (madde 28). Kilit tuşu akışıyla aynı filtreleri
// paylaşır ama "yalnızca açılırken göster" burada anlamsızdır: düzenin
// açık/kapalı hâli yoktur.
void App::OnLayoutChanged(const KeyboardLayoutInfo& layout) {
    if (!m_settings.watchKeyboardLayout) {
        return;   // izleyici zaten sorgulanmıyor; güvenlik ağı
    }
    LogV(L"Klavye düzeni değişti: %s / %s", layout.code.c_str(), layout.name.c_str());

    if (m_settings.announceToScreenReader) {
        const std::wstring& text = layout.name.empty() ? layout.code : layout.name;
        (void)ScreenReader::Announce(m_host, text);
    }

    if (!m_settings.showOsd || ForegroundAppExcluded()) {
        return;
    }
    // Kalıcı rozet kipinde OsdWindow bu çağrıyı kendisi yok sayar (bkz.
    // OsdWindow::ShowKeyboardLayout gerekçesi); burada ayrıca kontrol edilmez.
    m_osd.ShowKeyboardLayout(layout.code, layout.name);
    for (const auto& win : m_extraOsd) {
        if (win) {
            win->ShowKeyboardLayout(layout.code, layout.name);
        }
    }
}

bool App::ForegroundAppExcluded() const {
    return AppFilter::ForegroundExcluded(m_settings.excludedApps);
}

void App::OnHighContrastMaybeChanged() {
    const bool next = HighContrastActive();
    if (next == m_highContrast) {
        return;   // alakasız bir SPI yayını ya da çifte bildirim
    }
    m_highContrast = next;
    LogV(L"Yüksek kontrast kipi %s", next ? L"açıldı" : L"kapandı");
    // Palet OsdConfig üzerinden taşınıyor; tek yol ayarların yeniden uygulanması.
    ApplySettingsToModules(false);
}

void App::OnSystemThemeChanged(AppTheme theme) {
    if (m_settings.themeMode != ThemeMode::System) {
        LogV(L"Sistem teması değişti (%d) ama ThemeMode sabit — yok sayılıyor",
             static_cast<int>(theme));
        return;
    }
    ApplyProcessAppMode();  // tepsi menüsü de anında dönsün
    m_osd.OnThemeChanged(theme);
    RefreshTray();
    m_settingsDialog.OnThemeChanged(theme);
    m_aboutDialog.OnThemeChanged(theme);
}

}  // namespace kli
