// Theme.cpp — spec §3.3 renk tablolarının tek doğruluk kaynağı.
//
// Renkler hiçbir yerde tekrar edilmez: OsdRenderer yalnızca buradan gelen
// paleti kullanır. Böylece tema geçişinde "bir fırça güncellenmeyi unutuldu"
// hatası yapısal olarak imkânsız hâle gelir.
#include "Theme.h"

namespace kli {

OsdPalette MakePalette(AppTheme theme, float cardAlpha) noexcept {
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
