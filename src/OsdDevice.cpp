// OsdDevice.cpp — OsdWindow'un D3D11 / D2D / DirectComposition zinciri, yüzey
// yönetimi ve kare çizimi (spec §4.3).
//
// OsdWindow sınıfının parçasıdır; bölünme yalnızca 400 satır sınırı içindir.
#include "OsdWindow.h"

#include <d2d1.h>
#include <dxgi1_2.h>

namespace kli {
namespace {

// Cihaz kaybı: zincirin tamamı yıkılıp yeniden kurulmalıdır.
bool IsDeviceLost(HRESULT hr) noexcept {
    return hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED ||
           hr == DXGI_ERROR_DEVICE_RESET;
}

// IDCompositionSurface::BeginDraw ile EndDraw arasında erken dönüş olursa yüzey
// açık kalır ve SONRAKİ tüm BeginDraw çağrıları başarısız olur — OSD bir daha
// hiç çizilmez. Kapanışı kapsam sonuna bağlayarak bu sınıf hatasını imkânsız kılar.
//
// Başarı yolunda Close() AÇIKÇA çağrılmalıdır: DirectComposition, yüzey çizimi
// hâlâ açıkken yapılan Commit'i DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED
// (0x88980801) ile reddeder — yani EndDraw, Commit'ten ÖNCE gelmek zorundadır.
// Yıkıcı yalnızca hata yollarındaki güvenlik ağıdır.
class SurfaceDrawScope {
public:
    explicit SurfaceDrawScope(IDCompositionSurface* surface) noexcept
        : m_surface(surface) {}
    ~SurfaceDrawScope() { (void)Close(); }

    // Birden fazla kez çağrılabilir; ikinci çağrı S_OK döner.
    HRESULT Close() noexcept {
        if (m_surface == nullptr) {
            return S_OK;
        }
        IDCompositionSurface* surface = m_surface;
        m_surface = nullptr;
        return surface->EndDraw();
    }

    SurfaceDrawScope(const SurfaceDrawScope&) = delete;
    SurfaceDrawScope& operator=(const SurfaceDrawScope&) = delete;

private:
    IDCompositionSurface* m_surface;
};

}  // namespace

// ---------------------------------------------------------------------------
// Zincir kurulumu (spec §4.3)
// ---------------------------------------------------------------------------

HRESULT OsdWindow::CreateDeviceChain() {
    if (m_hwnd == nullptr) {
        return E_UNEXPECTED;
    }
    if (m_deviceChainValid) {
        return S_OK;
    }

    // BGRA_SUPPORT olmadan D2D birlikte çalışması kurulamaz.
    constexpr UINT kFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    HRESULT hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, kFlags,
                                     nullptr, 0, D3D11_SDK_VERSION,
                                     m_d3dDevice.ReleaseAndGetAddressOf(), nullptr,
                                     nullptr);
    if (FAILED(hr)) {
        // Donanım sürücüsü yok/bozuk (RDP, bazı sanal makineler): WARP yazılım
        // rasterleştiricisi görsel olarak aynı sonucu verir, yalnızca CPU kullanır.
        LogV(L"Donanım D3D11 cihazı alınamadı (0x%08X), WARP'a düşülüyor",
             static_cast<unsigned>(hr));
        hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, kFlags,
                                 nullptr, 0, D3D11_SDK_VERSION,
                                 m_d3dDevice.ReleaseAndGetAddressOf(), nullptr, nullptr);
    }
    HR(hr);

    ComPtr<IDXGIDevice> dxgiDevice;
    HR(m_d3dDevice.As(&dxgiDevice));

    D2D1_FACTORY_OPTIONS options{};
    options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
    HR(::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
                           &options,
                           reinterpret_cast<void**>(m_d2dFactory.ReleaseAndGetAddressOf())));

    HR(m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice));
    HR(m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext));

    HR(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(
                                 m_dwriteFactory.ReleaseAndGetAddressOf())));

    HR(::DCompositionCreateDevice(dxgiDevice.Get(), __uuidof(IDCompositionDevice),
                                  reinterpret_cast<void**>(
                                      m_dcompDevice.ReleaseAndGetAddressOf())));
    // topmost=TRUE: hedef, aynı pencerenin diğer DComp hedeflerinin üstünde kalır.
    HR(m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget));
    HR(m_dcompDevice->CreateVisual(&m_visual));

    // Opaklık, IDCompositionDevice (v1) yolunda görselin kendisinde değil bir
    // efekt grubunda yaşar; animasyon da bu grup üzerinden verilir.
    HR(m_dcompDevice->CreateEffectGroup(&m_effectGroup));
    HR(m_visual->SetEffect(m_effectGroup.Get()));

    HR(m_dcompDevice->CreateScaleTransform(&m_scale));
    HR(m_visual->SetTransform(m_scale.Get()));
    // ZORUNLU: ölçek açıkça 1.0'a kurulmazsa görsel hiç çizilmez. Merkez,
    // yüzey oluşturulurken EnsureSurface içinde ayarlanır.
    HR(m_scale->SetScaleX(1.0f));
    HR(m_scale->SetScaleY(1.0f));

    HR(m_dcompTarget->SetRoot(m_visual.Get()));

    HR(m_renderer.CreateDeviceIndependentResources(m_d2dFactory.Get(),
                                                   m_dwriteFactory.Get()));
    m_renderer.SetTheme(m_theme, m_config.cardAlpha);
    HR(m_renderer.CreateDeviceResources(m_d2dContext.Get()));

    // Gizli doğar: opaklık sıfırda tutulur, ilk Show animasyonu 0'dan başlatır.
    HR(m_effectGroup->SetOpacity(0.0f));
    HR(m_dcompDevice->Commit());

    // Yüzey henüz yok; ilk PositionForCurrentMonitor doğru DPI ile oluşturur.
    m_surfaceDpi = 0;
    m_surfacePx = 0;
    m_deviceChainValid = true;
    return S_OK;
}

void OsdWindow::ReleaseDeviceChain() {
    m_renderer.ReleaseDeviceResources();

    // Yıkım sırası önemlidir: içerik önce görselden koparılır, yoksa DComp
    // serbest bırakılan yüzeye bir kare daha başvurabilir.
    if (m_visual) {
        m_visual->SetContent(nullptr);
        m_visual->SetEffect(nullptr);
        m_visual->SetTransform(static_cast<IDCompositionTransform*>(nullptr));
    }
    if (m_dcompTarget) {
        m_dcompTarget->SetRoot(nullptr);
    }
    if (m_dcompDevice) {
        m_dcompDevice->Commit();
    }

    m_surface.Reset();
    m_scale.Reset();
    m_effectGroup.Reset();
    m_visual.Reset();
    m_dcompTarget.Reset();
    m_dcompDevice.Reset();

    if (m_d2dContext) {
        m_d2dContext->SetTarget(nullptr);
    }
    m_d2dContext.Reset();
    m_d2dDevice.Reset();
    m_d2dFactory.Reset();
    m_dwriteFactory.Reset();
    m_d3dDevice.Reset();

    m_surfaceDpi = 0;
    m_surfacePx = 0;
    m_deviceChainValid = false;
}

// ---------------------------------------------------------------------------
// Yüzey
// ---------------------------------------------------------------------------

HRESULT OsdWindow::EnsureSurface(UINT dpi) {
    if (!m_deviceChainValid || !m_dcompDevice || !m_visual || !m_scale) {
        return E_UNEXPECTED;
    }
    if (dpi == 0) {
        dpi = kDefaultDpi;
    }
    if (m_surface && dpi == m_surfaceDpi) {
        return S_OK;
    }

    const int px = DipToPx(OsdLayout::kSurfaceSize, dpi);
    if (px <= 0) {
        return E_INVALIDARG;
    }
    const UINT side = static_cast<UINT>(px);

    HR(m_dcompDevice->CreateSurface(side, side, DXGI_FORMAT_B8G8R8A8_UNORM,
                                    DXGI_ALPHA_MODE_PREMULTIPLIED,
                                    m_surface.ReleaseAndGetAddressOf()));
    HR(m_visual->SetContent(m_surface.Get()));

    // Ölçek merkezi yüzeyin ortası — giriş animasyonu kartın merkezinden büyür
    // (spec §3.6 "merkez orijinli"). DComp dönüşümleri fiziksel piksel uzayındadır.
    const float center = static_cast<float>(side) * 0.5f;
    HR(m_scale->SetCenterX(center));
    HR(m_scale->SetCenterY(center));

    m_surfaceDpi = dpi;
    m_surfacePx = side;
    HR(m_dcompDevice->Commit());
    return S_OK;
}

// ---------------------------------------------------------------------------
// Kare çizimi
// ---------------------------------------------------------------------------

HRESULT OsdWindow::RenderFrame() {
    if (!m_deviceChainValid || !m_surface || !m_d2dContext || !m_dcompDevice) {
        return E_UNEXPECTED;
    }

    ComPtr<IDXGISurface> dxgiSurface;
    POINT offset{};
    HR(m_surface->BeginDraw(nullptr, __uuidof(IDXGISurface),
                            reinterpret_cast<void**>(dxgiSurface.GetAddressOf()),
                            &offset));
    // Bu noktadan sonraki her çıkış yolu EndDraw'ı garanti eder.
    SurfaceDrawScope drawScope(m_surface.Get());

    // Hedef bitmap'in DPI'sı monitör DPI'sına eşitlenir; böylece çizim kodu
    // baştan sona DIP konuşur ve gölge bulanıklığı da doğru ölçeklenir.
    const float dpi = static_cast<float>(m_surfaceDpi != 0 ? m_surfaceDpi : kDefaultDpi);
    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpi, dpi);

    ComPtr<ID2D1Bitmap1> target;
    HR(m_d2dContext->CreateBitmapFromDxgiSurface(dxgiSurface.Get(), props, &target));

    // DComp yüzeyleri bir atlasta yaşar: BeginDraw gerçek bir ofset döndürür
    // (ölçülen: 1,1). Ofset yok sayılırsa kart bir piksel kayar ve kenarlar
    // bulanıklaşır. Çizim DIP uzayında olduğu için ofset de DIP'e çevrilir.
    const D2D1_POINT_2F originDip{
        PxToDip(static_cast<float>(offset.x), m_surfaceDpi != 0 ? m_surfaceDpi : kDefaultDpi),
        PxToDip(static_cast<float>(offset.y), m_surfaceDpi != 0 ? m_surfaceDpi : kDefaultDpi)};

    m_d2dContext->SetTarget(target.Get());
    // SetTarget, bağlamın DPI'sını hedef bitmap'ten DEVRALMAZ — bitmap DPI'sı
    // yalnızca bitmap bir KAYNAK olarak kullanıldığında geçerlidir. Bağlamın
    // kendi DPI'sı açıkça ayarlanmazsa 96'da kalır ve tüm DIP ölçüleri piksele
    // 1:1 eşlenir: kart 366 piksellik yüzeyde 270 değil 180 piksel çizilir.
    m_d2dContext->SetDpi(dpi, dpi);
    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    const HRESULT renderHr = m_renderer.Render(m_d2dContext.Get(), m_content, originDip);
    const HRESULT endHr = m_d2dContext->EndDraw();
    m_d2dContext->SetTarget(nullptr);

    if (FAILED(endHr)) {
        return endHr;
    }
    HR(renderHr);

    // Yüzey çizimi Commit'ten ÖNCE kapatılır (bkz. SurfaceDrawScope notu).
    HR(drawScope.Close());
    HR(m_dcompDevice->Commit());
    return S_OK;
}

bool OsdWindow::RenderWithDeviceRecovery() {
    if (m_hwnd == nullptr) {
        return false;
    }

    const UINT dpi = m_surfaceDpi != 0 ? m_surfaceDpi : kDefaultDpi;

    HRESULT hr = m_deviceChainValid ? S_OK : E_UNEXPECTED;
    if (SUCCEEDED(hr)) {
        hr = EnsureSurface(dpi);
    }
    if (SUCCEEDED(hr)) {
        hr = RenderFrame();
    }
    if (SUCCEEDED(hr)) {
        return true;
    }

    if (!IsDeviceLost(hr) && m_deviceChainValid) {
        // Cihaz kaybı değilse yeniden kurmak sorunu çözmez; gürültü yapmadan bildir.
        LogV(L"OSD çizimi başarısız: 0x%08X", static_cast<unsigned>(hr));
        return false;
    }

    // Zincirin tamamı yıkılıp yeniden kurulur ve çizim BİR KEZ tekrar denenir
    // (spec §4.3 cihaz kaybı kuralı).
    LogV(L"Cihaz kaybı (0x%08X) — zincir yeniden kuruluyor", static_cast<unsigned>(hr));
    ReleaseDeviceChain();

    hr = CreateDeviceChain();
    if (SUCCEEDED(hr)) {
        hr = EnsureSurface(dpi);
    }
    if (SUCCEEDED(hr)) {
        hr = RenderFrame();
    }
    if (FAILED(hr)) {
        LogV(L"Zincir yeniden kurulduktan sonra da çizilemedi: 0x%08X",
             static_cast<unsigned>(hr));
        return false;
    }
    return true;
}

}  // namespace kli
