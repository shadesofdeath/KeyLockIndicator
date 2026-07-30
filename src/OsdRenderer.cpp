// OsdRenderer.cpp — Direct2D çizim: gölge + kare kart + ikon + metin (spec §3).
//
// Hedef bitmap'in DPI'sı monitör DPI'sına EŞİTTİR (bunu OsdWindow ayarlar), bu
// yüzden bu dosya her zaman DIP konuşur; hiçbir yerde manuel ölçek çarpanı yok.
//
// `originDip` yüzeyin çizim orijinidir (DComp yüzey atlası kayması) ve tüm
// koordinatlara eklenir. Genel öteleme için SetTransform KULLANILMAZ: efekt
// çıktısını basan DrawImage ile birleştiğinde sonucun konumu tanımsızdır.
// Geçici SetTransform yalnızca ikon çiziminde kullanılır ve hemen Identity'ye
// dönülür.
#include "OsdRenderer.h"

#include <d2d1effects.h>
#include <d2d1effects_2.h>
#include <dwrite_1.h>

#include <utility>

namespace kli {
namespace {

// Windows 11'in arayüz ailesi; Windows 10'da bulunmaz. DWrite var olmayan bir
// aile adı için de biçim üretir (sessizce yedek fonta düşer), bu yüzden aile
// varlığı sistem font koleksiyonundan açıkça sorgulanır.
constexpr wchar_t kPreferredFamily[] = L"Segoe UI Variable Display";
constexpr wchar_t kFallbackFamily[] = L"Segoe UI";

// Başlık düzeni için kutu yüksekliği. NO_WRAP + tek satır olduğu için yalnızca
// üst sınırdır; hizalama PARAGRAPH_ALIGNMENT_NEAR ile üstten yapılır.
constexpr float kTitleLayoutHeight = 40.0f;

// Kartın (veya gölge siluetinin) yuvarlatılmış dikdörtgeni. inset > 0 →
// kenarlığın yarım piksel kayması olmaması için içe çekilmiş kenar.
[[nodiscard]] D2D1_ROUNDED_RECT CardRoundedRect(D2D1_POINT_2F origin, float inset) noexcept {
    return D2D1::RoundedRect(
        D2D1::RectF(origin.x + inset,
                    origin.y + inset,
                    origin.x + OsdLayout::kCardSize - inset,
                    origin.y + OsdLayout::kCardSize - inset),
        OsdLayout::kCornerRadius - inset,
        OsdLayout::kCornerRadius - inset);
}

[[nodiscard]] const wchar_t* PickFontFamily(IDWriteFactory* factory) {
    ComPtr<IDWriteFontCollection> system;
    if (FAILED(factory->GetSystemFontCollection(&system, FALSE)) || !system) {
        return kFallbackFamily;
    }
    UINT32 index = 0;
    BOOL exists = FALSE;
    if (FAILED(system->FindFamilyName(kPreferredFamily, &index, &exists)) || !exists) {
        return kFallbackFamily;
    }
    return kPreferredFamily;
}

[[nodiscard]] HRESULT CreateFormat(IDWriteFactory* factory, const wchar_t* family,
                                   DWRITE_FONT_WEIGHT weight, float size,
                                   ComPtr<IDWriteTextFormat>& out) {
    ComPtr<IDWriteTextFormat> format;
    // Yerel ad nötr (L""): metin ölçüsü ve şekillendirme kullanıcı yereline
    // göre değişmesin, OSD her dilde aynı görünsün.
    HR(factory->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, size, L"", &format));
    HR(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER));
    HR(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    HR(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    out = std::move(format);
    return S_OK;
}

}  // namespace

// ---------------------------------------------------------------------------
// Cihazdan bağımsız kaynaklar
// ---------------------------------------------------------------------------

HRESULT OsdRenderer::CreateDeviceIndependentResources(ID2D1Factory1* d2dFactory,
                                                      IDWriteFactory* dwriteFactory) {
    if (d2dFactory == nullptr || dwriteFactory == nullptr) {
        return E_INVALIDARG;
    }
    m_d2dFactory = d2dFactory;
    m_dwriteFactory = dwriteFactory;

    HR(m_icons.Initialize(d2dFactory));

    const wchar_t* const family = PickFontFamily(dwriteFactory);
    HR(CreateFormat(dwriteFactory, family, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    OsdLayout::kTitleFontSize, m_titleFormat));
    HR(CreateFormat(dwriteFactory, family, DWRITE_FONT_WEIGHT_REGULAR,
                    OsdLayout::kStatusFontSize, m_statusFormat));

    // SetTheme henüz çağrılmadıysa fırçalar boş palet ile kurulmasın.
    m_palette = MakePalette(m_theme, m_cardAlpha);
    return S_OK;
}

// ---------------------------------------------------------------------------
// Cihaza bağlı kaynaklar
// ---------------------------------------------------------------------------

HRESULT OsdRenderer::CreateDeviceResources(ID2D1DeviceContext* ctx) {
    if (ctx == nullptr) {
        return E_INVALIDARG;
    }
    ReleaseDeviceResources();

    HR(ctx->CreateSolidColorBrush(m_palette.cardFill, &m_cardBrush));
    HR(ctx->CreateSolidColorBrush(m_palette.border, &m_borderBrush));
    HR(ctx->CreateSolidColorBrush(m_palette.iconOff, &m_iconBrush));
    HR(ctx->CreateSolidColorBrush(m_palette.title, &m_titleBrush));
    HR(ctx->CreateSolidColorBrush(m_palette.status, &m_statusBrush));

    HR(ctx->CreateEffect(CLSID_D2D1Shadow, &m_shadowEffect));

    // Komut listesi ve efekt girdisi yeni cihazda ilk Render'da kurulur.
    m_shadowSource.Reset();
    m_shadowOrigin = D2D1_POINT_2F{-1.0f, -1.0f};

    m_deviceResourcesValid = true;
    return S_OK;
}

void OsdRenderer::ReleaseDeviceResources() {
    // Cihazdan bağımsız kaynaklara (m_icons, metin biçimleri, fabrikalar)
    // dokunulmaz; cihaz kaybında onların yeniden üretilmesi gerekmez.
    m_cardBrush.Reset();
    m_borderBrush.Reset();
    m_iconBrush.Reset();
    m_titleBrush.Reset();
    m_statusBrush.Reset();
    m_shadowSource.Reset();
    m_shadowEffect.Reset();
    m_shadowOrigin = D2D1_POINT_2F{-1.0f, -1.0f};
    m_deviceResourcesValid = false;
}

// ---------------------------------------------------------------------------
// Tema
// ---------------------------------------------------------------------------

void OsdRenderer::SetTheme(AppTheme theme, float cardAlpha) {
    m_theme = theme;
    m_cardAlpha = Clamp(cardAlpha, 0.60f, 1.00f);
    m_palette = MakePalette(m_theme, m_cardAlpha);

    // Fırçalar yaşıyorsa yeniden oluşturulmaz; yalnızca renkleri güncellenir.
    // Böylece OSD görünürken tema değişimi anında ve tahsissiz uygulanır.
    if (m_cardBrush) {
        m_cardBrush->SetColor(m_palette.cardFill);
    }
    if (m_borderBrush) {
        m_borderBrush->SetColor(m_palette.border);
    }
    if (m_titleBrush) {
        m_titleBrush->SetColor(m_palette.title);
    }
    if (m_statusBrush) {
        m_statusBrush->SetColor(m_palette.status);
    }
    // İkon fırçasının rengi her çizimde açık/kapalı durumuna göre atanır.

    if (m_shadowEffect) {
        const D2D1_VECTOR_4F color{m_palette.shadow.r, m_palette.shadow.g,
                                   m_palette.shadow.b, m_palette.shadow.a};
        HR_LOG(m_shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, color));
    }
    // Gölge rengi değişti: komut listesi + efekt özellikleri yeniden kurulsun.
    m_shadowOrigin = D2D1_POINT_2F{-1.0f, -1.0f};
}

// ---------------------------------------------------------------------------
// Çizim
// ---------------------------------------------------------------------------

HRESULT OsdRenderer::Render(ID2D1DeviceContext* ctx, const OsdContent& content,
                            D2D1_POINT_2F originDip) {
    if (ctx == nullptr || !m_deviceResourcesValid) {
        return E_UNEXPECTED;
    }

    // Saydam hedefte ClearType alt piksel filtresi çalışmaz; kenarlarda siyah
    // artefakt bırakır (spec §11 "fade'de siyah kenar görünmez").
    ctx->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ctx->SetTransform(D2D1::Matrix3x2F::Identity());

    // Gölge kurulumu HER ŞEYDEN ÖNCE yapılır: komut listesi ikinci bir bağlamda
    // doldurulur ve bu bağlamın hedefi henüz bu kareye bir şey yazmamış olmalı.
    if (originDip.x != m_shadowOrigin.x || originDip.y != m_shadowOrigin.y) {
        HR_LOG(BuildShadowCommandList(ctx, originDip));
    }
    if (m_shadowEffect && m_shadowSource) {
        // Ofset komut listesine gömülüdür; DrawImage'a targetOffset verilmez.
        ctx->DrawImage(m_shadowEffect.Get());
    }

    const D2D1_POINT_2F cardOrigin{originDip.x + OsdLayout::kCardLeft,
                                   originDip.y + OsdLayout::kCardTop};

    ctx->FillRoundedRectangle(CardRoundedRect(cardOrigin, 0.0f), m_cardBrush.Get());
    // Kenarlık kartın İÇ kenarında: 1 DIP kalınlığın yarısı kadar inset,
    // aksi hâlde kontur kart sınırına taşar ve kenar yumuşak görünür.
    ctx->DrawRoundedRectangle(CardRoundedRect(cardOrigin, OsdLayout::kBorderInset),
                              m_borderBrush.Get(), OsdLayout::kBorderThickness);

    HR(DrawIcon(ctx, content, cardOrigin));
    HR(DrawText(ctx, content, cardOrigin));
    return S_OK;
}

HRESULT OsdRenderer::BuildShadowCommandList(ID2D1DeviceContext* ctx,
                                            D2D1_POINT_2F originDip) {
    if (m_shadowEffect == nullptr) {
        return E_UNEXPECTED;
    }

    // Çağıran zaten BeginDraw açtı; aynı bağlamda ikinci bir BeginDraw yasaktır.
    // Bu yüzden komut listesi AYNI ID2D1Device'tan açılan ikinci bir bağlamda
    // doldurulur — komut listesi ve efekt aynı cihaza ait olduğu için uyumludur,
    // iç içe BeginDraw da oluşmaz. (Başlık değişmez olduğu için bağlam üye
    // değil yereldir; yalnızca orijin/tema değiştiğinde kurulur, karede değil.)
    ComPtr<ID2D1Device> device;
    ctx->GetDevice(&device);
    if (!device) {
        return E_UNEXPECTED;
    }

    ComPtr<ID2D1DeviceContext> builder;
    HR(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &builder));

    // Komut listesi vektörel kalır ama rasterleştirme çözünürlüğü bağlamın
    // DPI'sından türetilir: gölgenin bulanıklığı monitör DPI'sında doğru olsun.
    float dpiX = 0.0f;
    float dpiY = 0.0f;
    ctx->GetDpi(&dpiX, &dpiY);
    builder->SetDpi(dpiX, dpiY);
    builder->SetUnitMode(ctx->GetUnitMode());
    builder->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    ComPtr<ID2D1CommandList> list;
    HR(builder->CreateCommandList(&list));

    // Siluet TAM OPAK çizilir; gölgenin alfası efektin Color özelliğinden gelir.
    ComPtr<ID2D1SolidColorBrush> silhouette;
    HR(builder->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &silhouette));

    const D2D1_POINT_2F shadowOrigin{
        originDip.x + OsdLayout::kCardLeft,
        originDip.y + OsdLayout::kCardTop + OsdLayout::kShadowOffsetY};

    builder->SetTarget(list.Get());
    builder->BeginDraw();
    // Clear YAPILMAZ: komut listesi piksel tutmaz, taze liste zaten boştur. Clip
    // olmadan yapılan bir Clear kayda "sonsuz dikdörtgeni boya" olarak girer ve
    // listenin sınırlarını sonsuza taşır; bulanıklık efekti girdisinin sınırından
    // türetildiği için gölge o zaman ya çok pahalıya rasterleşir ya da bozulur.
    builder->FillRoundedRectangle(CardRoundedRect(shadowOrigin, 0.0f), silhouette.Get());
    HR(builder->EndDraw());
    builder->SetTarget(nullptr);
    HR(list->Close());

    m_shadowSource = std::move(list);
    m_shadowEffect->SetInput(0, m_shadowSource.Get());
    HR(m_shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION,
                                OsdLayout::kShadowBlurStdDev));
    const D2D1_VECTOR_4F color{m_palette.shadow.r, m_palette.shadow.g,
                               m_palette.shadow.b, m_palette.shadow.a};
    HR(m_shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, color));

    m_shadowOrigin = originDip;
    return S_OK;
}

HRESULT OsdRenderer::DrawIcon(ID2D1DeviceContext* ctx, const OsdContent& content,
                              D2D1_POINT_2F cardOrigin) {
    if (!m_icons.Ready() || !m_iconBrush) {
        return S_OK;  // İkon yoksa kart + metin yine de çizilir.
    }

    const IconGeometry::Icon& icon = m_icons.Get(content.key, content.on);
    if (!icon.fill && !icon.stroke) {
        return S_OK;
    }

    // Erişilebilirlik: geometri de değişir (spec §3.5); renk tek ayırt edici
    // değildir, ama açık/kapalı kontrastı da korunur.
    m_iconBrush->SetColor(content.on ? m_palette.iconOn : m_palette.iconOff);

    // İkon kutusu kart içinde yatay ortalı, üstten kIconTop.
    const float left = cardOrigin.x + (OsdLayout::kCardSize - OsdLayout::kIconBox) * 0.5f;
    const float top = cardOrigin.y + OsdLayout::kIconTop;

    // Geometriler 72 birimlik kutuda tanımlı ve hedef kutu da 72 DIP olduğu
    // için ölçekleme gerekmez — yalnızca öteleme, yani kenarlar keskin kalır.
    ctx->SetTransform(D2D1::Matrix3x2F::Translation(left, top));
    if (icon.fill) {
        ctx->FillGeometry(icon.fill.Get(), m_iconBrush.Get());
    }
    if (icon.stroke) {
        ctx->DrawGeometry(icon.stroke.Get(), m_iconBrush.Get(), icon.strokeWidth);
    }
    ctx->SetTransform(D2D1::Matrix3x2F::Identity());
    return S_OK;
}

HRESULT OsdRenderer::DrawText(ID2D1DeviceContext* ctx, const OsdContent& content,
                              D2D1_POINT_2F cardOrigin) {
    if (!content.title.empty() && m_titleFormat && m_titleBrush && m_dwriteFactory) {
        const UINT32 length = static_cast<UINT32>(content.title.size());
        ComPtr<IDWriteTextLayout> layout;
        HR(m_dwriteFactory->CreateTextLayout(content.title.c_str(), length,
                                             m_titleFormat.Get(), OsdLayout::kCardSize,
                                             kTitleLayoutHeight, &layout));

        // Harf aralığı IDWriteTextFormat üzerinden ayarlanamaz; IDWriteTextLayout1
        // gerekir. QI başarısızsa (çok eski DWrite) aralık olmadan devam edilir.
        ComPtr<IDWriteTextLayout1> spacing;
        if (SUCCEEDED(layout.As(&spacing))) {
            const DWRITE_TEXT_RANGE range{0, length};
            HR_LOG(spacing->SetCharacterSpacing(0.0f, OsdLayout::kTitleTracking, 0.0f, range));
        }

        ctx->DrawTextLayout(D2D1::Point2F(cardOrigin.x, cardOrigin.y + OsdLayout::kTitleTop),
                            layout.Get(), m_titleBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }

    if (!content.status.empty() && m_statusFormat && m_statusBrush) {
        // Durum metninde aralık ayarı yok; doğrudan biçimle çizilir (tahsissiz).
        ctx->DrawTextW(content.status.c_str(),
                       static_cast<UINT32>(content.status.size()),
                       m_statusFormat.Get(),
                       D2D1::RectF(cardOrigin.x,
                                   cardOrigin.y + OsdLayout::kStatusTop,
                                   cardOrigin.x + OsdLayout::kCardSize,
                                   cardOrigin.y + OsdLayout::kCardSize),
                       m_statusBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    }
    return S_OK;
}

}  // namespace kli
