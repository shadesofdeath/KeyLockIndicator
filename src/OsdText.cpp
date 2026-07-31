// OsdText.cpp — OSD kartındaki metnin çizimi (spec §3.2 + madde 13–15).
//
// OsdRenderer sınıfının parçasıdır; bölünme yalnızca dosya başına 400 satır
// sınırı içindir (spec §9). OsdRenderer.cpp gölge/kart/ikon, bu dosya metin.
//
// Çağıran (OsdRenderer::Render) dünya dönüşümünü zaten kurdu: koordinatlar
// ÖLÇEKSİZ DIP'tir. DirectWrite, dönüşümün ölçeğini glif rasterleştirmesine
// taşır — metin her boyut çarpanında net kalır.
#include "OsdRenderer.h"

#include <dwrite_1.h>

#include <string>
#include <utility>

namespace kli {

// Başlık düzeni; harf aralığı da burada uygulanır. IDWriteTextFormat harf
// aralığını taşıyamaz, IDWriteTextLayout1 gerekir — bu yüzden başlık her iki
// metin kipinde de düzen nesnesiyle çizilir (durum metni aralık kullanmaz).
HRESULT OsdRenderer::MakeTitleLayout(const std::wstring& text, IDWriteTextFormat* format,
                                     float maxWidth, float maxHeight,
                                     ComPtr<IDWriteTextLayout>& out) const {
    if (!m_dwriteFactory || format == nullptr) {
        return E_UNEXPECTED;
    }
    const UINT32 length = static_cast<UINT32>(text.size());
    ComPtr<IDWriteTextLayout> layout;
    HR(m_dwriteFactory->CreateTextLayout(text.c_str(), length, format, maxWidth, maxHeight,
                                         &layout));

    // QI başarısızsa (çok eski DWrite) aralık olmadan devam edilir; metin yine
    // doğru yerde çıkar, yalnızca harf aralığı biraz dar olur.
    ComPtr<IDWriteTextLayout1> spacing;
    if (SUCCEEDED(layout.As(&spacing))) {
        const DWRITE_TEXT_RANGE range{0, length};
        HR_LOG(spacing->SetCharacterSpacing(0.0f, OsdLayout::kTitleTracking, 0.0f, range));
    }

    out = std::move(layout);
    return S_OK;
}

HRESULT OsdRenderer::DrawText(ID2D1DeviceContext* ctx, const OsdContent& content) {
    if (ctx == nullptr) {
        return E_UNEXPECTED;
    }
    const OsdMetrics& m = ForView(m_view);
    switch (m.text) {
        case OsdTextMode::None:
            // "Sadece ikon" kipi: kart yalnızca ikonu taşır (madde 13).
            return S_OK;
        case OsdTextMode::Stacked:
            return DrawStackedText(ctx, content, m);
        case OsdTextMode::Inline:
            return DrawInlineText(ctx, content, m);
    }
    return S_OK;
}

// İkonun ALTINDA iki satır, yatay ortalı — 1.0'ın tek yerleşimi (madde 14).
HRESULT OsdRenderer::DrawStackedText(ID2D1DeviceContext* ctx, const OsdContent& content,
                                     const OsdMetrics& m) {
    const float left = OsdLayout::kCardLeft + m.textLeft;

    if (!content.title.empty() && m_titleFormat && m_titleBrush) {
        ComPtr<IDWriteTextLayout> layout;
        HR(MakeTitleLayout(content.title, m_titleFormat.Get(), m.textWidth,
                           m.textBoxHeight, layout));
        ctx->DrawTextLayout(D2D1::Point2F(left, OsdLayout::kCardTop + m.titleTop),
                            layout.Get(), m_titleBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    if (!content.status.empty() && m_statusFormat && m_statusBrush) {
        // Durum metninde aralık ayarı yok; doğrudan biçimle çizilir (tahsissiz).
        ctx->DrawTextW(content.status.c_str(),
                       static_cast<UINT32>(content.status.size()),
                       m_statusFormat.Get(),
                       D2D1::RectF(left, OsdLayout::kCardTop + m.statusTop,
                                   left + m.textWidth,
                                   OsdLayout::kCardTop + m.cardH),
                       m_statusBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }
    return S_OK;
}

// İkonun SAĞINDA tek satır: "CAPS LOCK · Açık" (madde 15, minimal çubuk).
//
// İki parça ayrı çizilir çünkü başlık ile durumun fırçası farklıdır (durum daha
// soluk). Tek bir düzen nesnesinde iki fırça kullanmak özel bir metin çizici
// (IDWriteTextRenderer) gerektirirdi; iki çizim hem daha az kod hem daha ucuz.
// Konum kayması başlığın ÖLÇÜLEN genişliğinden gelir, tahmin edilmez.
//
// Her iki çizim de D2D1_DRAW_TEXT_OPTIONS_CLIP ile yapılır: ölçüler en uzun
// çeviriye göre seçildi (bkz. OsdMetrics.h) ama ileride eklenecek bir dil
// beklenenden genişse metin kartın dışına TAŞMAK yerine kırpılır.
HRESULT OsdRenderer::DrawInlineText(ID2D1DeviceContext* ctx, const OsdContent& content,
                                    const OsdMetrics& m) {
    if (!m_barTitleFormat || !m_barStatusFormat || !m_titleBrush || !m_statusBrush) {
        return S_OK;
    }
    const float left = OsdLayout::kCardLeft + m.textLeft;
    const float top = OsdLayout::kCardTop;
    const float bottom = top + m.textBoxHeight;

    float used = 0.0f;
    if (!content.title.empty()) {
        ComPtr<IDWriteTextLayout> layout;
        HR(MakeTitleLayout(content.title, m_barTitleFormat.Get(), m.textWidth,
                           m.textBoxHeight, layout));
        DWRITE_TEXT_METRICS metrics{};
        if (SUCCEEDED(layout->GetMetrics(&metrics))) {
            used = metrics.widthIncludingTrailingWhitespace;
        }
        ctx->DrawTextLayout(D2D1::Point2F(left, top), layout.Get(), m_titleBrush.Get(),
                            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    if (content.status.empty() || used >= m.textWidth) {
        return S_OK;
    }

    // Ayraç durumun başına eklenir: başlıktan sonra tek bir kaydırma yeter ve
    // ayracın rengi de durumla aynı olur (başlık vurgulu kalsın).
    std::wstring tail(OsdLayout::kInlineSeparator);
    tail += content.status;
    ctx->DrawTextW(tail.c_str(), static_cast<UINT32>(tail.size()), m_barStatusFormat.Get(),
                   D2D1::RectF(left + used, top, left + m.textWidth, bottom),
                   m_statusBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    return S_OK;
}

}  // namespace kli
