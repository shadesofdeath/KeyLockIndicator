// Theme.h — Tema kimliği ve OSD renk paleti (spec §3.3). Yaprak modül.
#pragma once

#include <d2d1_1.h>

#include <cstdint>

namespace kli {

enum class AppTheme {
    Light = 0,
    Dark = 1,
};

// Spec §3.3 tablolarının kod karşılığı. Alfa değerleri D2D fırçalarının kendi
// opaklığıdır; pencere geneline uygulanan global alfa DEĞİLDİR.
struct OsdPalette {
    D2D1_COLOR_F cardFill{};    // Kart zemini (alfa ayarlardan gelir)
    D2D1_COLOR_F border{};      // Kenarlık
    D2D1_COLOR_F iconOn{};      // İkon — açık
    D2D1_COLOR_F iconOff{};     // İkon — kapalı
    D2D1_COLOR_F title{};       // Başlık metni
    D2D1_COLOR_F status{};      // Durum metni
    D2D1_COLOR_F shadow{};      // Gölge rengi + alfa
};

// #RRGGBB + ayrı alfa → D2D1_COLOR_F (düz, premultiply edilmemiş)
[[nodiscard]] constexpr D2D1_COLOR_F Rgba(uint32_t rgb, float alpha) noexcept {
    return D2D1_COLOR_F{
        static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
        static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
        static_cast<float>(rgb & 0xFF) / 255.0f,
        alpha,
    };
}

// cardAlpha: 0.60 – 1.00 aralığında, ayarlardan gelir. Varsayılanlar tema
// başına farklıdır (koyu 0.82 / açık 0.85) ve Settings tarafında tutulur.
//
// highContrast=true (madde 29): tablolar TAMAMEN devre dışı kalır ve renkler
// sistemin yüksek kontrast paletinden (GetSysColor) türetilir; kart tam opak
// olur, cardAlpha yok sayılır ve gölge alfası sıfırlanır. Gerekçe: yüksek
// kontrast kipinin tek amacı okunabilirliktir, saydam bir kart ve yumuşak bir
// gölge o amacı doğrudan bozar.
[[nodiscard]] OsdPalette MakePalette(AppTheme theme, float cardAlpha,
                                     bool highContrast) noexcept;

// Sistemin yüksek kontrast kipi açık mı (SPI_GETHIGHCONTRAST → HCF_HIGHCONTRASTON).
// Sorgu başarısızsa false döner: yanlış pozitif, kullanıcının seçtiği temayı
// sebepsiz ezmek demek olurdu.
[[nodiscard]] bool HighContrastActive() noexcept;

// Ayarlarda saklanan yüzde değeri (60–100) → kart alfası.
[[nodiscard]] inline float OpacityPercentToAlpha(unsigned percent) noexcept {
    const float p = static_cast<float>(percent < 60 ? 60u : (percent > 100 ? 100u : percent));
    return p / 100.0f;
}

}  // namespace kli
