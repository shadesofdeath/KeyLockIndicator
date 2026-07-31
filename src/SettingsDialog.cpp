// SettingsDialog.cpp — WinDark ile temalandırılmış modeless ayar penceresi (spec §4.6).
//
// "Uygula" butonu yoktur: her kontrol değişimi PushFromControls → Emit zinciriyle
// anında App'e iletilir. m_loading bayrağı, LoadControls'un kontrollere yazması
// sırasında doğan bildirimlerin geri besleme döngüsü kurmasını engeller.
// Şablon (IDD_SETTINGS) yalnızca yerleşimi taşır; etiket metinleri çalışma
// zamanında Loc::Str ile konur, böylece dil ayarı anında etki eder.
#include "SettingsDialog.h"

#include "HotkeyBox.h"
#include "Localization.h"
#include "Messages.h"
#include "MonitorUtil.h"
#include "SettingsFields.h"
#include "SettingsPaint.h"
#include "Util.h"
#include "resource.h"

#include <WinDark/WinDark.h>

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <utility>

namespace kli {
namespace {

// Özel davranışı olmayan kontroller: bildirimleri doğrudan PushFromControls +
// Emit tetikler (IDC_CHK_AUTOSTART burada yoktur — bkz. WM_COMMAND).
bool IsPlainCheckbox(int id) noexcept {
    return id == IDC_CHK_SHOWOSD || id == IDC_CHK_WATCHCAPS || id == IDC_CHK_WATCHNUM ||
           id == IDC_CHK_WATCHSCROLL || id == IDC_CHK_SUPPRESSFS || id == IDC_CHK_ONLYON ||
           id == IDC_CHK_HOTKEY || id == IDC_CHK_BADGE || id == IDC_CHK_ANNOUNCE ||
           id == IDC_CHK_WATCHLAYOUT;
}

bool IsComboBox(int id) noexcept {
    return id == IDC_CMB_LANGUAGE || id == IDC_CMB_TRAYKEY || id == IDC_CMB_TRAYCLICK ||
           id == IDC_CMB_POSITION || id == IDC_CMB_THEME || id == IDC_CMB_MONITOR ||
           id == IDC_CMB_VIEWMODE;
}

bool IsSlider(int id) noexcept {
    return id == IDC_SLD_DURATION || id == IDC_SLD_OPACITY || id == IDC_SLD_SCALE;
}

// InitControlRanges / FillComboBoxes SettingsFields.cpp içindedir
// (400 satır sınırı, spec §9).

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
            SettingsFields::InitControlRanges(m_hwnd);
            SettingsFields::FillComboBoxes(m_hwnd);
            // Proses genelindeki app modu App::Initialize'da kuruldu (tepsi menüsü
            // de ona bağlı); burada yalnızca pencere ve kontroller temalandırılır.
            const bool dark = (m_theme == AppTheme::Dark);
            WinDark::ApplyToWindow(m_hwnd, dark);
            WinDark::ApplyToChildren(m_hwnd, dark);
            LoadControls();
            UpdateValueLabels();
            RefreshExcludeList();
            UpdateStorageLabel();
            CenterOnActiveMonitor(m_hwnd);
            // Konumlandırmadan SONRA: başlık genişlikleri pencerenin son DPI'ında
            // ölçülsün. Bölüm çizgileri de bu daraltılmış kenardan başlar.
            SettingsPaint::LayoutSectionHeaders(m_hwnd);
            return TRUE;  // odak varsayılan kontrolde kalır (SetFocus yapılmadı)
        }
        case WM_HSCROLL: {
            // Yalnızca üç trackbar'dan gelir; lParam kontrolün kolu.
            const HWND ctrl = reinterpret_cast<HWND>(lParam);
            const int id = ctrl ? GetDlgCtrlID(ctrl) : 0;
            if (!IsSlider(id)) { break; }
            // Değeri okunabilir bir adıma oturt: süre 50 ms, kart boyutu %5.
            // Opaklık yüzdesi zaten tek tek anlamlıdır, oturtulmaz.
            const int step = (id == IDC_SLD_DURATION) ? 50 : (id == IDC_SLD_SCALE ? 5 : 0);
            if (step > 0) {
                const int pos = static_cast<int>(SendMessageW(ctrl, TBM_GETPOS, 0, 0));
                const int snapped = ((pos + step / 2) / step) * step;
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
                RefreshExcludeList();   // istisna listesi de varsayılana (boş) döner
                Emit();
                return TRUE;
            }
            if (code == BN_CLICKED &&
                (id == IDC_BTN_EXCL_ADD || id == IDC_BTN_EXCL_DEL ||
                 id == IDC_BTN_EXPORT || id == IDC_BTN_IMPORT)) {
                // Dosya seçici açan komutlar; hepsi SettingsExclude.cpp içinde.
                switch (id) {
                    case IDC_BTN_EXCL_ADD: OnAddExcluded(); break;
                    case IDC_BTN_EXCL_DEL: OnRemoveExcluded(); break;
                    case IDC_BTN_EXPORT:   OnExportSettings(); break;
                    default:               OnImportSettings(); break;
                }
                return TRUE;
            }
            if (id == IDC_LST_EXCLUDE && code == LBN_SELCHANGE) {
                // "Kaldır" butonunun etkinliği seçime bağlı.
                if (const HWND del = GetDlgItem(m_hwnd, IDC_BTN_EXCL_DEL)) {
                    EnableWindow(del, TRUE);
                }
                return TRUE;
            }
            if (id == IDC_BTN_PICKPOS && code == BN_CLICKED) {
                // OSD penceresi App'e aittir; SettingsDialog onu tanımaz (spec §2).
                // Autostart ile aynı desen: komut sahibe iletilir, App sürükleme
                // kipini başlatır ve sonucu SyncFrom ile geri yansıtır.
                if (const HWND owner = GetWindow(m_hwnd, GW_OWNER)) {
                    SendMessageW(owner, WM_COMMAND, MAKEWPARAM(kCmdPickPosition, 0), 0);
                }
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
            // msctls_hotkey32 de değişimini EN_CHANGE ile bildirir.
            if ((code == BN_CLICKED && IsPlainCheckbox(id)) ||
                (code == CBN_SELCHANGE && IsComboBox(id)) ||
                (code == EN_CHANGE && (id == IDC_EDT_MARGIN || id == IDC_HOT_SHORTCUT))) {
                PushFromControls();
                UpdateValueLabels();   // kısayol kutusunun etkinliği de buradan
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

// ApplyLocalizedText / LoadControls / PushFromControls / UpdateValueLabels
// SettingsFields.cpp içindedir (400 satır sınırı, spec §9).

void SettingsDialog::Emit() {
    if (m_loading) { return; }
    if (m_onApply) { m_onApply(m_settings); }  // App kaydeder, modülleri günceller
}

void SettingsDialog::SetHotkeyWarning(bool conflict) {
    if (!m_hwnd) { return; }
    // Çakışma yokken metin BOŞTUR: kalıcı bir uyarı satırı, sorun olmadığında da
    // kullanıcının gözünü rahatsız ederdi.
    SetDlgItemTextW(m_hwnd, IDC_LBL_HOTKEYWARN,
                    conflict ? Loc::Str(IDS_HOTKEY_FAILED).c_str() : L"");
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void SettingsDialog::SetPickingHint(bool picking) {
    if (!m_hwnd) { return; }
    SetDlgItemTextW(m_hwnd, IDC_LBL_HINT,
                    Loc::Str(picking ? IDS_SET_PICKHINT : IDS_SET_HINT).c_str());
    // Buton kip boyunca pasif: ikinci basış kipi baştan başlatır ve kullanıcı
    // hangi kartı sürüklediğini kaybederdi.
    if (const HWND btn = GetDlgItem(m_hwnd, IDC_BTN_PICKPOS)) {
        EnableWindow(btn, picking ? FALSE : TRUE);
    }
}

void SettingsDialog::SyncFrom(const Settings& s) {
    m_settings = s;
    if (!m_hwnd) { return; }
    LoadControls();
    UpdateValueLabels();
    RefreshExcludeList();
}

void SettingsDialog::OnLanguageChanged() {
    if (!m_hwnd) { return; }
    ApplyLocalizedText();
    // CB_RESETCONTENT seçimi de siler; hemen ardından gelen LoadControls geri koyar
    // (m_loading bayrağı bu yazmaların Emit etmesini keser, döngü oluşmaz).
    SettingsFields::FillComboBoxes(m_hwnd);
    LoadControls();
    UpdateValueLabels();
    UpdateStorageLabel();   // depo satırı da yeni dile döner
    // Başlık genişlikleri çeviriyle değişir: ayırıcı çizgiler yeni metnin sağ
    // kenarından başlamak zorunda, yoksa çizgi başlığın üstünden geçer.
    SettingsPaint::LayoutSectionHeaders(m_hwnd);
    InvalidateRect(m_hwnd, nullptr, TRUE);   // RDW_ERASE → ayırıcılar yeniden çizilsin
}

void SettingsDialog::OnThemeChanged(AppTheme theme) {
    m_theme = theme;
    if (!m_hwnd) { return; }
    // SetAppMode App'in sorumluluğunda (tepsi menüsü de ondan besleniyor).
    const bool dark = (theme == AppTheme::Dark);
    WinDark::ApplyToWindow(m_hwnd, dark);
    WinDark::ApplyToChildren(m_hwnd, dark);
    WinDark::RefreshWindow(m_hwnd);
    // RefreshWindow'un RDW_ERASE'i TEK BAŞINA YETMİYOR (ölçüldü): kontroller yeni
    // renkleriyle çizilirken diyaloğun ZEMİNİ eski temada kalıyor — açık temaya
    // geçildiğinde beyaz kontroller koyu bir zeminin üstünde duruyordu. Sebep
    // sıradan: RefreshWindow'un ilk işi SWP_FRAMECHANGED'dir ve o çağrının
    // tetiklediği çizim, RedrawWindow işaretlemesinden ÖNCE istemci alanını
    // geçerli kılıyor. Bu yüzden zemin ayrıca ve açıkça geçersizleştirilir;
    // TRUE parametresi WM_ERASEBKGND'yi garanti eder.
    InvalidateRect(m_hwnd, nullptr, TRUE);
    UpdateWindow(m_hwnd);
}

}  // namespace kli
