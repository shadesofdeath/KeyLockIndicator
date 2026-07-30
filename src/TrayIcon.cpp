// TrayIcon.cpp — Shell_NotifyIcon + bağlam menüsü (spec §4.5).
//
// İkon kimliği sabit kIconId'dir; NIF_GUID bilinçli olarak kullanılmaz: GUID
// kimliği exe yolunu da kapsar, dolayısıyla taşınabilir dağıtımda klasör
// değişince kabuk ikonu "yeni" sayar ve kullanıcının gizle/göster tercihi sıfırlanır.
#include "TrayIcon.h"

#include "Localization.h"
#include "Messages.h"
#include "resource.h"

#include <commctrl.h>  // LoadIconWithScaleDown (comctl32)

#include <cstdlib>     // _countof
#include <cwchar>      // wcsncpy_s
#include <string>
#include <utility>

namespace kli {
namespace {

// Tuş + durum + tema → kaynak kimliği. "DARK" varyantı KOYU temada kullanılır
// (glif açık renkli), "LIGHT" varyantı açık temada (glif koyu renkli).
[[nodiscard]] int TrayIconResourceId(LockKey key, bool on, AppTheme theme) noexcept {
    const bool dark = (theme == AppTheme::Dark);
    switch (key) {
        case LockKey::Caps:
            return dark ? (on ? IDI_TRAY_CAPS_ON_DARK : IDI_TRAY_CAPS_OFF_DARK)
                        : (on ? IDI_TRAY_CAPS_ON_LIGHT : IDI_TRAY_CAPS_OFF_LIGHT);
        case LockKey::Num:
            return dark ? (on ? IDI_TRAY_NUM_ON_DARK : IDI_TRAY_NUM_OFF_DARK)
                        : (on ? IDI_TRAY_NUM_ON_LIGHT : IDI_TRAY_NUM_OFF_LIGHT);
        case LockKey::Scroll:
            return dark ? (on ? IDI_TRAY_SCROLL_ON_DARK : IDI_TRAY_SCROLL_OFF_DARK)
                        : (on ? IDI_TRAY_SCROLL_ON_LIGHT : IDI_TRAY_SCROLL_OFF_LIGHT);
    }
    // Ulaşılmaz; yalnızca "tüm yollar değer döndürmüyor" durumunu kapatır.
    return dark ? IDI_TRAY_CAPS_OFF_DARK : IDI_TRAY_CAPS_OFF_LIGHT;
}

// Ortak NOTIFYICONDATAW iskeleti. cbSize sürüm belirleyicidir; Vista+ tam yapı
// gönderilir (NIM_SETVERSION bunu gerektirir).
[[nodiscard]] NOTIFYICONDATAW MakeIconData(HWND host) noexcept {
    NOTIFYICONDATAW nid{static_cast<DWORD>(sizeof(NOTIFYICONDATAW))};
    nid.hWnd = host;
    nid.uID = TrayIcon::kIconId;
    return nid;
}

// Menü öğesi bayrakları — MF_STRING + koşullu onay/gri.
[[nodiscard]] UINT ItemFlags(bool checked, bool grayed) noexcept {
    UINT flags = MF_STRING;
    if (checked) {
        flags |= MF_CHECKED;
    }
    if (grayed) {
        flags |= MF_GRAYED;
    }
    return flags;
}

}  // namespace

TrayIcon::~TrayIcon() {
    Remove();
}

bool TrayIcon::Add(HINSTANCE hInstance, HWND hostWnd, CommandCallback onCommand) {
    if (hostWnd == nullptr) {
        LogV(L"TrayIcon::Add — host pencere yok");
        return false;
    }
    m_instance = hInstance;
    m_host = hostWnd;
    m_onCommand = std::move(onCommand);
    return SendAdd();
}

// Add/OnTaskbarCreated ortak yolu: ikonu yükle, NIM_ADD, ardından sürüm 4'e geç.
bool TrayIcon::SendAdd() {
    if (m_host == nullptr) {
        return false;
    }
    if (m_added) {
        return true;
    }

    LoadIconFor(m_displayKey, m_state.Get(m_displayKey), m_theme);

    NOTIFYICONDATAW nid = MakeIconData(m_host);
    // NIF_SHOWTIP şart: sürüm 4'te kabuk standart tooltip'i normalde bastırır ve
    // uygulamanın kendi açılır penceresini beklerdi; bu bayrak onu geri açar.
    nid.uFlags = NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_KLI_TRAYICON;
    if (m_icon) {
        // İkon kaynağı yüklenemediyse NIF_ICON hiç gönderilmez: NULL hIcon ile
        // NIM_ADD başarısız olabilir ve o zaman menü/çıkış yolu da kaybolur.
        nid.uFlags |= NIF_ICON;
        nid.hIcon = m_icon.get();
    }
    BuildTooltip(m_state, nid.szTip, _countof(nid.szTip));

    if (!::Shell_NotifyIconW(NIM_ADD, &nid)) {
        // Oturum açılışında kabuk henüz hazır olmayabilir; TaskbarCreated mesajı
        // geldiğinde yeniden denenir, bu yüzden burada ölümcül hata yok.
        LogV(L"Shell_NotifyIconW(NIM_ADD) başarısız (GetLastError=%lu)", ::GetLastError());
        return false;
    }
    m_added = true;

    // Sürüm 4: geri bildirimde olay kimliği LOWORD(lParam), imleç konumu wParam.
    nid.uVersion = NOTIFYICON_VERSION_4;
    if (!::Shell_NotifyIconW(NIM_SETVERSION, &nid)) {
        // Sürüm ayarlanamazsa kabuk eski (0) davranışta kalır: olay wParam'da
        // gelir. OnTrayMessage her iki biçimi de tanıdığı için akış bozulmaz.
        LogV(L"Shell_NotifyIconW(NIM_SETVERSION) başarısız — eski davranışla devam");
    }
    return true;
}

void TrayIcon::Remove() {
    if (!m_added) {
        return;
    }
    NOTIFYICONDATAW nid = MakeIconData(m_host);
    if (!::Shell_NotifyIconW(NIM_DELETE, &nid)) {
        LogV(L"Shell_NotifyIconW(NIM_DELETE) başarısız (GetLastError=%lu)", ::GetLastError());
    }
    m_added = false;
}

void TrayIcon::Update(LockState state, LockKey displayKey, AppTheme theme) {
    // Durum her koşulda saklanır: ikon henüz eklenmemişse (kabuk hazır değil ya
    // da Explorer çökmüş) SendAdd bu değerlerle doğru ikonu ekler.
    m_state = state;
    m_displayKey = displayKey;
    m_theme = theme;
    if (!m_added) {
        return;
    }

    LoadIconFor(displayKey, state.Get(displayKey), theme);

    NOTIFYICONDATAW nid = MakeIconData(m_host);
    nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    if (m_icon) {
        // İkon yüklenememişse NIF_ICON gönderilmez; boş kare göstermek yerine
        // kabuktaki mevcut görsel korunur.
        nid.uFlags |= NIF_ICON;
        nid.hIcon = m_icon.get();
    }
    BuildTooltip(state, nid.szTip, _countof(nid.szTip));

    if (!::Shell_NotifyIconW(NIM_MODIFY, &nid)) {
        // Modify yalnızca ikon kaybolduysa başarısız olur (ör. Explorer yeniden
        // başladı ama TaskbarCreated henüz işlenmedi). Bir sonraki ekleme düzeltir.
        LogV(L"Shell_NotifyIconW(NIM_MODIFY) başarısız (GetLastError=%lu)", ::GetLastError());
    }
}

void TrayIcon::SetMenuState(bool osdEnabled, bool autostartEnabled, bool autostartLocked) {
    m_osdEnabled = osdEnabled;
    m_autostartEnabled = autostartEnabled;
    m_autostartLocked = autostartLocked;
}

void TrayIcon::LoadIconFor(LockKey key, bool on, AppTheme theme) {
    const int id = TrayIconResourceId(key, on, theme);
    if (m_icon && m_loadedIconIndex == id) {
        return;  // aynı glif zaten yüklü — her tuş olayında yeniden yükleme yok
    }

    const int cx = ::GetSystemMetrics(SM_CXSMICON);
    const int cy = ::GetSystemMetrics(SM_CYSMICON);

    // LoadIconWithScaleDown, .ico içindeki bir üst boyutu (32/48) alıp yüksek
    // kaliteli küçültme yapar; LoadImage yalnızca en yakın boyutu seçip sertçe
    // gerdiği için 125/150% ölçeklemede tırtıklı görünür.
    HICON hIcon = nullptr;
    HR_LOG(::LoadIconWithScaleDown(m_instance, MAKEINTRESOURCEW(id), cx, cy, &hIcon));
    if (hIcon == nullptr) {
        hIcon = static_cast<HICON>(
            ::LoadImageW(m_instance, MAKEINTRESOURCEW(id), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
    }
    if (hIcon == nullptr) {
        LogV(L"Tray ikonu yüklenemedi (kaynak %d, GetLastError=%lu)", id, ::GetLastError());
        return;  // eski ikon (varsa) korunur
    }

    m_icon.reset(hIcon);
    m_loadedIconIndex = id;
}

// Üç tuşun tamamı, satır başına "<Başlık>: <Durum>". szTip 128 karakterle
// sınırlıdır ve kabuk taşan dizeyi kendisi kesmez; kopyalama _TRUNCATE ile yapılır.
void TrayIcon::BuildTooltip(LockState state, wchar_t* out, size_t cch) const {
    if (out == nullptr || cch == 0) {
        return;
    }

    std::wstring text;
    text.reserve(96);
    for (size_t i = 0; i < kLockKeyCount; ++i) {
        const LockKey key = static_cast<LockKey>(i);
        if (i != 0) {
            text.push_back(L'\n');
        }
        text += Loc::KeyTitle(key);
        text += L": ";
        text += Loc::StateText(state.Get(key));
    }

    (void)::wcsncpy_s(out, cch, text.c_str(), _TRUNCATE);
}

void TrayIcon::OnTrayMessage(WPARAM wParam, LPARAM lParam) {
    // Sürüm 4'te olay LOWORD(lParam), ikon kimliği HIWORD(lParam), imleç konumu
    // wParam'dadır. Sürüm 0 davranışında ise wParam ikon kimliğini, lParam yine
    // fare mesajını taşır — bu yüzden LOWORD(lParam) her iki modda da doğrudur
    // ve wParam'a hiç bakmaya gerek yoktur (tek ikon olduğu için kimlik gereksiz).
    (void)wParam;
    const UINT event = LOWORD(lParam);

    switch (event) {
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
            ShowContextMenu();
            break;

        case NIN_SELECT:
        case NIN_KEYSELECT:  // klavyeyle seçim (Space/Enter) — erişilebilirlik
        case WM_LBUTTONUP:
            // Sol tık tuşun durumunu DEĞİŞTİRMEZ (spec §4.5: yanlışlıkla
            // tetiklenme riski), yalnızca anlık OSD gösterir. Bu eylem için
            // Messages.h'de bir TrayCommand kimliği yoktur; sözleşme gereği
            // komut 0 gönderilir ve App bunu "anlık OSD göster" olarak yorumlar.
            if (m_onCommand) {
                m_onCommand(0);
            }
            break;

        default:
            break;
    }
}

void TrayIcon::ShowContextMenu() {
    if (m_host == nullptr) {
        return;
    }

    unique_hmenu menu(::CreatePopupMenu());
    if (!menu) {
        LogV(L"CreatePopupMenu başarısız");
        return;
    }

    // Dizeler yerelde tutulur: c_str() işaretçileri AppendMenu çağrısı boyunca
    // geçerli olmalı (AppendMenu metni kendisi kopyalar, ama sıra önemlidir).
    const std::wstring settings = Loc::Str(IDS_MENU_SETTINGS);
    const std::wstring autostart = Loc::Str(IDS_MENU_AUTOSTART);
    const std::wstring toggleOsd = Loc::Str(IDS_MENU_TOGGLEOSD);
    const std::wstring about = Loc::Str(IDS_MENU_ABOUT);
    const std::wstring exitItem = Loc::Str(IDS_MENU_EXIT);

    // İlk öğe kalın (MF_DEFAULT): çift tık ve Enter bunu seçer.
    ::AppendMenuW(menu.get(), MF_STRING | MF_DEFAULT, static_cast<UINT_PTR>(kCmdSettings),
                  settings.c_str());
    ::AppendMenuW(menu.get(), ItemFlags(m_autostartEnabled, m_autostartLocked),
                  static_cast<UINT_PTR>(kCmdAutostart), autostart.c_str());
    // Öğe metni "OSD'yi geçici kapat" olduğu için onay işareti OSD KAPALI iken konur.
    ::AppendMenuW(menu.get(), ItemFlags(!m_osdEnabled, false),
                  static_cast<UINT_PTR>(kCmdToggleOsd), toggleOsd.c_str());
    ::AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu.get(), MF_STRING, static_cast<UINT_PTR>(kCmdAbout), about.c_str());
    ::AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu.get(), MF_STRING, static_cast<UINT_PTR>(kCmdExit), exitItem.c_str());

    POINT pt{};
    if (!::GetCursorPos(&pt)) {
        LogV(L"GetCursorPos başarısız — menü gösterilmiyor");
        return;
    }

    // Klasik tray menüsü kuralı: önce host öne alınır, aksi hâlde menü ilk
    // tıklamada kapanmaz; kapandıktan sonra WM_NULL ile mesaj kuyruğu tetiklenir.
    ::SetForegroundWindow(m_host);
    const int cmd = ::TrackPopupMenu(menu.get(),
                                     TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                     pt.x, pt.y, 0, m_host, nullptr);
    ::PostMessageW(m_host, WM_NULL, 0, 0);

    // Menü dışına tıklanırsa 0 döner. Komut, menü yıkıldıktan sonra çalıştırılır:
    // Ayarlar/Hakkında modal pencere açabilir, menü hâlâ ekranda olmamalı.
    if (cmd != 0 && m_onCommand) {
        m_onCommand(cmd);
    }
}

UINT TrayIcon::TaskbarCreatedMessage() {
    // Kabuk bu mesajı yayınlar; kimlik proses ömrü boyunca sabittir.
    static const UINT msg = ::RegisterWindowMessageW(L"TaskbarCreated");
    return msg;
}

void TrayIcon::OnTaskbarCreated() {
    // Explorer çöküp yeniden başladığında eski ikon kaydı yok olur; NIM_DELETE
    // denemeden doğrudan yeniden eklenir.
    m_added = false;
    if (m_host == nullptr) {
        return;
    }
    if (!SendAdd()) {
        LogV(L"TaskbarCreated sonrası ikon yeniden eklenemedi");
    }
}

}  // namespace kli
