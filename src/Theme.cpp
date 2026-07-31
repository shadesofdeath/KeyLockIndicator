// Theme.cpp — spec §3.3 renk tablolarının tek doğruluk kaynağı.
//
// Renkler hiçbir yerde tekrar edilmez: OsdRenderer yalnızca buradan gelen
// paleti kullanır. Böylece tema geçişinde "bir fırça güncellenmeyi unutuldu"
// hatası yapısal olarak imkânsız hâle gelir.
#include "Theme.h"

#include <windows.h>

namespace kli {
namespace {

// COLORREF (0x00BBGGRR) → D2D rengi. Sistem renkleri her zaman tam opaktır;
// alfa çağırandan gelir.
[[nodiscard]] D2D1_COLOR_F FromSysColor(int index, float alpha) noexcept {
    const COLORREF c = ::GetSysColor(index);
    return D2D1_COLOR_F{
        static_cast<float>(GetRValue(c)) / 255.0f,
        static_cast<float>(GetGValue(c)) / 255.0f,
        static_cast<float>(GetBValue(c)) / 255.0f,
        alpha,
    };
}

// Yüksek kontrast paleti. Kullanıcının seçtiği yüksek kontrast şeması hangi
// renkleri kullanıyorsa OSD de onları kullanır; sabit renk YAZILMAZ, çünkü
// şemalar (siyah/beyaz/#1/#2) birbirinin tam tersi olabilir.
[[nodiscard]] OsdPalette MakeHighContrastPalette() noexcept {
    OsdPalette p{};
    p.cardFill = FromSysColor(COLOR_WINDOW, 1.00f);       // TAM OPAK
    p.border   = FromSysColor(COLOR_WINDOWTEXT, 1.00f);   // kartın sınırını bu taşır
    // "Açık" hâl sistem vurgu rengiyle, "kapalı" hâl gövde metni rengiyle çizilir.
    // Yüksek kontrastta soluklaştırma (alfa düşürme) YASAKTIR: şemanın garanti
    // ettiği kontrast oranını bozar. Ayrım zaten geometriden de geliyor (§3.5).
    p.iconOn   = FromSysColor(COLOR_HIGHLIGHT, 1.00f);
    p.iconOff  = FromSysColor(COLOR_WINDOWTEXT, 1.00f);
    p.title    = FromSysColor(COLOR_WINDOWTEXT, 1.00f);
    p.status   = FromSysColor(COLOR_WINDOWTEXT, 1.00f);
    // Gölge alfası 0: OsdRenderer bunu görüp tüm gölge yolunu atlar.
    p.shadow   = Rgba(0x000000, 0.00f);
    return p;
}

}  // namespace

bool HighContrastActive() noexcept {
    HIGHCONTRASTW hc{};
    hc.cbSize = static_cast<UINT>(sizeof(hc));
    if (!::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0)) {
        return false;
    }
    return (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

OsdPalette MakePalette(AppTheme theme, float cardAlpha, bool highContrast) noexcept {
    if (highContrast) {
        return MakeHighContrastPalette();
    }

    // Ayarlardan gelen alfa normalde 0.60–1.00 aralığındadır (Settings::Clamp).
    // Yine de savunma amaçlı sınırlanır: bozuk registry değeri D2D'ye geçersiz
    // fırça opaklığı olarak sızmasın. Util.h'deki Clamp yerine yerel ifade
    // kullanılıyor; Theme yaprak modülünün windows.h dışında bağımlılığı olmasın.
    const float alpha = cardAlpha < 0.0f ? 0.0f : (cardAlpha > 1.0f ? 1.0f : cardAlpha);

    OsdPalette p{};

    if (theme == AppTheme::Dark) {
        p.cardFill = Rgba(0x1F1F1F, alpha);   // varsayılan alfa 0.82
        p.border   = Rgba(0xFFFFFF, 0.09f);
        p.iconOn   = Rgba(0xFFFFFF, 1.00f);
        p.iconOff  = Rgba(0xFFFFFF, 0.38f);
        p.title    = Rgba(0xFFFFFF, 0.95f);
        p.status   = Rgba(0xFFFFFF, 0.58f);
        p.shadow   = Rgba(0x000000, 0.28f);   // spec §3.4: koyu temada daha güçlü
    } else {
        p.cardFill = Rgba(0xF7F7F7, alpha);   // varsayılan alfa 0.85
        p.border   = Rgba(0x000000, 0.08f);
        p.iconOn   = Rgba(0x1A1A1A, 1.00f);
        p.iconOff  = Rgba(0x1A1A1A, 0.32f);
        p.title    = Rgba(0x000000, 0.90f);
        p.status   = Rgba(0x000000, 0.55f);
        p.shadow   = Rgba(0x000000, 0.18f);
    }

    return p;
}

}  // namespace kli
