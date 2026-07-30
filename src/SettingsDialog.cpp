// SettingsDialog.cpp — WinDark ile temalandırılmış modeless ayar penceresi (spec §4.6).
//
// "Uygula" butonu yoktur: her kontrol değişimi PushFromControls → Emit zinciriyle
// anında App'e iletilir. m_loading bayrağı, LoadControls'un kontrollere yazması
// sırasında doğan bildirimlerin geri besleme döngüsü kurmasını engeller.
// Şablon (IDD_SETTINGS) yalnızca yerleşimi taşır; etiket metinleri çalışma
// zamanında Loc::Str ile konur, böylece dil ayarı anında etki eder.
#include "SettingsDialog.h"

#include "Localization.h"
#include "Messages.h"
#include "MonitorUtil.h"
#include "SettingsPaint.h"
#include "Util.h"
#include "resource.h"

#include <WinDark/WinDark.h>

#include <windows.h>
#include <commctrl.h>

#include <cstdio>
#include <string>
#include <utility>

namespace kli {
namespace {

// --- Kontrol okuma/yazma kısaltmaları --------------------------------------
// Cast'ler burada toplanır ki çağrı yerleri /W4 daraltma uyarısı üretmesin.

int TrackPos(HWND dlg, int id) {
    return static_cast<int>(SendDlgItemMessageW(dlg, id, TBM_GETPOS, 0, 0));
}

void SetTrackPos(HWND dlg, int id, unsigned value) {
    SendDlgItemMessageW(dlg, id, TBM_SETPOS, TRUE, static_cast<LPARAM>(value));
}

bool IsChecked(HWND dlg, int id) { return IsDlgButtonChecked(dlg, id) == BST_CHECKED; }

void SetCheck(HWND dlg, int id, bool on) {
    CheckDlgButton(dlg, id, on ? BST_CHECKED : BST_UNCHECKED);
}

int ComboSel(HWND dlg, int id) {
    return static_cast<int>(SendDlgItemMessageW(dlg, id, CB_GETCURSEL, 0, 0));
}

void SetComboSel(HWND dlg, int id, int index) {
    SendDlgItemMessageW(dlg, id, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

// CB_ERR (-1) ya da beklenmeyen indeks gelirse mevcut ayar korunur.
int ComboSelOr(HWND dlg, int id, int fallback) {
    const int sel = ComboSel(dlg, id);
    return (sel >= 0 && sel <= 2) ? sel : fallback;
}

// Özel davranışı olmayan kontroller: bildirimleri doğrudan PushFromControls +
// Emit tetikler (IDC_CHK_AUTOSTART burada yoktur — bkz. WM_COMMAND).
bool IsPlainCheckbox(int id) noexcept {
    return id == IDC_CHK_SHOWOSD || id == IDC_CHK_WATCHCAPS || id == IDC_CHK_WATCHNUM ||
           id == IDC_CHK_WATCHSCROLL || id == IDC_CHK_SUPPRESSFS || id == IDC_CHK_PRIMARYONLY;
}

bool IsComboBox(int id) noexcept {
    return id == IDC_CMB_LANGUAGE || id == IDC_CMB_TRAYKEY ||
           id == IDC_CMB_POSITION || id == IDC_CMB_THEME;
}

// --- Tek seferlik kurulum --------------------------------------------------

void InitControlRanges(HWND dlg) {
    // Süre: 800–4000 ms. Sayfa adımı 100, tik her 400 ms'de bir.
    SendDlgItemMessageW(dlg, IDC_SLD_DURATION, TBM_SETRANGE, FALSE,
                        MAKELPARAM(Settings::kMinDurationMs, Settings::kMaxDurationMs));
    SendDlgItemMessageW(dlg, IDC_SLD_DURATION, TBM_SETPAGESIZE, 0, 100);
    SendDlgItemMessageW(dlg, IDC_SLD_DURATION, TBM_SETTICFREQ, 400, 0);
    // Opaklık: yüzde 60–100, tik her %10'da bir.
    SendDlgItemMessageW(dlg, IDC_SLD_OPACITY, TBM_SETRANGE, FALSE,
                        MAKELPARAM(Settings::kMinOpacity, Settings::kMaxOpacity));
    SendDlgItemMessageW(dlg, IDC_SLD_OPACITY, TBM_SETTICFREQ, 10, 0);
    // Üst boşluk: 0–400 DIP. Buddy açıkça bağlanır — şablondaki kontrol sırası
    // değişirse UDS_AUTOBUDDY sessizce yanlış kontrolü seçebilir.
    SendDlgItemMessageW(dlg, IDC_SPN_MARGIN, UDM_SETRANGE32, 0,
                        static_cast<LPARAM>(Settings::kMaxTopMarginDip));
    SendDlgItemMessageW(dlg, IDC_SPN_MARGIN, UDM_SETBUDDY,
                        reinterpret_cast<WPARAM>(GetDlgItem(dlg, IDC_EDT_MARGIN)), 0);
}

void FillComboBoxes(HWND dlg) {
    // Öğe sırası, ilgili enum'un tam sayı değerleriyle birebir örtüşür (Settings.h).
    const struct { int ctrl; UINT items[3]; } kLists[] = {
        {IDC_CMB_POSITION, {IDS_POS_TOP, IDS_POS_CENTER, IDS_POS_BOTTOM}},
        {IDC_CMB_THEME,    {IDS_THEME_SYSTEM, IDS_THEME_LIGHT, IDS_THEME_DARK}},
        {IDC_CMB_TRAYKEY,  {IDS_KEY_CAPS, IDS_KEY_NUM, IDS_KEY_SCROLL}},
        {IDC_CMB_LANGUAGE, {IDS_LANG_AUTO, IDS_LANG_TR, IDS_LANG_EN}},
    };
    for (const auto& list : kLists) {
        SendDlgItemMessageW(dlg, list.ctrl, CB_RESETCONTENT, 0, 0);
        for (const UINT stringId : list.items) {
            const std::wstring text = Loc::Str(stringId);
            SendDlgItemMessageW(dlg, list.ctrl, CB_ADDSTRING, 0,
                                reinterpret_cast<LPARAM>(text.c_str()));
        }
    }
}

void ApplyDialogIcon(HWND dlg, HINSTANCE inst) {
    if (!inst) { return; }
    // LR_SHARED: ikon sistem önbelleğinden gelir, DestroyIcon gerekmez.
    const auto load = [inst](int cx, int cy) {
        return static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                             cx, cy, LR_DEFAULTCOLOR | LR_SHARED));
    };
    const HICON big = load(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
    const HICON tiny = load(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
    if (big) { SendMessageW(dlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big)); }
    if (tiny) { SendMessageW(dlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(tiny)); }
}

// İmlecin bulunduğu monitörde ortalar. Sahip pencerenin monitörü kullanılamaz:
// sahip, App'in 0x0 boyutlu ve hiç gösterilmeyen host penceresidir; onun
// monitörü daima ana monitör çıkar ve pencere kullanıcının baktığı ekrana gelmez.
void CenterOnActiveMonitor(HWND dlg) {
    RECT rc{};
    if (!GetWindowRect(dlg, &rc)) { return; }
    const MonitorMetrics mon = MonitorUtil::Active(false);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    int x = mon.work.left + ((mon.work.right - mon.work.left) - w) / 2;
    int y = mon.work.top + ((mon.work.bottom - mon.work.top) - h) / 2;
    // Pencere çalışma alanından yüksekse ortalama negatif kayma verir; başlık
    // çubuğu erişilemez olmasın diye sol/üst kenara oturtulur.
    if (x < mon.work.left) { x = mon.work.left; }
    if (y < mon.work.top) { y = mon.work.top; }
    SetWindowPos(dlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace

SettingsDialog::~SettingsDialog() { Close(); }

void SettingsDialog::Show(HINSTANCE hInstance, HWND owner, const Settings& current,
                          ApplyCallback onApply) {
    m_instance = hInstance;
    m_onApply = std::move(onApply);
    if (m_hwnd) {
        SetForegroundWindow(m_hwnd);
        SyncFrom(current);
        return;
    }
    m_settings = current;
    // MODELESS: mesaj döngüsü App tarafında IsDialogMessageW ile beslenir.
    const HWND dlg = CreateDialogParamW(hInstance, MAKEINTRESOURCEW(IDD_SETTINGS), owner,
                                        DlgProc, reinterpret_cast<LPARAM>(this));
    if (!dlg) {
        LogV(L"CreateDialogParamW(IDD_SETTINGS) başarısız: %lu", GetLastError());
        return;
    }
    ShowWindow(dlg, SW_SHOW);
}

void SettingsDialog::Close() {
    if (!m_hwnd) { return; }
    // Kolu önce sıfırla: DestroyWindow sırasında gelen mesajlar yeniden Close
    // çağırırsa özyineleme oluşmasın.
    const HWND dlg = m_hwnd;
    m_hwnd = nullptr;
    DestroyWindow(dlg);
}

INT_PTR CALLBACK SettingsDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INITDIALOG) {
        SetWindowLongPtrW(hwnd, DWLP_USER, static_cast<LONG_PTR>(lParam));
    }
    auto* self = reinterpret_cast<SettingsDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    if (!self) { return FALSE; }
    if (msg == WM_INITDIALOG) {
        self->m_hwnd = hwnd;
    }
    return self->HandleMessage(msg, wParam, lParam);
}

INT_PTR SettingsDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INITDIALOG: {
            // Trackbar/updown pencere sınıflarının kayıtlı olduğunu garanti eder.
            INITCOMMONCONTROLSEX icc{};
            icc.dwSize = static_cast<DWORD>(sizeof(icc));
            icc.dwICC = ICC_BAR_CLASSES | ICC_UPDOWN_CLASS | ICC_STANDARD_CLASSES;
            InitCommonControlsEx(&icc);
            ApplyDialogIcon(m_hwnd, m_instance);
            ApplyLocalizedText();
            InitControlRanges(m_hwnd);
            FillComboBoxes(m_hwnd);
            // Proses genelindeki app modu App::Initialize'da kuruldu (tepsi menüsü
            // de ona bağlı); burada yalnızca pencere ve kontroller temalandırılır.
            const bool dark = (m_theme == AppTheme::Dark);
            WinDark::ApplyToWindow(m_hwnd, dark);
            WinDark::ApplyToChildren(m_hwnd, dark);
            LoadControls();
            UpdateValueLabels();
            CenterOnActiveMonitor(m_hwnd);
            // Konumlandırmadan SONRA: başlık genişlikleri pencerenin son DPI'ında
            // ölçülsün. Bölüm çizgileri de bu daraltılmış kenardan başlar.
            SettingsPaint::LayoutSectionHeaders(m_hwnd);
            return TRUE;  // odak varsayılan kontrolde kalır (SetFocus yapılmadı)
        }
        case WM_HSCROLL: {
            // Yalnızca iki trackbar'dan gelir; lParam kontrolün kolu.
            const HWND ctrl = reinterpret_cast<HWND>(lParam);
            const int id = ctrl ? GetDlgCtrlID(ctrl) : 0;
            if (id != IDC_SLD_DURATION && id != IDC_SLD_OPACITY) { break; }
            if (id == IDC_SLD_DURATION) {
                // Süreyi 50 ms'nin katına oturt: etiketteki değer okunabilir kalsın.
                const int pos = static_cast<int>(SendMessageW(ctrl, TBM_GETPOS, 0, 0));
                const int snapped = ((pos + 25) / 50) * 50;
                if (snapped != pos) { SendMessageW(ctrl, TBM_SETPOS, TRUE, snapped); }
            }
            PushFromControls();
            UpdateValueLabels();
            Emit();
            return TRUE;
        }
        case WM_COMMAND: {
            const int id = static_cast<int>(LOWORD(wParam));
            const int code = static_cast<int>(HIWORD(wParam));
            if (id == IDC_BTN_CLOSE || id == IDCANCEL) {
                Close();
                return TRUE;
            }
            if (id == IDC_BTN_DEFAULTS && code == BN_CLICKED) {
                m_settings.ResetToDefaults();
                LoadControls();
                UpdateValueLabels();
                Emit();
                return TRUE;
            }
            if (id == IDC_CHK_AUTOSTART && code == BN_CLICKED) {
                // Autostart durumu Settings'te taşınmaz (spec §4.7: Run anahtarı
                // ya da StartupTask). Bu yüzden kutu buradan okunmaz; komut
                // sahibe — App'in gizli host penceresine — iletilir. App gerçek
                // durumu değiştirip yanıt olarak kutuya BM_SETCHECK gönderir;
                // LoadControls bu kontrole hiç dokunmaz.
                if (const HWND owner = GetWindow(m_hwnd, GW_OWNER)) {
                    SendMessageW(owner, WM_COMMAND, MAKEWPARAM(kCmdAutostart, 0), 0);
                }
                return TRUE;
            }
            if ((code == BN_CLICKED && IsPlainCheckbox(id)) ||
                (code == CBN_SELCHANGE && IsComboBox(id)) ||
                (code == EN_CHANGE && id == IDC_EDT_MARGIN)) {
                PushFromControls();
                Emit();
                return TRUE;
            }
            break;
        }
        case WM_ERASEBKGND:
            // Zemin + bölüm ayırıcıları; GROUPBOX çerçevelerinin yerini alır.
            SettingsPaint::Background(m_hwnd, reinterpret_cast<HDC>(wParam),
                                      m_theme == AppTheme::Dark);
            return TRUE;  // zemin silindi, DefDlgProc tekrar boyamasın
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            // WinDark paleti + bölüm başlığı vurgusu; ayrımı SettingsPaint yapar.
            const HBRUSH b =
                SettingsPaint::OnCtlColor(msg, wParam, lParam, m_theme == AppTheme::Dark);
            if (b) { return reinterpret_cast<INT_PTR>(b); }
            break;
        }
        case WM_CLOSE:
            Close();
            return TRUE;
        case WM_DESTROY:
            m_hwnd = nullptr;
            break;
        default:
            break;
    }
    return FALSE;
}

void SettingsDialog::ApplyLocalizedText() {
    SetWindowTextW(m_hwnd, Loc::Str(IDS_SET_TITLE).c_str());
    struct LabelText { int ctrl; UINT str; };
    constexpr LabelText kTexts[] = {
        {IDC_GRP_GENERAL, IDS_SET_GRP_GENERAL},   {IDC_CHK_SHOWOSD, IDS_SET_SHOWOSD},
        {IDC_CHK_AUTOSTART, IDS_SET_AUTOSTART},   {IDC_LBL_LANGUAGE, IDS_SET_LANGUAGE},
        {IDC_GRP_KEYS, IDS_SET_GRP_KEYS},         {IDC_CHK_WATCHCAPS, IDS_SET_WATCHCAPS},
        {IDC_CHK_WATCHNUM, IDS_SET_WATCHNUM},     {IDC_CHK_WATCHSCROLL, IDS_SET_WATCHSCROLL},
        {IDC_LBL_TRAYKEY, IDS_SET_TRAYKEY},       {IDC_GRP_OSD, IDS_SET_GRP_OSD},
        {IDC_LBL_DURATION, IDS_SET_DURATION},     {IDC_LBL_OPACITY, IDS_SET_OPACITY},
        {IDC_LBL_POSITION, IDS_SET_POSITION},     {IDC_LBL_MARGIN, IDS_SET_MARGIN},
        {IDC_LBL_THEME, IDS_SET_THEME},           {IDC_GRP_BEHAVIOR, IDS_SET_GRP_BEHAVIOR},
        {IDC_CHK_SUPPRESSFS, IDS_SET_SUPPRESSFS}, {IDC_CHK_PRIMARYONLY, IDS_SET_PRIMARYONLY},
        {IDC_BTN_DEFAULTS, IDS_SET_DEFAULTS},     {IDC_BTN_CLOSE, IDS_SET_CLOSE},
        {IDC_LBL_HINT, IDS_SET_HINT},
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
    SetCheck(m_hwnd, IDC_CHK_SUPPRESSFS, m_settings.suppressFullscreen);
    SetCheck(m_hwnd, IDC_CHK_PRIMARYONLY, m_settings.primaryMonitorOnly);
    // IDC_CHK_AUTOSTART bilinçli olarak atlanır (bkz. WM_COMMAND açıklaması).
    SetTrackPos(m_hwnd, IDC_SLD_DURATION, m_settings.osdDurationMs);
    SetTrackPos(m_hwnd, IDC_SLD_OPACITY, m_settings.osdOpacity);
    // UDM_SETPOS32 buddy edit metnini de günceller.
    SendDlgItemMessageW(m_hwnd, IDC_SPN_MARGIN, UDM_SETPOS32, 0,
                        static_cast<LPARAM>(m_settings.osdTopMarginDip));
    SetComboSel(m_hwnd, IDC_CMB_POSITION, static_cast<int>(m_settings.osdPosition));
    SetComboSel(m_hwnd, IDC_CMB_THEME, static_cast<int>(m_settings.themeMode));
    SetComboSel(m_hwnd, IDC_CMB_TRAYKEY, static_cast<int>(m_settings.trayIconKey));
    SetComboSel(m_hwnd, IDC_CMB_LANGUAGE,
                m_settings.language == L"tr" ? 1 : (m_settings.language == L"en" ? 2 : 0));
    m_loading = false;
}

void SettingsDialog::PushFromControls() {
    if (m_loading || !m_hwnd) { return; }
    m_settings.showOsd = IsChecked(m_hwnd, IDC_CHK_SHOWOSD);
    m_settings.watchCaps = IsChecked(m_hwnd, IDC_CHK_WATCHCAPS);
    m_settings.watchNum = IsChecked(m_hwnd, IDC_CHK_WATCHNUM);
    m_settings.watchScroll = IsChecked(m_hwnd, IDC_CHK_WATCHSCROLL);
    m_settings.suppressFullscreen = IsChecked(m_hwnd, IDC_CHK_SUPPRESSFS);
    m_settings.primaryMonitorOnly = IsChecked(m_hwnd, IDC_CHK_PRIMARYONLY);
    m_settings.osdDurationMs = static_cast<unsigned>(TrackPos(m_hwnd, IDC_SLD_DURATION));
    m_settings.osdOpacity = static_cast<unsigned>(TrackPos(m_hwnd, IDC_SLD_OPACITY));
    // Kullanıcı alanı boşaltmışsa updown 0 döner; aralığı Clamp zorlar.
    const int margin = static_cast<int>(
        SendDlgItemMessageW(m_hwnd, IDC_SPN_MARGIN, UDM_GETPOS32, 0, 0));
    m_settings.osdTopMarginDip = static_cast<unsigned>(margin < 0 ? 0 : margin);
    m_settings.osdPosition = static_cast<OsdPositionMode>(
        ComboSelOr(m_hwnd, IDC_CMB_POSITION, static_cast<int>(m_settings.osdPosition)));
    m_settings.themeMode = static_cast<ThemeMode>(
        ComboSelOr(m_hwnd, IDC_CMB_THEME, static_cast<int>(m_settings.themeMode)));
    m_settings.trayIconKey = static_cast<LockKey>(
        ComboSelOr(m_hwnd, IDC_CMB_TRAYKEY, static_cast<int>(m_settings.trayIconKey)));
    switch (ComboSel(m_hwnd, IDC_CMB_LANGUAGE)) {
        case 0:  m_settings.language = L"auto"; break;
        case 1:  m_settings.language = L"tr"; break;
        case 2:  m_settings.language = L"en"; break;
        default: break;  // CB_ERR — mevcut dil korunur
    }
    m_settings.Clamp();
}

void SettingsDialog::UpdateValueLabels() {
    if (!m_hwnd) { return; }
    wchar_t buf[64];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d %s", TrackPos(m_hwnd, IDC_SLD_DURATION),
                 Loc::Str(IDS_UNIT_MS).c_str());
    SetDlgItemTextW(m_hwnd, IDC_VAL_DURATION, buf);
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%d %s", TrackPos(m_hwnd, IDC_SLD_OPACITY),
                 Loc::Str(IDS_UNIT_PERCENT).c_str());
    SetDlgItemTextW(m_hwnd, IDC_VAL_OPACITY, buf);
}

void SettingsDialog::Emit() {
    if (m_loading) { return; }
    if (m_onApply) { m_onApply(m_settings); }  // App kaydeder, modülleri günceller
}

void SettingsDialog::SyncFrom(const Settings& s) {
    m_settings = s;
    if (!m_hwnd) { return; }
    LoadControls();
    UpdateValueLabels();
}

void SettingsDialog::OnThemeChanged(AppTheme theme) {
    m_theme = theme;
    if (!m_hwnd) { return; }
    // SetAppMode App'in sorumluluğunda (tepsi menüsü de ondan besleniyor).
    const bool dark = (theme == AppTheme::Dark);
    WinDark::ApplyToWindow(m_hwnd, dark);
    WinDark::ApplyToChildren(m_hwnd, dark);
    WinDark::RefreshWindow(m_hwnd);  // RDW_ERASE → ayırıcılar yeni renkle çizilir
}

}  // namespace kli
