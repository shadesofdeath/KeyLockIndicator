// OsdRenderer.h — Direct2D çizim: kare kart + gölge + ikon + metin (spec §3).
#pragma once

#include "IconGeometry.h"
#include "LockTypes.h"
#include "Theme.h"
#include "Util.h"

#include <d2d1_1.h>
#include <dwrite.h>

#include <string>

namespace kli {

// Spec §3.1 / §3.2 ölçüleri. Tümü DIP; render DPI'sı hedef bitmap üzerinden
// uygulanır, çizim kodu her zaman DIP konuşur.
struct OsdLayout {
    static constexpr float kCardSize        = 180.0f;  // KARE
    static constexpr float kCornerRadius    = 12.0f;
    static constexpr float kBorderThickness = 1.0f;
    static constexpr float kBorderInset     = 0.5f;

    // Gölge için her yönde 32 DIP pay → yüzey 244 x 244 DIP
    static constexpr float kSurfacePadding  = 32.0f;
    static constexpr float kSurfaceSize     = kCardSize + 2.0f * kSurfacePadding;

    // Kart içi yerleşim (180x180 kutu içinde, kartın sol üstüne göre)
    static constexpr float kIconTop         = 34.0f;
    static constexpr float kIconBox         = 72.0f;
    static constexpr float kTitleTop        = 120.0f;
    static constexpr float kStatusTop       = 144.0f;
    static constexpr float kTitleFontSize   = 15.0f;
    static constexpr float kStatusFontSize  = 13.0f;
    static constexpr float kTitleTracking   = 0.5f;   // harf aralığı +0.5 DIP

    // Gölge
    static constexpr float kShadowBlurStdDev = 9.0f;
    static constexpr float kShadowOffsetY    = 6.0f;

    // Kartın yüzey içindeki sol üst köşesi
    static constexpr float kCardLeft = kSurfacePadding;
    static constexpr float kCardTop  = kSurfacePadding;
};

struct OsdContent {
    LockKey key = LockKey::Caps;
    bool on = false;
    std::wstring title;   // "CAPS LOCK"
    std::wstring status;  // "Açık" / "Kapalı"
};

class OsdRenderer {
public:
    OsdRenderer() = default;
    OsdRenderer(const OsdRenderer&) = delete;
    OsdRenderer& operator=(const OsdRenderer&) = delete;

    // Cihazdan bağımsız: DWrite metin biçimleri + ikon geometrileri.
    HRESULT CreateDeviceIndependentResources(ID2D1Factory1* d2dFactory,
                                             IDWriteFactory* dwriteFactory);

    // Cihaza bağlı: fırçalar, gölge efekti. Cihaz kaybında Release + yeniden.
    HRESULT CreateDeviceResources(ID2D1DeviceContext* ctx);
    void ReleaseDeviceResources();

    void SetTheme(AppTheme theme, float cardAlpha);
    [[nodiscard]] AppTheme Theme() const noexcept { return m_theme; }

    // Çağıran hedef bitmap'i ve DPI'yı ayarlamış, Clear'ı yapmıştır.
    // originDip: yüzeyin çizim orijini (DComp BeginDraw offset'i DIP'e çevrilmiş).
    HRESULT Render(ID2D1DeviceContext* ctx, const OsdContent& content,
                   D2D1_POINT_2F originDip);

private:
    HRESULT BuildShadowCommandList(ID2D1DeviceContext* ctx, D2D1_POINT_2F originDip);
    HRESULT DrawIcon(ID2D1DeviceContext* ctx, const OsdContent& content,
                     D2D1_POINT_2F cardOrigin);
    HRESULT DrawText(ID2D1DeviceContext* ctx, const OsdContent& content,
                     D2D1_POINT_2F cardOrigin);

    ComPtr<ID2D1Factory1> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<IDWriteTextFormat> m_titleFormat;
    ComPtr<IDWriteTextFormat> m_statusFormat;
    IconGeometry m_icons;

    ComPtr<ID2D1SolidColorBrush> m_cardBrush;
    ComPtr<ID2D1SolidColorBrush> m_borderBrush;
    ComPtr<ID2D1SolidColorBrush> m_iconBrush;
    ComPtr<ID2D1SolidColorBrush> m_titleBrush;
    ComPtr<ID2D1SolidColorBrush> m_statusBrush;
    ComPtr<ID2D1Effect> m_shadowEffect;
    ComPtr<ID2D1CommandList> m_shadowSource;

    AppTheme m_theme = AppTheme::Dark;
    float m_cardAlpha = 0.82f;
    OsdPalette m_palette{};
    bool m_deviceResourcesValid = false;
    D2D1_POINT_2F m_shadowOrigin{-1.0f, -1.0f};
};

}  // namespace kli
