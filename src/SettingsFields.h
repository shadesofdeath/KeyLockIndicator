// SettingsFields.h — Ayarlar penceresinin kontrol erişim kısaltmaları.
//
// Cast'ler burada toplanır ki çağrı yerleri /W4 daraltma uyarısı üretmesin.
// SettingsDialog.cpp ile SettingsFields.cpp bu kısaltmaları paylaşır: sınıfın
// alan okuma/yazma bölümü (ApplyLocalizedText, LoadControls, PushFromControls,
// UpdateValueLabels) ayrı dosyada yaşar çünkü SettingsDialog.cpp 400 satır
// sınırına dayandı (spec §9 → böl). Bölünme yeni bir katman DEĞİLDİR; iki dosya
// aynı sınıfın parçalarıdır, OsdWindow/OsdDevice bölünmesiyle aynı desen.
#pragma once

#include "Settings.h"
#include "resource.h"

#include <windows.h>
#include <commctrl.h>

namespace kli {
namespace SettingsFields {

// --- Tek seferlik kontrol kurulumu -----------------------------------------
// İkisi de SettingsFields.cpp içindedir. SettingsDialog.cpp 400 satır sınırına
// dayandığı için (spec §9) buraya alındılar; sınıfın "kontrolle konuşan"
// bölümüyle zaten aynı yerdeler.

// Kaydırıcı aralıkları, yukarı/aşağı düğmesinin buddy'si, kısayol kutusunun
// alt sınıflanması. WM_INITDIALOG'da bir kez çağrılır.
void InitControlRanges(HWND dlg);

// Tüm açılır listelerin içeriği. Dil değişiminde YENİDEN çağrılır; seçimleri
// silen CB_RESETCONTENT'in ardından LoadControls seçimleri geri koyar.
void FillComboBoxes(HWND dlg);

// --- Konum açılır listesi --------------------------------------------------
// Listedeki OKUMA SIRASI ile enum'un SAYISAL sırası bilerek farklıdır: enum
// değerleri geriye uyum için 1.0'daki 0=Üst / 1=Orta / 2=Alt'a çakılı kaldı
// (bkz. Settings.h), kullanıcı ise ızgarayı sol üstten sağ alta okur. Sıra ve
// dize kimliği TEK tabloda tutulur ki ikisi birbirinden ayrı düşemesin.
struct PositionItem {
    OsdPositionMode mode;
    UINT stringId;
};

inline constexpr PositionItem kPositionItems[] = {
    {OsdPositionMode::TopLeft,      IDS_POS_TOPLEFT},
    {OsdPositionMode::TopCenter,    IDS_POS_TOP},
    {OsdPositionMode::TopRight,     IDS_POS_TOPRIGHT},
    {OsdPositionMode::MiddleLeft,   IDS_POS_MIDLEFT},
    {OsdPositionMode::Center,       IDS_POS_CENTER},
    {OsdPositionMode::MiddleRight,  IDS_POS_MIDRIGHT},
    {OsdPositionMode::BottomLeft,   IDS_POS_BOTLEFT},
    {OsdPositionMode::BottomCenter, IDS_POS_BOTTOM},
    {OsdPositionMode::BottomRight,  IDS_POS_BOTRIGHT},
    {OsdPositionMode::Custom,       IDS_POS_CUSTOM},
};

inline constexpr int kPositionItemCount =
    static_cast<int>(sizeof(kPositionItems) / sizeof(kPositionItems[0]));

[[nodiscard]] inline int PositionToIndex(OsdPositionMode mode) noexcept {
    for (int i = 0; i < kPositionItemCount; ++i) {
        if (kPositionItems[i].mode == mode) {
            return i;
        }
    }
    return 0;
}

[[nodiscard]] inline OsdPositionMode IndexToPosition(int index,
                                                     OsdPositionMode fallback) noexcept {
    if (index < 0 || index >= kPositionItemCount) {
        return fallback;   // CB_ERR: mevcut ayar korunur
    }
    return kPositionItems[index].mode;
}

[[nodiscard]] inline int TrackPos(HWND dlg, int id) noexcept {
    return static_cast<int>(::SendDlgItemMessageW(dlg, id, TBM_GETPOS, 0, 0));
}

inline void SetTrackPos(HWND dlg, int id, unsigned value) noexcept {
    ::SendDlgItemMessageW(dlg, id, TBM_SETPOS, TRUE, static_cast<LPARAM>(value));
}

[[nodiscard]] inline bool IsChecked(HWND dlg, int id) noexcept {
    return ::IsDlgButtonChecked(dlg, id) == BST_CHECKED;
}

inline void SetCheck(HWND dlg, int id, bool on) noexcept {
    ::CheckDlgButton(dlg, id, on ? BST_CHECKED : BST_UNCHECKED);
}

[[nodiscard]] inline int ComboSel(HWND dlg, int id) noexcept {
    return static_cast<int>(::SendDlgItemMessageW(dlg, id, CB_GETCURSEL, 0, 0));
}

inline void SetComboSel(HWND dlg, int id, int index) noexcept {
    ::SendDlgItemMessageW(dlg, id, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

// CB_ERR (-1) ya da beklenmeyen indeks gelirse mevcut ayar korunur. maxIndex
// listenin son öğesidir; enum'a doğrudan dönüştürülen listelerde aralık dışı
// bir indeksin geçmesi tanımsız davranış olurdu.
[[nodiscard]] inline int ComboSelOr(HWND dlg, int id, int fallback, int maxIndex = 2) noexcept {
    const int sel = ComboSel(dlg, id);
    return (sel >= 0 && sel <= maxIndex) ? sel : fallback;
}

}  // namespace SettingsFields
}  // namespace kli
