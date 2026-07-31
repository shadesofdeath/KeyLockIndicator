// SettingsFields.cpp — SettingsDialog'un kontrol ⇄ Settings alan eşlemesi.
//
// SettingsDialog sınıfının parçasıdır (bkz. SettingsFields.h başlık notu).
// Buradaki dört üye fonksiyon tek bir sözleşmeyi paylaşır: yalnızca kontrollerle
// m_settings arasında veri taşırlar, hiçbir yan etki üretmezler (Emit çağıran
// tarafın işidir).
#include "SettingsDialog.h"

#include "HotkeyBox.h"
#include "Localization.h"
#include "SettingsFields.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>

#include <cstdio>
#include <string>

namespace kli {
namespace SettingsFields {
namespace {

void AddComboItem(HWND dlg, int ctrl, UINT stringId) {
    const std::wstring text = Loc::Str(stringId);
    ::SendDlgItemMessageW(dlg, ctrl, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(text.c_str()));
}

// Dil listesi TEK doğruluk kaynağından, Loc::Languages() tablosundan doldurulur.
// Öğeler KENDİ dillerindeki adla yazılır ("Deutsch", "日本語"); bu adlar arayüz
// diline göre değişmediği için kaynak dizesi değil literal taşırlar. Yalnızca
// ilk öğe ("Otomatik") çevrilir ve tabloda endonym'i nullptr'dır.
void FillLanguageCombo(HWND dlg) {
    ::SendDlgItemMessageW(dlg, IDC_CMB_LANGUAGE, CB_RESETCONTENT, 0, 0);
    const Loc::Language* langs = Loc::Languages();
    const int count = Loc::LanguageCount();
    for (int i = 0; i < count; ++i) {
        if (langs[i].endonym == nullptr) {
            AddComboItem(dlg, IDC_CMB_LANGUAGE, IDS_LANG_AUTO);
            continue;
        }
        ::SendDlgItemMessageW(dlg, IDC_CMB_LANGUAGE, CB_ADDSTRING, 0,
                              reinterpret_cast<LPARAM>(langs[i].endonym));
    }
}

}  // namespace

void InitControlRanges(HWND dlg) {
    // Süre: 800–4000 ms. Sayfa adımı 100, tik her 400 ms'de bir.
    ::SendDlgItemMessageW(dlg, IDC_SLD_DURATION, TBM_SETRANGE, FALSE,
                          MAKELPARAM(Settings::kMinDurationMs, Settings::kMaxDurationMs));
    ::SendDlgItemMessageW(dlg, IDC_SLD_DURATION, TBM_SETPAGESIZE, 0, 100);
    ::SendDlgItemMessageW(dlg, IDC_SLD_DURATION, TBM_SETTICFREQ, 400, 0);
    // Opaklık: yüzde 60–100, tik her %10'da bir.
    ::SendDlgItemMessageW(dlg, IDC_SLD_OPACITY, TBM_SETRANGE, FALSE,
                          MAKELPARAM(Settings::kMinOpacity, Settings::kMaxOpacity));
    ::SendDlgItemMessageW(dlg, IDC_SLD_OPACITY, TBM_SETTICFREQ, 10, 0);
    // Kart boyutu: yüzde 75–150, tik her %25'te bir, sayfa adımı 5.
    ::SendDlgItemMessageW(dlg, IDC_SLD_SCALE, TBM_SETRANGE, FALSE,
                          MAKELPARAM(Settings::kMinScalePercent, Settings::kMaxScalePercent));
    ::SendDlgItemMessageW(dlg, IDC_SLD_SCALE, TBM_SETPAGESIZE, 0, 5);
    ::SendDlgItemMessageW(dlg, IDC_SLD_SCALE, TBM_SETTICFREQ, 25, 0);
    // Üst boşluk: 0–400 DIP. Buddy açıkça bağlanır — şablondaki kontrol sırası
    // değişirse UDS_AUTOBUDDY sessizce yanlış kontrolü seçebilir.
    ::SendDlgItemMessageW(dlg, IDC_SPN_MARGIN, UDM_SETRANGE32, 0,
                          static_cast<LPARAM>(Settings::kMaxTopMarginDip));
    ::SendDlgItemMessageW(dlg, IDC_SPN_MARGIN, UDM_SETBUDDY,
                          reinterpret_cast<WPARAM>(::GetDlgItem(dlg, IDC_EDT_MARGIN)), 0);
    // Kısayol kutusu salt okunur bir EDIT'tir; tuş yakalama alt sınıflamayla
    // eklenir (bkz. HotkeyBox.h — comctl32 hotkey sınıfı koyu temaya uymuyor).
    HotkeyBox::Attach(::GetDlgItem(dlg, IDC_HOT_SHORTCUT));

    // İstisna listesinin çerçevesi (madde 18).
    //
    // ÖLÇÜLDÜ: diyalog yöneticisi EDIT / LISTBOX / COMBOBOX kontrollerine, şablon
    // istemese bile WS_EX_CLIENTEDGE ekler ("3D görünüm"). EDIT ve COMBOBOX'ta
    // WinDark bu çerçeveyi DarkMode_CFD ile koyultabiliyor, LISTBOX'ta ise
    // koyultamıyor: kutu, koyu temada bembeyaz bir çukur çerçeveyle kalıyor
    // (canlı pencerede exstyle 0x00000204 okundu). Bu yüzden stil kaldırılır ve
    // çerçeveyi SettingsPaint::Background tema renginde kendisi çizer.
    if (const HWND list = ::GetDlgItem(dlg, IDC_LST_EXCLUDE)) {
        const LONG_PTR ex = ::GetWindowLongPtrW(list, GWL_EXSTYLE);
        ::SetWindowLongPtrW(list, GWL_EXSTYLE,
                            ex & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE));
        // SWP_FRAMECHANGED olmadan kontrol yeni çerçevesizliğini hesaplamaz.
        ::SetWindowPos(list, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                           SWP_FRAMECHANGED);
    }
}

void FillComboBoxes(HWND dlg) {
    // Öğe sırası, ilgili enum'un tam sayı değerleriyle birebir örtüşür (Settings.h).
    // Konum ve dil listeleri bunun İSTİSNASIDIR ve ayrı doldurulur.
    const struct { int ctrl; UINT items[3]; } kLists[] = {
        {IDC_CMB_THEME,    {IDS_THEME_SYSTEM, IDS_THEME_LIGHT, IDS_THEME_DARK}},
        {IDC_CMB_TRAYKEY,  {IDS_KEY_CAPS, IDS_KEY_NUM, IDS_KEY_SCROLL}},
        {IDC_CMB_TRAYCLICK, {IDS_TRAYCLICK_OSD, IDS_TRAYCLICK_SETTINGS,
                             IDS_TRAYCLICK_NONE}},
        {IDC_CMB_MONITOR,  {IDS_MON_CURSOR, IDS_MON_PRIMARY, IDS_MON_ALL}},
        {IDC_CMB_VIEWMODE, {IDS_VIEW_ICONTEXT, IDS_VIEW_ICONONLY, IDS_VIEW_BAR}},
    };
    for (const auto& list : kLists) {
        ::SendDlgItemMessageW(dlg, list.ctrl, CB_RESETCONTENT, 0, 0);
        for (const UINT stringId : list.items) {
            AddComboItem(dlg, list.ctrl, stringId);
        }
    }

    ::SendDlgItemMessageW(dlg, IDC_CMB_POSITION, CB_RESETCONTENT, 0, 0);
    for (const PositionItem& item : kPositionItems) {
        AddComboItem(dlg, IDC_CMB_POSITION, item.stringId);
    }

    FillLanguageCombo(dlg);
}

}  // namespace SettingsFields

using namespace SettingsFields;

void SettingsDialog::ApplyLocalizedText() {
    SetWindowTextW(m_hwnd, Loc::Str(IDS_SET_TITLE).c_str());
    struct LabelText { int ctrl; UINT str; };
    constexpr LabelText kTexts[] = {
        {IDC_GRP_GENERAL, IDS_SET_GRP_GENERAL},   {IDC_CHK_SHOWOSD, IDS_SET_SHOWOSD},
        {IDC_CHK_AUTOSTART, IDS_SET_AUTOSTART},   {IDC_LBL_LANGUAGE, IDS_SET_LANGUAGE},
        {IDC_LBL_TRAYCLICK, IDS_SET_TRAYCLICK},   {IDC_GRP_KEYS, IDS_SET_GRP_KEYS},
        {IDC_CHK_WATCHCAPS, IDS_SET_WATCHCAPS},   {IDC_CHK_WATCHNUM, IDS_SET_WATCHNUM},
        {IDC_CHK_WATCHSCROLL, IDS_SET_WATCHSCROLL},
        {IDC_LBL_TRAYKEY, IDS_SET_TRAYKEY},       {IDC_GRP_OSD, IDS_SET_GRP_OSD},
        {IDC_LBL_DURATION, IDS_SET_DURATION},     {IDC_LBL_OPACITY, IDS_SET_OPACITY},
        {IDC_LBL_SCALE, IDS_SET_SCALE},           {IDC_LBL_POSITION, IDS_SET_POSITION},
        {IDC_LBL_MARGIN, IDS_SET_MARGIN},         {IDC_CHK_ONLYON, IDS_SET_ONLYON},
        {IDC_LBL_THEME, IDS_SET_THEME},           {IDC_GRP_BEHAVIOR, IDS_SET_GRP_BEHAVIOR},
        {IDC_CHK_SUPPRESSFS, IDS_SET_SUPPRESSFS}, {IDC_LBL_MONITOR, IDS_SET_MONITOR},
        {IDC_CHK_HOTKEY, IDS_SET_HOTKEY},
        {IDC_BTN_DEFAULTS, IDS_SET_DEFAULTS},     {IDC_BTN_CLOSE, IDS_SET_CLOSE},
        {IDC_LBL_HINT, IDS_SET_HINT},             {IDC_BTN_PICKPOS, IDS_SET_PICKPOS},
        {IDC_LBL_VIEWMODE, IDS_SET_VIEWMODE},     {IDC_CHK_BADGE, IDS_SET_BADGE},
        {IDC_CHK_ANNOUNCE, IDS_SET_ANNOUNCE},
        {IDC_GRP_EXCLUDE, IDS_SET_GRP_EXCLUDE},   {IDC_BTN_EXCL_ADD, IDS_SET_EXCL_ADD},
        {IDC_BTN_EXCL_DEL, IDS_SET_EXCL_DEL},     {IDC_CHK_WATCHLAYOUT, IDS_SET_WATCHLAYOUT},
        {IDC_BTN_EXPORT, IDS_SET_EXPORT},         {IDC_BTN_IMPORT, IDS_SET_IMPORT},
    };
    for (const LabelText& t : kTexts) {
        SetDlgItemTextW(m_hwnd, t.ctrl, Loc::Str(t.str).c_str());
    }
}

void SettingsDialog::LoadControls() {
    if (!m_hwnd) { return; }
    // Kontrollere yazmak bildirim doğurur; m_loading onların Emit etmesini keser.
    m_loading = true;
    SetCheck(m_hwnd, IDC_CHK_SHOWOSD, m_settings.showOsd);
    SetCheck(m_hwnd, IDC_CHK_WATCHCAPS, m_settings.watchCaps);
    SetCheck(m_hwnd, IDC_CHK_WATCHNUM, m_settings.watchNum);
    SetCheck(m_hwnd, IDC_CHK_WATCHSCROLL, m_settings.watchScroll);
    SetCheck(m_hwnd, IDC_CHK_WATCHLAYOUT, m_settings.watchKeyboardLayout);
    SetCheck(m_hwnd, IDC_CHK_SUPPRESSFS, m_settings.suppressFullscreen);
    SetCheck(m_hwnd, IDC_CHK_ONLYON, m_settings.showOnlyWhenTurnedOn);
    SetComboSel(m_hwnd, IDC_CMB_MONITOR, static_cast<int>(m_settings.monitorTarget));
    SetComboSel(m_hwnd, IDC_CMB_VIEWMODE, static_cast<int>(m_settings.osdViewMode));
    SetCheck(m_hwnd, IDC_CHK_BADGE, m_settings.persistentBadge);
    SetCheck(m_hwnd, IDC_CHK_ANNOUNCE, m_settings.announceToScreenReader);
    SetCheck(m_hwnd, IDC_CHK_HOTKEY, m_settings.hotkeyEnabled);
    HotkeyBox::Set(GetDlgItem(m_hwnd, IDC_HOT_SHORTCUT), m_settings.hotkeyMods,
                   m_settings.hotkeyVk);
    // IDC_CHK_AUTOSTART bilinçli olarak atlanır (bkz. WM_COMMAND açıklaması).
    SetTrackPos(m_hwnd, IDC_SLD_DURATION, m_settings.osdDurationMs);
    SetTrackPos(m_hwnd, IDC_SLD_OPACITY, m_settings.osdOpacity);
    SetTrackPos(m_hwnd, IDC_SLD_SCALE, m_settings.osdScalePercent);
    // UDM_SETPOS32 buddy edit metnini de günceller.
    SendDlgItemMessageW(m_hwnd, IDC_SPN_MARGIN, UDM_SETPOS32, 0,
                        static_cast<LPARAM>(m_settings.osdTopMarginDip));
    // Konum listesinde indeks = enum değeri DEĞİLDİR (bkz. SettingsFields.h).
    SetComboSel(m_hwnd, IDC_CMB_POSITION, PositionToIndex(m_settings.osdPosition));
    SetComboSel(m_hwnd, IDC_CMB_THEME, static_cast<int>(m_settings.themeMode));
    SetComboSel(m_hwnd, IDC_CMB_TRAYKEY, static_cast<int>(m_settings.trayIconKey));
    SetComboSel(m_hwnd, IDC_CMB_TRAYCLICK, static_cast<int>(m_settings.trayLeftClick));
    // Dil listesinin sırası Loc::Languages() tablosudur; indeks oradan gelir.
    SetComboSel(m_hwnd, IDC_CMB_LANGUAGE, Loc::LanguageIndex(m_settings.language));
    m_loading = false;
}

void SettingsDialog::PushFromControls() {
    if (m_loading || !m_hwnd) { return; }
    m_settings.showOsd = IsChecked(m_hwnd, IDC_CHK_SHOWOSD);
    m_settings.watchCaps = IsChecked(m_hwnd, IDC_CHK_WATCHCAPS);
    m_settings.watchNum = IsChecked(m_hwnd, IDC_CHK_WATCHNUM);
    m_settings.watchScroll = IsChecked(m_hwnd, IDC_CHK_WATCHSCROLL);
    m_settings.watchKeyboardLayout = IsChecked(m_hwnd, IDC_CHK_WATCHLAYOUT);
    m_settings.suppressFullscreen = IsChecked(m_hwnd, IDC_CHK_SUPPRESSFS);
    m_settings.showOnlyWhenTurnedOn = IsChecked(m_hwnd, IDC_CHK_ONLYON);
    m_settings.monitorTarget = static_cast<MonitorTarget>(
        ComboSelOr(m_hwnd, IDC_CMB_MONITOR, static_cast<int>(m_settings.monitorTarget)));
    m_settings.osdViewMode = static_cast<OsdViewMode>(
        ComboSelOr(m_hwnd, IDC_CMB_VIEWMODE, static_cast<int>(m_settings.osdViewMode)));
    m_settings.persistentBadge = IsChecked(m_hwnd, IDC_CHK_BADGE);
    m_settings.announceToScreenReader = IsChecked(m_hwnd, IDC_CHK_ANNOUNCE);
    m_settings.hotkeyEnabled = IsChecked(m_hwnd, IDC_CHK_HOTKEY);
    const HWND hot = GetDlgItem(m_hwnd, IDC_HOT_SHORTCUT);
    m_settings.hotkeyMods = HotkeyBox::Mods(hot);
    m_settings.hotkeyVk = HotkeyBox::Vk(hot);
    m_settings.osdDurationMs = static_cast<unsigned>(TrackPos(m_hwnd, IDC_SLD_DURATION));
    m_settings.osdOpacity = static_cast<unsigned>(TrackPos(m_hwnd, IDC_SLD_OPACITY));
    m_settings.osdScalePercent = static_cast<unsigned>(TrackPos(m_hwnd, IDC_SLD_SCALE));
    // Kullanıcı alanı boşaltmışsa updown 0 döner; aralığı Clamp zorlar.
    const int margin = static_cast<int>(
        SendDlgItemMessageW(m_hwnd, IDC_SPN_MARGIN, UDM_GETPOS32, 0, 0));
    m_settings.osdTopMarginDip = static_cast<unsigned>(margin < 0 ? 0 : margin);
    m_settings.osdPosition =
        IndexToPosition(ComboSel(m_hwnd, IDC_CMB_POSITION), m_settings.osdPosition);
    m_settings.themeMode = static_cast<ThemeMode>(
        ComboSelOr(m_hwnd, IDC_CMB_THEME, static_cast<int>(m_settings.themeMode)));
    m_settings.trayIconKey = static_cast<LockKey>(
        ComboSelOr(m_hwnd, IDC_CMB_TRAYKEY, static_cast<int>(m_settings.trayIconKey)));
    m_settings.trayLeftClick = static_cast<TrayClickAction>(
        ComboSelOr(m_hwnd, IDC_CMB_TRAYCLICK, static_cast<int>(m_settings.trayLeftClick)));
    // CB_ERR (-1) gelirse mevcut dil korunur; LanguageCodeAt sınırları kendisi
    // kontrol eder ama -1 için "auto" döndürürdü ve seçim sessizce sıfırlanırdı.
    if (const int langIndex = ComboSel(m_hwnd, IDC_CMB_LANGUAGE); langIndex >= 0) {
        m_settings.language = Loc::LanguageCodeAt(langIndex);
    }
    m_settings.Clamp();
}

void SettingsDialog::UpdateValueLabels() {
    if (!m_hwnd) { return; }
    struct ValueLabel { int slider; int label; UINT unit; };
    // Biçim tek yerde: "<değer> <birim>". Üç kaydırıcı da aynı kalıbı kullanır.
    constexpr ValueLabel kLabels[] = {
        {IDC_SLD_DURATION, IDC_VAL_DURATION, IDS_UNIT_MS},
        {IDC_SLD_OPACITY, IDC_VAL_OPACITY, IDS_UNIT_PERCENT},
        {IDC_SLD_SCALE, IDC_VAL_SCALE, IDS_UNIT_PERCENT},
    };
    wchar_t buf[64];
    for (const ValueLabel& v : kLabels) {
        _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d %s", TrackPos(m_hwnd, v.slider),
                     Loc::Str(v.unit).c_str());
        SetDlgItemTextW(m_hwnd, v.label, buf);
    }

    // Kısayol kutusu yalnızca özellik açıkken düzenlenebilir; kapalıyken kutuya
    // yazmak kullanıcıya çalışan bir kısayol kurduğunu düşündürürdü.
    if (const HWND hot = GetDlgItem(m_hwnd, IDC_HOT_SHORTCUT)) {
        EnableWindow(hot, IsChecked(m_hwnd, IDC_CHK_HOTKEY) ? TRUE : FALSE);
    }

    // Kalıcı rozet kipinde kart hiç kaybolmaz, dolayısıyla "görünme süresi"
    // ayarının hiçbir etkisi yoktur; kaydırıcı pasifleşir ki kullanıcı etkisiz
    // bir denetimle uğraşmasın (değer korunur, kip kapanınca geri döner).
    if (const HWND duration = GetDlgItem(m_hwnd, IDC_SLD_DURATION)) {
        EnableWindow(duration, IsChecked(m_hwnd, IDC_CHK_BADGE) ? FALSE : TRUE);
    }
}

}  // namespace kli
