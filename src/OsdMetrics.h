// OsdMetrics.h — OSD görünüm kiplerinin ölçü tablosu (spec §3.1 / §3.2 + madde 13–15).
//
// NEDEN AYRI BAŞLIK: yüzey boyutunu OsdWindow, çizimi OsdRenderer hesaplar. İkisi
// ayrı ölçü kaynağı kullanırsa kart yüzeye sığmaz ya da kırpılır. Bu yüzden her
// iki taraf da SADECE buradaki tablodan okur; ölçü tek yerde tanımlıdır.
//
// Tüm değerler ÖLÇEKSİZ DIP'tir. Kullanıcının boyut çarpanı (OsdConfig::scale)
// yüzeye ve dünya dönüşümüne ayrıca uygulanır — bu tabloya karışmaz.
#pragma once

namespace kli {

// OSD kartının görünüm kipi. Settings::OsdViewMode ile SAYISAL OLARAK BİREBİR
// aynıdır; çeviriyi App yapar (spec §2: OsdWindow/OsdRenderer Settings'i tanımaz).
//
// GERİYE UYUM: 0 = 1.0'ın tek ve varsayılan görünümüdür. Ayar hiç yazılmamışsa
// (güncelleyen kullanıcı) 0 okunur ve kart birebir eskisi gibi çizilir.
enum class OsdView {
    IconText = 0,   // ikon + başlık + durum (1.0'ın görünümü, VARSAYILAN)
    IconOnly = 1,   // yalnızca ikon, küçük kare kart
    Bar = 2,        // geniş ve alçak çubuk: solda ikon, sağında tek satır
};

inline constexpr unsigned kOsdViewCount = 3;

// Metnin karttaki yerleşim biçimi. Kipin kendisinden ayrı tutulur çünkü çizim
// kodu "hangi kip" değil "metin nasıl duruyor" sorusunu sorar.
enum class OsdTextMode {
    None,      // metin çizilmez
    Stacked,   // ikonun altında iki satır, yatay ortalı
    Inline,    // ikonun sağında tek satır: "CAPS LOCK · Açık"
};

// Kipten BAĞIMSIZ ölçüler: gölge payı, kenarlık, punto, harf aralığı.
struct OsdLayout {
    static constexpr float kBorderThickness = 1.0f;
    // Yüksek kontrast kipinde kenarlık kalınlaşır: saydamlık kapatıldığı için
    // kartın sınırını yalnızca kenarlık taşır (madde 29).
    static constexpr float kBorderThicknessHc = 2.0f;

    // Gölge için her yönde 32 DIP pay. Kip ne olursa olsun aynıdır; gölge
    // yarıçapı da kipe göre değişmez.
    static constexpr float kSurfacePadding = 32.0f;

    // Kartın yüzey içindeki sol üst köşesi (gölge payı kadar içeride).
    static constexpr float kCardLeft = kSurfacePadding;
    static constexpr float kCardTop = kSurfacePadding;

    // Gölge
    static constexpr float kShadowBlurStdDev = 9.0f;
    static constexpr float kShadowOffsetY = 6.0f;

    // Punto: yığılmış yerleşimde başlık/durum farklı, satır içi yerleşimde İKİSİ
    // DE AYNI. Sebep ölçüsel: satır içi metinde iki parça aynı kutuya dikey
    // ortalanıyor; farklı puntoda taban çizgileri kayar ve satır eğri görünür.
    static constexpr float kTitleFontSize = 15.0f;
    static constexpr float kStatusFontSize = 13.0f;
    static constexpr float kBarFontSize = 13.0f;

    static constexpr float kTitleTracking = 0.5f;   // harf aralığı +0.5 DIP

    // Satır içi yerleşimde başlıkla durumu ayıran işaret.
    static constexpr const wchar_t* kInlineSeparator = L" · ";
};

// Tek bir görünüm kipinin tüm kart ölçüleri.
struct OsdMetrics {
    float cardW;
    float cardH;
    float cornerRadius;

    // İkon kutusu — kartın sol üst köşesine göre. Geometriler 72 birimlik kutuda
    // tanımlıdır (IconGeometry::kBoxSize); çizim iconBox/72 ile ölçekler.
    float iconBox;
    float iconLeft;
    float iconTop;

    OsdTextMode text;
    float textLeft;        // kartın soluna göre
    float textWidth;       // metin kutusunun genişliği
    float titleTop;        // Stacked: başlık satırının üstü
    float statusTop;       // Stacked: durum satırının üstü
    float textBoxHeight;   // Inline: dikey ortalama kutusunun yüksekliği

    [[nodiscard]] constexpr float surfaceW() const noexcept {
        return cardW + 2.0f * OsdLayout::kSurfacePadding;
    }
    [[nodiscard]] constexpr float surfaceH() const noexcept {
        return cardH + 2.0f * OsdLayout::kSurfacePadding;
    }
};

// Kip başına ölçü tablosu. Sıra OsdView ile birebirdir.
//
// KÖŞE YARIÇAPI kipe göre değişir ("orantılı"), ama birebir doğrusal DEĞİLDİR.
// 180 → 12 oranı (kenarın 1/15'i) küçük karta uygulansaydı 96 → 6.4 çıkardı ve
// kart gözle keskin görünürdü; algısal yuvarlaklık kenar küçüldükçe daha büyük
// bir orana ihtiyaç duyar, bu yüzden 10 seçildi. Çubukta kısa kenar 56'dır ve 12
// zaten o kenarın beşte biridir — daha büyüğü çubuğu hapa çevirirdi.
//
// ÇUBUK GENİŞLİĞİ 240 (görev tanımındaki 220 örneği değil): en uzun metin
// "SCROLL LOCK · Kapalı" 13 DIP puntoda ~142 DIP ölçülüyor. 220'de metin kutusu
// 144 DIP kalıyor ve iki DIP payla sığıyordu; 240'ta 164 DIP olup 22 DIP pay
// bırakıyor. Yine de çizim kırpma seçeneğiyle yapılır (bkz. OsdText.cpp): hiçbir
// çeviri kartın dışına taşamaz.
inline constexpr OsdMetrics kOsdMetrics[kOsdViewCount] = {
    // IconText — 1.0'ın ölçüleri, DEĞİŞTİRİLMEDİ.
    OsdMetrics{
        /*cardW*/ 180.0f, /*cardH*/ 180.0f, /*cornerRadius*/ 12.0f,
        /*iconBox*/ 72.0f, /*iconLeft*/ 54.0f, /*iconTop*/ 34.0f,
        /*text*/ OsdTextMode::Stacked,
        /*textLeft*/ 0.0f, /*textWidth*/ 180.0f,
        /*titleTop*/ 120.0f, /*statusTop*/ 144.0f, /*textBoxHeight*/ 40.0f,
    },
    // IconOnly — kart küçülür, ikon tek başına ortalanır.
    OsdMetrics{
        /*cardW*/ 96.0f, /*cardH*/ 96.0f, /*cornerRadius*/ 10.0f,
        /*iconBox*/ 56.0f, /*iconLeft*/ 20.0f, /*iconTop*/ 20.0f,
        /*text*/ OsdTextMode::None,
        /*textLeft*/ 0.0f, /*textWidth*/ 0.0f,
        /*titleTop*/ 0.0f, /*statusTop*/ 0.0f, /*textBoxHeight*/ 0.0f,
    },
    // Bar — solda 32 DIP ikon (16 kenar boşluğu), 12 DIP boşluk, kalan metin.
    OsdMetrics{
        /*cardW*/ 240.0f, /*cardH*/ 56.0f, /*cornerRadius*/ 12.0f,
        /*iconBox*/ 32.0f, /*iconLeft*/ 16.0f, /*iconTop*/ 12.0f,
        /*text*/ OsdTextMode::Inline,
        /*textLeft*/ 60.0f, /*textWidth*/ 164.0f,
        /*titleTop*/ 0.0f, /*statusTop*/ 0.0f, /*textBoxHeight*/ 56.0f,
    },
};

// Görev tanımındaki `OsdMetrics ForView(...)`. Ölçek parametresi BİLEREK yoktur:
// ölçeklemeyi tek bir yerde (dünya dönüşümü + yüzey boyutu) yapmak, ölçüleri
// önceden çarpmaktan daha güvenlidir — çarpılmış bir tablo çizim tarafına
// sızarsa kart iki kez ölçeklenir.
[[nodiscard]] constexpr const OsdMetrics& ForView(OsdView view) noexcept {
    const unsigned index = static_cast<unsigned>(view);
    return kOsdMetrics[index < kOsdViewCount ? index : 0u];
}

}  // namespace kli
