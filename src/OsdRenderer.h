// OsdRenderer.h — Direct2D çizim: kart + gölge + ikon + metin (spec §3).
//
// Kart ÖLÇÜLERİ burada değil OsdMetrics.h'dedir: yüzeyi OsdWindow, çizimi bu
// sınıf hesaplar ve ikisinin aynı tabloyu okuması zorunludur (bkz. OsdMetrics.h).
#pragma once

#include "IconGeometry.h"
#include "LockTypes.h"
#include "OsdMetrics.h"
#include "Theme.h"
#include "Util.h"

#include <d2d1_1.h>
#include <dwrite.h>

#include <string>

namespace kli {

struct OsdContent {
    LockKey key = LockKey::Caps;
    bool on = false;
    // true ise kart bir kilit tuşunu değil AKTİF KLAVYE DÜZENİNİ gösterir
    // (madde 28): ikon klavye rozetidir, key/on alanları yok sayılır ve başlık
    // dil kodu ("TR"), durum ise düzen adı ("Türkçe Q") olur.
    bool keyboardLayout = false;
    std::wstring title;   // "CAPS LOCK" / "TR"
    std::wstring status;  // "Açık" / "Kapalı" / "Türkçe Q"
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

    // highContrast: sistem yüksek kontrast kipi açık (madde 29). Açıkken palet
    // sistem renklerinden türer, kart tam opak olur ve kenarlık kalınlaşır;
    // cardAlpha yok sayılır.
    void SetTheme(AppTheme theme, float cardAlpha, bool highContrast);
    [[nodiscard]] AppTheme Theme() const noexcept { return m_theme; }

    // Kartın tamamının boyut çarpanı (1.0 = spec §3.1 ölçüleri). Yüzeyin de aynı
    // çarpanla büyütülmesi çağıranın sorumluluğundadır.
    void SetScale(float scale);
    [[nodiscard]] float Scale() const noexcept { return m_drawScale; }

    // Görünüm kipi (madde 13–15). Kartın geometrisi tamamen değiştiği için gölge
    // komut listesi de geçersizleşir.
    void SetView(OsdView view);
    [[nodiscard]] OsdView View() const noexcept { return m_view; }

    // Çağıran hedef bitmap'i ve DPI'yı ayarlamış, Clear'ı yapmıştır.
    // originDip: yüzeyin çizim orijini (DComp BeginDraw offset'i DIP'e çevrilmiş).
    HRESULT Render(ID2D1DeviceContext* ctx, const OsdContent& content,
                   D2D1_POINT_2F originDip);

private:
    HRESULT BuildShadowCommandList(ID2D1DeviceContext* ctx, D2D1_POINT_2F originDip);
    // Ölçek + yüzey orijini: kart yerleşimi ölçeksiz DIP olarak yazılır, dönüşüm
    // ölçekler ve yüzey atlası kaymasını ekler (öteleme ölçeklenMEZ).
    [[nodiscard]] D2D1::Matrix3x2F WorldTransform(D2D1_POINT_2F originDip) const noexcept;
    HRESULT DrawIcon(ID2D1DeviceContext* ctx, const OsdContent& content,
                     const D2D1::Matrix3x2F& world);

    // Aşağıdaki üçü OsdText.cpp içindedir (400 satır sınırı, spec §9).
    HRESULT DrawText(ID2D1DeviceContext* ctx, const OsdContent& content);
    HRESULT DrawStackedText(ID2D1DeviceContext* ctx, const OsdContent& content,
                            const OsdMetrics& m);
    HRESULT DrawInlineText(ID2D1DeviceContext* ctx, const OsdContent& content,
                           const OsdMetrics& m);

    // Başlık düzeni + harf aralığı; iki metin kipinde de kullanılır.
    HRESULT MakeTitleLayout(const std::wstring& text, IDWriteTextFormat* format,
                            float maxWidth, float maxHeight,
                            ComPtr<IDWriteTextLayout>& out) const;

    ComPtr<ID2D1Factory1> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    // Yığılmış yerleşim: ortalı, başlık 15 / durum 13.
    ComPtr<IDWriteTextFormat> m_titleFormat;
    ComPtr<IDWriteTextFormat> m_statusFormat;
    // Satır içi yerleşim: sola dayalı, dikey ortalı, ikisi de 13 (aynı taban çizgisi).
    ComPtr<IDWriteTextFormat> m_barTitleFormat;
    ComPtr<IDWriteTextFormat> m_barStatusFormat;
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
    float m_drawScale = 1.0f;
    OsdView m_view = OsdView::IconText;
    bool m_highContrast = false;
    OsdPalette m_palette{};
    bool m_deviceResourcesValid = false;
    // Gölge komut listesinin hangi orijin/ölçek/kip için kurulduğu; üçü de
    // değişmezse liste yeniden kurulmaz (her karede kurmak pahalıdır).
    D2D1_POINT_2F m_shadowOrigin{-1.0f, -1.0f};
    float m_shadowScale = -1.0f;
};

}  // namespace kli
