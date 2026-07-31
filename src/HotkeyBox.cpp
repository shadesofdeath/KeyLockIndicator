// HotkeyBox.cpp — "kısayola bas" kutusu (bkz. HotkeyBox.h başlık notu).
#include "HotkeyBox.h"

#include <commctrl.h>

#include <cstdio>
#include <string>

namespace kli {
namespace HotkeyBox {
namespace {

constexpr UINT_PTR kSubclassId = 1;

// Değer kontrolün kendi GWLP_USERDATA'sında taşınır: standart EDIT sınıfı bu
// yuvayı kullanmaz, uygulamaya aittir. Böylece ne global değişken ne de ayrı
// bir tablo gerekir (spec §9).
constexpr unsigned kModMask = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;

void Store(HWND edit, unsigned mods, unsigned vk) noexcept {
    const LONG_PTR packed = static_cast<LONG_PTR>(((mods & kModMask) << 8) | (vk & 0xFFu));
    ::SetWindowLongPtrW(edit, GWLP_USERDATA, packed);
}

[[nodiscard]] unsigned Packed(HWND edit) noexcept {
    if (edit == nullptr) {
        return 0;
    }
    return static_cast<unsigned>(::GetWindowLongPtrW(edit, GWLP_USERDATA));
}

// Değiştirici tuşların KENDİSİ kısayolun ana tuşu olamaz; kullanıcı Ctrl'ye
// bastığında kutuya "Ctrl" yazılmamalı, ana tuş beklenmeli.
[[nodiscard]] bool IsModifierVk(unsigned vk) noexcept {
    switch (vk) {
        case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU:    case VK_LMENU:    case VK_RMENU:
        case VK_LWIN:    case VK_RWIN:
            return true;
        default:
            return false;
    }
}

// GetKeyNameTextW, genişletilmiş tuşları yalnızca 24. bit kurulduğunda doğru
// adlandırır; kurulmazsa "Insert" yerine numpad "0" yazar.
[[nodiscard]] bool IsExtendedVk(unsigned vk) noexcept {
    switch (vk) {
        case VK_INSERT: case VK_DELETE: case VK_HOME:  case VK_END:
        case VK_PRIOR:  case VK_NEXT:   case VK_LEFT:  case VK_RIGHT:
        case VK_UP:     case VK_DOWN:   case VK_NUMLOCK: case VK_DIVIDE:
            return true;
        default:
            return false;
    }
}

// Ana tuşun adı klavye DÜZENİNE göre alınır: Türkçe Q'da VK_OEM_1 "ş" yazar,
// sabit bir tablo bunu yanlış gösterirdi.
[[nodiscard]] std::wstring KeyName(unsigned vk) {
    const UINT sc = ::MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (sc != 0) {
        LONG lparam = static_cast<LONG>(sc << 16);
        if (IsExtendedVk(vk)) {
            lparam |= (1L << 24);
        }
        wchar_t buf[64]{};
        const int n = ::GetKeyNameTextW(lparam, buf, static_cast<int>(_countof(buf)));
        if (n > 0) {
            return std::wstring(buf, static_cast<size_t>(n));
        }
    }
    // Adı olmayan tuş (bazı medya tuşları): en azından tanınabilir kalsın.
    wchar_t fallback[16]{};
    _snwprintf_s(fallback, _countof(fallback), _TRUNCATE, L"VK %02X", vk);
    return fallback;
}

// Değiştirici adları bilerek ÇEVRİLMEZ: "Ctrl / Alt / Shift / Win" tuş
// kapaklarında da bu şekilde yazar ve her dilde bu şekilde tanınır.
[[nodiscard]] std::wstring Describe(unsigned mods, unsigned vk) {
    if (vk == 0) {
        return std::wstring();
    }
    std::wstring text;
    const auto add = [&text](const wchar_t* part) {
        if (!text.empty()) {
            text += L" + ";
        }
        text += part;
    };
    if ((mods & MOD_CONTROL) != 0) { add(L"Ctrl"); }
    if ((mods & MOD_ALT) != 0)     { add(L"Alt"); }
    if ((mods & MOD_SHIFT) != 0)   { add(L"Shift"); }
    if ((mods & MOD_WIN) != 0)     { add(L"Win"); }
    if (!text.empty()) {
        text += L" + ";
    }
    text += KeyName(vk);
    return text;
}

// Basım ANINDAKİ değiştirici durumu. GetKeyState mesaj kuyruğuyla eşzamanlıdır;
// GetAsyncKeyState kullanılsaydı kullanıcı tuşu bırakmışsa yanlış okurdu.
[[nodiscard]] unsigned CurrentMods() noexcept {
    unsigned mods = 0;
    if (::GetKeyState(VK_CONTROL) < 0) { mods |= MOD_CONTROL; }
    if (::GetKeyState(VK_MENU) < 0)    { mods |= MOD_ALT; }
    if (::GetKeyState(VK_SHIFT) < 0)   { mods |= MOD_SHIFT; }
    if (::GetKeyState(VK_LWIN) < 0 || ::GetKeyState(VK_RWIN) < 0) { mods |= MOD_WIN; }
    return mods;
}

LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                      UINT_PTR id, DWORD_PTR ref) {
    (void)ref;
    switch (msg) {
        case WM_GETDLGCODE:
            // Tüm tuşlar kutuya gelsin (Ctrl+Tab gibi kombinasyonlar da geçerli
            // kısayoldur). Değiştiricisiz Tab/Esc aşağıda diyaloğa iade edilir.
            return DLGC_WANTALLKEYS | DLGC_WANTCHARS | DLGC_WANTARROWS;

        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            // Kutu salt okunur; karakter girilmemeli ve Alt kombinasyonları
            // sistem "bip" sesi çıkarmamalı.
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const unsigned vk = static_cast<unsigned>(wParam);
            const unsigned mods = CurrentMods();

            if (mods == 0 && (vk == VK_TAB || vk == VK_ESCAPE)) {
                // Düz Tab ve Esc kutunun malı değildir: klavyeyle gezinme ve
                // pencereyi kapatma çalışmaya devam etmeli.
                const HWND dlg = ::GetParent(hwnd);
                if (dlg != nullptr && vk == VK_TAB) {
                    const HWND next = ::GetNextDlgTabItem(dlg, hwnd, FALSE);
                    if (next != nullptr) {
                        ::SetFocus(next);
                    }
                } else if (dlg != nullptr) {
                    ::SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
                }
                return 0;
            }
            if (IsModifierVk(vk)) {
                return 0;   // ana tuş henüz gelmedi
            }

            // En az bir "gerçek" değiştirici şart: tek başına (ya da yalnız
            // Shift'li) bir tuşu genel kısayol yapmak o tuşu tüm sistemde
            // uygulamaya çeker. Eksikse comctl32'nin HKM_SETRULES davranışı
            // taklit edilir ve Ctrl+Alt eklenir.
            unsigned finalMods = mods;
            if ((finalMods & (MOD_CONTROL | MOD_ALT | MOD_WIN)) == 0) {
                finalMods |= (MOD_CONTROL | MOD_ALT);
            }
            Set(hwnd, finalMods, vk);
            return 0;
        }

        case WM_NCDESTROY:
            ::RemoveWindowSubclass(hwnd, Proc, id);
            break;

        default:
            break;
    }
    return ::DefSubclassProc(hwnd, msg, wParam, lParam);
}

}  // namespace

void Attach(HWND edit) {
    if (edit == nullptr) {
        return;
    }
    ::SetWindowSubclass(edit, Proc, kSubclassId, 0);
}

void Set(HWND edit, unsigned mods, unsigned vk) {
    if (edit == nullptr) {
        return;
    }
    const unsigned m = mods & kModMask;
    const unsigned v = vk & 0xFFu;
    Store(edit, m, v);
    // SetWindowTextW, EDIT kontrolünün ebeveynine EN_CHANGE gönderir; ayarlar
    // penceresi değeri oradan okur, ayrıca bildirim üretmeye gerek yok.
    ::SetWindowTextW(edit, Describe(m, v).c_str());
}

unsigned Mods(HWND edit) noexcept {
    return (Packed(edit) >> 8) & kModMask;
}

unsigned Vk(HWND edit) noexcept {
    return Packed(edit) & 0xFFu;
}

}  // namespace HotkeyBox
}  // namespace kli
