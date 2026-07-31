// OsdRenderer.cpp — Direct2D çizim: gölge + kart + ikon (spec §3).
//
// Metin çizimi OsdText.cpp içindedir; bölünme yalnızca dosya başına 400 satır
// sınırı içindir (spec §9), iki dosya aynı sınıfın parçalarıdır.
//
// Hedef bitmap'in DPI'sı monitör DPI'sına EŞİTTİR (bunu OsdWindow ayarlar), bu
// yüzden bu dosya her zaman DIP konuşur; hiçbir yerde manuel ölçek çarpanı yok.
//
// `originDip` yüzeyin çizim orijinidir (DComp yüzey atlası kayması). Kart
// yerleşimi HER ZAMAN ölçeksiz DIP olarak yazılır; ölçek (OsdConfig::scale) ve
// bu orijin, vektör çizimleri için kurulan dünya dönüşümünde birleşir
// (WorldTransform). Metin de bu dönüşümün altında çizildiği için DirectWrite
// glifleri ölçeklenmiş punto boyutunda rasterleştirir ve net kalır.
//
// DrawImage'ı (gölge efekti) saran çizimde dönüşüm KULLANILMAZ: efekt çıktısının
// dönüşümle birleştiğinde konumu tanımsızdır. Bunun yerine ölçek gölgenin komut
// listesine gömülür (BuildShadowCommandList aynı dönüşümü builder'a kurar) ve
// bulanıklık yarıçapı da ölçekle çarpılır.
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

// Kartın yüzey içindeki sol üst köşesi — ÖLÇEKSİZ DIP. Ölçek dünya dönüşümünden
// gelir, kip değişse de bu köşe sabittir (gölge payı her kipte aynı).
constexpr D2D1_POINT_2F kCardOrigin{OsdLayout::kCardLeft, OsdLayout::kCardTop};

// Kartın (veya gölge siluetinin) yuvarlatılmış dikdörtgeni. inset > 0 →
// kenarlığın yarım piksel kayması olmaması için içe çekilmiş kenar.
[[nodiscard]] D2D1_ROUNDED_RECT CardRoundedRect(const OsdMetrics& m, float offsetY,
                                                float inset) noexcept {
    const float left = kCardOrigin.x + inset;
    const float top = kCardOrigin.y + offsetY + inset;
    return D2D1::RoundedRect(
        D2D1::RectF(left, top, kCardOrigin.x + m.cardW - inset,
                    kCardOrigin.y + offsetY + m.cardH - inset),
        m.cornerRadius - inset, m.cornerRadius - inset);
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
                                   DWRITE_TEXT_ALIGNMENT align,
                                   DWRITE_PARAGRAPH_ALIGNMENT paragraph,
                                   ComPtr<IDWriteTextFormat>& out) {
    ComPtr<IDWriteTextFormat> format;
    // Yerel ad nötr (L""): metin ölçüsü ve şekillendirme kullanıcı yereline
    // göre değişmesin, OSD her dilde aynı görünsün.
    HR(factory->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
                                 DWRITE_FONT_STRETCH_NORMAL, size, L"", &format));
    HR(format->SetTextAlignment(align));
    HR(format->SetParagraphAlignment(paragraph));
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
    // Yığılmış yerleşim: ortalı, üstten hizalı (konum tamamen y ile verilir).
    HR(CreateFormat(dwriteFactory, family, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    OsdLayout::kTitleFontSize, DWRITE_TEXT_ALIGNMENT_CENTER,
                    DWRITE_PARAGRAPH_ALIGNMENT_NEAR, m_titleFormat));
    HR(CreateFormat(dwriteFactory, family, DWRITE_FONT_WEIGHT_REGULAR,
                    OsdLayout::kStatusFontSize, DWRITE_TEXT_ALIGNMENT_CENTER,
                    DWRITE_PARAGRAPH_ALIGNMENT_NEAR, m_statusFormat));
    // Satır içi yerleşim: sola dayalı ve kutuya dikey ORTALI. İki biçimin puntosu
    // da aynıdır; farklı olsaydı ortalanan iki parçanın taban çizgisi kayardı.
    HR(CreateFormat(dwriteFactory, family, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                    OsdLayout::kBarFontSize, DWRITE_TEXT_ALIGNMENT_LEADING,
                    DWRITE_PARAGRAPH_ALIGNMENT_CENTER, m_barTitleFormat));
    HR(CreateFormat(dwriteFactory, family, DWRITE_FONT_WEIGHT_REGULAR,
                    OsdLayout::kBarFontSize, DWRITE_TEXT_ALIGNMENT_LEADING,
                    DWRITE_PARAGRAPH_ALIGNMENT_CENTER, m_barStatusFormat));

    // SetTheme henüz çağrılmadıysa fırçalar boş palet ile kurulmasın.
    m_palette = MakePalette(m_theme, m_cardAlpha, m_highContrast);
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
    m_shadowScale = -1.0f;

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
    m_shadowScale = -1.0f;
    m_deviceResourcesValid = false;
}

// ---------------------------------------------------------------------------
// Tema
// ---------------------------------------------------------------------------

void OsdRenderer::SetTheme(AppTheme theme, float cardAlpha, bool highContrast) {
    m_theme = theme;
    m_highContrast = highContrast;
    m_cardAlpha = Clamp(cardAlpha, 0.60f, 1.00f);
    m_palette = MakePalette(m_theme, m_cardAlpha, m_highContrast);

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

void OsdRenderer::SetScale(float scale) {
    // Üst/alt sınır yalnızca akıl sağlığı kontrolü; gerçek aralığı Settings kısıtlar.
    const float next = Clamp(scale, 0.25f, 4.0f);
    if (next == m_drawScale) {
        return;
    }
    m_drawScale = next;
    // Gölge siluetinin geometrisi ve bulanıklık yarıçapı ölçeğe bağlıdır.
    m_shadowScale = -1.0f;
}

void OsdRenderer::SetView(OsdView view) {
    if (view == m_view) {
        return;
    }
    m_view = view;
    // Kartın boyutu ve köşe yarıçapı değişti: gölge silueti de yeniden kurulmalı.
    m_shadowScale = -1.0f;
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
    const OsdMetrics& m = ForView(m_view);

    // Saydam hedefte ClearType alt piksel filtresi çalışmaz; kenarlarda siyah
    // artefakt bırakır (spec §11 "fade'de siyah kenar görünmez").
    ctx->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    ctx->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ctx->SetTransform(D2D1::Matrix3x2F::Identity());

    // Gölge kurulumu HER ŞEYDEN ÖNCE yapılır: komut listesi ikinci bir bağlamda
    // doldurulur ve bu bağlamın hedefi henüz bu kareye bir şey yazmamış olmalı.
    // Yüksek kontrastta gölge alfası sıfırdır; boş bir bulanıklık hesaplamak
    // yerine tüm gölge yolu atlanır (madde 29: saydamlık okunabilirliği bozmasın).
    if (m_palette.shadow.a > 0.0f) {
        if (originDip.x != m_shadowOrigin.x || originDip.y != m_shadowOrigin.y ||
            m_drawScale != m_shadowScale) {
            HR_LOG(BuildShadowCommandList(ctx, originDip));
        }
        if (m_shadowEffect && m_shadowSource) {
            // Ofset ve ölçek komut listesine gömülüdür; DrawImage dönüşümsüz çağrılır.
            ctx->DrawImage(m_shadowEffect.Get());
        }
    }

    // Bundan sonraki her vektör/metin çizimi ölçeklenir. Kart yerleşimi ölçeksiz
    // DIP kalır; dönüşüm kaldırıldığında OsdMetrics ölçüleri aynen geri gelir.
    const D2D1::Matrix3x2F world = WorldTransform(originDip);
    ctx->SetTransform(world);

    ctx->FillRoundedRectangle(CardRoundedRect(m, 0.0f, 0.0f), m_cardBrush.Get());
    // Kenarlık kartın İÇ kenarında: kalınlığın yarısı kadar inset, aksi hâlde
    // kontur kart sınırına taşar ve kenar yumuşak görünür. Yüksek kontrastta
    // kalınlık iki katına çıkar; kartın sınırını yalnızca kenarlık taşır.
    const float thickness =
        m_highContrast ? OsdLayout::kBorderThicknessHc : OsdLayout::kBorderThickness;
    ctx->DrawRoundedRectangle(CardRoundedRect(m, 0.0f, thickness * 0.5f),
                              m_borderBrush.Get(), thickness);

    HR(DrawIcon(ctx, content, world));
    HR(DrawText(ctx, content));
    ctx->SetTransform(D2D1::Matrix3x2F::Identity());
    return S_OK;
}

D2D1::Matrix3x2F OsdRenderer::WorldTransform(D2D1_POINT_2F originDip) const noexcept {
    // Sıra ÖNEMLİ: önce ölçek, sonra öteleme. Ters sırada yüzey atlası kayması da
    // ölçeklenir ve kart alt piksel kayarak bulanıklaşır.
    return D2D1::Matrix3x2F::Scale(m_drawScale, m_drawScale) *
           D2D1::Matrix3x2F::Translation(originDip.x, originDip.y);
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
    // Kartla AYNI dönüşüm: gölge silueti ölçekle birlikte büyür ve konumu kartın
    // altında kalır. Dönüşüm komut listesine kaydedilir, DrawImage'a taşınmaz.
    builder->SetTransform(WorldTransform(originDip));

    ComPtr<ID2D1CommandList> list;
    HR(builder->CreateCommandList(&list));

    // Siluet TAM OPAK çizilir; gölgenin alfası efektin Color özelliğinden gelir.
    ComPtr<ID2D1SolidColorBrush> silhouette;
    HR(builder->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &silhouette));

    builder->SetTarget(list.Get());
    builder->BeginDraw();
    // Clear YAPILMAZ: komut listesi piksel tutmaz, taze liste zaten boştur. Clip
    // olmadan yapılan bir Clear kayda "sonsuz dikdörtgeni boya" olarak girer ve
    // listenin sınırlarını sonsuza taşır; bulanıklık efekti girdisinin sınırından
    // türetildiği için gölge o zaman ya çok pahalıya rasterleşir ya da bozulur.
    builder->FillRoundedRectangle(
        CardRoundedRect(ForView(m_view), OsdLayout::kShadowOffsetY, 0.0f), silhouette.Get());
    HR(builder->EndDraw());
    builder->SetTarget(nullptr);
    HR(list->Close());

    m_shadowSource = std::move(list);
    m_shadowEffect->SetInput(0, m_shadowSource.Get());
    // Bulanıklık DIP cinsindendir ve komut listesine gömülü dönüşümden etkilenmez;
    // silüet ölçekle büyüdüğü için yarıçap da elle ölçeklenmelidir.
    HR(m_shadowEffect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION,
                                OsdLayout::kShadowBlurStdDev * m_drawScale));
    const D2D1_VECTOR_4F color{m_palette.shadow.r, m_palette.shadow.g,
                               m_palette.shadow.b, m_palette.shadow.a};
    HR(m_shadowEffect->SetValue(D2D1_SHADOW_PROP_COLOR, color));

    m_shadowOrigin = originDip;
    m_shadowScale = m_drawScale;
    return S_OK;
}

HRESULT OsdRenderer::DrawIcon(ID2D1DeviceContext* ctx, const OsdContent& content,
                              const D2D1::Matrix3x2F& world) {
    if (!m_icons.Ready() || !m_iconBrush) {
        return S_OK;  // İkon yoksa kart + metin yine de çizilir.
    }

    // Klavye düzeni kartının kendi rozeti vardır ve "açık/kapalı" hâli yoktur.
    const IconGeometry::Icon& icon =
        content.keyboardLayout ? m_icons.Keyboard() : m_icons.Get(content.key, content.on);
    if (!icon.fill && !icon.stroke) {
        return S_OK;
    }
    const OsdMetrics& m = ForView(m_view);

    // Erişilebilirlik: geometri de değişir (spec §3.5); renk tek ayırt edici
    // değildir, ama açık/kapalı kontrastı da korunur. Düzen rozeti bir durum
    // taşımadığı için daima vurgulu ("açık") renkle çizilir.
    m_iconBrush->SetColor((content.on || content.keyboardLayout) ? m_palette.iconOn
                                                                 : m_palette.iconOff);

    // Geometriler 72 birimlik kutuda tanımlıdır; hedef kutu kipe göre 32–72 DIP
    // arasında değişir, bu yüzden yerel dönüşüm öteleme + kutu oranıdır. Dünya
    // dönüşümünün ÖNÜNE eklenir, böylece kartla aynı çarpanla ölçeklenir.
    const float iconScale = m.iconBox / IconGeometry::kBoxSize;
    const float left = kCardOrigin.x + m.iconLeft;
    const float top = kCardOrigin.y + m.iconTop;
    ctx->SetTransform(D2D1::Matrix3x2F::Scale(iconScale, iconScale) *
                      D2D1::Matrix3x2F::Translation(left, top) * world);
    if (icon.fill) {
        ctx->FillGeometry(icon.fill.Get(), m_iconBrush.Get());
    }
    if (icon.stroke) {
        // Kontur kalınlığı 72'lik kutu ölçeğindedir; dönüşüm zaten ölçekliyor,
        // bu yüzden değer olduğu gibi verilir (aksi hâlde iki kez ölçeklenirdi).
        ctx->DrawGeometry(icon.stroke.Get(), m_iconBrush.Get(), icon.strokeWidth);
    }
    ctx->SetTransform(world);
    return S_OK;
}

}  // namespace kli
