// OsdWindow.h — DirectComposition katmanlı OSD penceresi + yaşam döngüsü (spec §3.6, §4.3).
//
// KeyMonitor'ı, Settings'i veya App'i TANIMAZ. Yerleşim/süre/opaklık gibi
// yapılandırma App tarafından OsdConfig ile enjekte edilir.
#pragma once

#include "LockTypes.h"
#include "MonitorUtil.h"
#include "OsdMetrics.h"
#include "OsdRenderer.h"
#include "Theme.h"
#include "Util.h"

#include <windows.h>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>

#include <functional>
#include <string>

namespace kli {

// Settings::OsdPositionMode ile aynı sayısal değerler; App çevirir. Ayrı tutulur
// çünkü OsdWindow, Settings başlığına bağlanamaz (spec §2 bağımlılık yönü).
// Sıralamadaki boşluksuzluk ve 0/1/2'nin anlamı Settings.h'deki notla aynıdır.
enum class OsdPlacement {
    TopCenter = 0,
    Center = 1,
    BottomCenter = 2,
    TopLeft = 3,
    TopRight = 4,
    MiddleLeft = 5,
    MiddleRight = 6,
    BottomLeft = 7,
    BottomRight = 8,
    Custom = 9,
};

// Serbest (Custom) konumun oran ölçeği: 0 = çalışma alanının sol/üst kenarı,
// kCustomRangeMax = sağ/alt kenarı. 10000 seçildi çünkü 2560 px genişlikte
// çeyrek pikselden ince adım verir; yüzde (100 kademe) 25 px'lik sıçrama yapardı.
// Settings.h aynı ölçeği kMaxCustomPos ile tekrarlar — iki başlık birbirini
// tanıyamaz (spec §2), çeviriyi App yapar.
inline constexpr unsigned kCustomRangeMax = 10000;

struct OsdConfig {
    unsigned durationMs = 1400;                     // 800 – 4000
    OsdPlacement placement = OsdPlacement::TopCenter;
    unsigned topMarginDip = 72;                     // 0 – 400
    bool primaryMonitorOnly = false;
    bool suppressFullscreen = true;
    float cardAlpha = 0.82f;                        // 0.60 – 1.00
    // Kartın boyut çarpanı. Yüzey kSurfaceSize * scale kadar büyür ve OsdRenderer
    // tüm çizimi aynı çarpanla ölçekler; metin de ölçeklendiği için net kalır.
    float scale = 1.0f;                             // kMinScale – kMaxScale

    // OsdPlacement::Custom için: yüzeyin sol-üst köşesinin, monitörün çalışma
    // alanı içindeki oransal konumu. Piksel yerine oran saklanır ki çözünürlük,
    // DPI ya da monitör değişince konum makul bir yerde kalsın.
    unsigned customX = kCustomRangeMax / 2;         // 0 – kCustomRangeMax
    unsigned customY = kCustomRangeMax / 2;

    // Sabitlenmiş monitör (çoklu monitör "hepsi" kipi). nullptr ise konum
    // primaryMonitorOnly'ye göre imleçten ya da ana monitörden bulunur.
    HMONITOR pinnedMonitor = nullptr;

    // Görünüm kipi (madde 13–15). Kartın ölçüleri, yüzey boyutu ve metin
    // yerleşimi buradan türer; tablo OsdMetrics.h'dedir.
    OsdView view = OsdView::IconText;

    // Kalıcı rozet kipi (madde 16). true iken OSD hiç kaybolmaz: bekleme sayacı
    // kurulmaz, giriş/çıkış animasyonu uygulanmaz, kart seçilen konumda durup
    // tuş durumunu canlı gösterir. Tıklama geçirgenliği (WS_EX_TRANSPARENT)
    // aynen korunur; tam ekran bastırma da bu kipte geçerlidir.
    bool persistent = false;

    // Sistem yüksek kontrast kipi (madde 29). Palet sistem renklerinden türer,
    // kart tam opak olur ve kenarlık kalınlaşır.
    bool highContrast = false;
};

class OsdWindow {
public:
    OsdWindow() = default;
    OsdWindow(const OsdWindow&) = delete;
    OsdWindow& operator=(const OsdWindow&) = delete;
    ~OsdWindow();

    // Uygulama açılışında bir kez çağrılır; pencere ve tüm D3D/D2D/DComp zinciri
    // önceden hazırlanır (ilk gösterimde ~200 ms gecikme olmasın — spec §6).
    bool Create(HINSTANCE hInstance);
    void Destroy();

    void SetConfig(const OsdConfig& config);
    [[nodiscard]] const OsdConfig& Config() const noexcept { return m_config; }

    // Yeniden tetiklenebilir: görünürken çağrılırsa animasyon baştan başlamaz,
    // içerik anında güncellenir ve bekleme sayacı sıfırlanır.
    void Show(LockKey key, bool isOn);

    // Klavye düzeni kartı (madde 28). code: "TR", name: "Türkçe Q".
    //
    // KALICI ROZET KİPİNDE ETKİSİZDİR: rozet tek kart taşır ve düzen kartı,
    // kullanıcının ekranda tuttuğu kilit tuşu göstergesinin yerini KALICI olarak
    // alırdı. Düzen bildirimi tanımı gereği geçicidir.
    void ShowKeyboardLayout(const std::wstring& code, const std::wstring& name);

    void HideImmediate();

    void OnThemeChanged(AppTheme theme);
    void OnDpiOrMonitorChanged();

    // --- Konum seçme kipi (spec dışı ek; "Konumu ekrandan seç") -------------
    // Kip her BİTİŞİNDE çağrılır — iptalde de. committed=false ise x/y anlamsızdır
    // ve konum değişmemelidir; çağıranın arayüzü kipten çıkışı böyle öğrenir.
    // Oranın ölçeği kCustomRangeMax'tır.
    using PositionPickedCallback =
        std::function<void(bool committed, unsigned x, unsigned y)>;

    // OSD'yi kalıcı görünür yapar, tıklama geçirgenliğini GEÇİCİ olarak kaldırır
    // ve pencereyi fareyle sürüklenebilir hâle getirir. Bırakıldığında onPicked
    // çağrılır ve kip kendiliğinden kapanır; Esc / sağ tık iptal eder.
    void BeginPositionPicking(PositionPickedCallback onPicked);

    // commit=false: konum kaydedilmez (iptal). Kip zaten kapalıysa etkisizdir.
    void EndPositionPicking(bool commit);

    [[nodiscard]] bool PickingPosition() const noexcept { return m_picking; }

    [[nodiscard]] HWND Handle() const noexcept { return m_hwnd; }
    [[nodiscard]] bool Visible() const noexcept { return m_phase != Phase::Hidden; }

    // Animasyon süreleri (spec §3.6)
    static constexpr double kEnterDurationSec = 0.130;
    static constexpr double kExitDurationSec  = 0.200;
    static constexpr float  kEnterScaleFrom   = 0.94f;

    // Boyut çarpanının kabul edilen aralığı. Settings 75–150 % ile zaten kısıtlar;
    // bu sınırlar OsdWindow'un Settings'i tanımadığı için ayrıca uygulanır (§2).
    static constexpr float kMinScale = 0.50f;
    static constexpr float kMaxScale = 2.00f;

private:
    enum class Phase {
        Hidden,
        Entering,
        Dwell,
        Exiting,
    };

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HRESULT CreateDeviceChain();
    void ReleaseDeviceChain();
    HRESULT EnsureSurface(UINT dpi);
    HRESULT RenderFrame();
    bool RenderWithDeviceRecovery();

    // Ölçeklenmiş yüzey ölçüleri (DIP). Yüzey boyutu ile çizim ölçeği aynı
    // çarpandan VE aynı ölçü tablosundan türemek ZORUNDA: ayrışırlarsa kart
    // yüzeye sığmaz ya da kırpılır. Kaynak tek: OsdMetrics.h::ForView.
    // Kart artık kare değildir (minimal çubuk kipi), bu yüzden iki eksen ayrı.
    [[nodiscard]] const OsdMetrics& Metrics() const noexcept { return ForView(m_config.view); }
    [[nodiscard]] float SurfaceWidthDip() const noexcept {
        return Metrics().surfaceW() * m_config.scale;
    }
    [[nodiscard]] float SurfaceHeightDip() const noexcept {
        return Metrics().surfaceH() * m_config.scale;
    }
    [[nodiscard]] float SurfacePaddingDip() const noexcept {
        return OsdLayout::kSurfacePadding * m_config.scale;
    }

    // Gösterim yolunun ortak başı ve sonu (OsdShow.cpp). BeginShow, kartı
    // göstermenin mümkün olup olmadığını sorar (konum seçme kipi, tam ekran
    // bastırma, cihaz zinciri); Present ise içerik yazıldıktan SONRA faz
    // makinesini ve bekleme sayacını çalıştırır. İkisi arasında yalnızca
    // m_content doldurulur — kilit tuşu ve klavye düzeni kartlarının tek farkı
    // budur.
    [[nodiscard]] bool BeginShow();
    void Present();

    // Kalıcı rozeti (yeniden) gösterir; içerik m_content'ten gelir.
    void ShowPersistent();
    // Kalıcı rozet kipinde saniyede bir tam ekran durumunu yoklar ve rozeti
    // gizler/geri getirir. Geçici OSD'de bu kontrol Show() içinde yapılır ama
    // rozet zaten ekranda olduğu için ayrıca yoklanması gerekir.
    void PollFullscreenSuppression();

    // Aşağıdaki dördü OsdPosition.cpp içindedir (400 satır sınırı, spec §9).
    [[nodiscard]] MonitorMetrics TargetMonitor() const;
    void PositionForCurrentMonitor();
    // Konum seçme kipindeki fare/klavye mesajlarını tüketir; işlediyse true döner
    // ve sonucu result'a yazar.
    bool HandlePickMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT& result);
    void ApplyPickStyles(bool picking);

    void ApplyEnterAnimation(float fromOpacity);
    void ApplyExitAnimation();
    void SetStaticOpacity(float value);
    [[nodiscard]] float EstimateCurrentOpacity() const;
    void ArmDwellTimer();

    HINSTANCE m_instance = nullptr;
    HWND m_hwnd = nullptr;

    // D3D / D2D
    ComPtr<ID3D11Device> m_d3dDevice;
    ComPtr<ID2D1Factory1> m_d2dFactory;
    ComPtr<ID2D1Device> m_d2dDevice;
    ComPtr<ID2D1DeviceContext> m_d2dContext;
    ComPtr<IDWriteFactory> m_dwriteFactory;

    // DirectComposition
    ComPtr<IDCompositionDevice> m_dcompDevice;
    ComPtr<IDCompositionTarget> m_dcompTarget;
    ComPtr<IDCompositionVisual> m_visual;
    ComPtr<IDCompositionEffectGroup> m_effectGroup;
    ComPtr<IDCompositionScaleTransform> m_scale;
    ComPtr<IDCompositionSurface> m_surface;

    OsdRenderer m_renderer;
    OsdConfig m_config{};
    OsdContent m_content{};
    AppTheme m_theme = AppTheme::Dark;

    UINT m_surfaceDpi = 0;
    UINT m_surfaceW = 0;
    UINT m_surfaceH = 0;
    Phase m_phase = Phase::Hidden;
    ULONGLONG m_phaseStartTick = 0;
    bool m_deviceChainValid = false;

    // Konum seçme kipi
    PositionPickedCallback m_onPicked;
    POINT m_dragGrab{};        // fare ile pencerenin sol-üst köşesi arasındaki fark
    bool m_picking = false;
    bool m_dragging = false;
};

}  // namespace kli
