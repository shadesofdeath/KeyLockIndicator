// OsdWindow.h — DirectComposition katmanlı OSD penceresi + yaşam döngüsü (spec §3.6, §4.3).
//
// KeyMonitor'ı, Settings'i veya App'i TANIMAZ. Yerleşim/süre/opaklık gibi
// yapılandırma App tarafından OsdConfig ile enjekte edilir.
#pragma once

#include "LockTypes.h"
#include "MonitorUtil.h"
#include "OsdRenderer.h"
#include "Theme.h"
#include "Util.h"

#include <windows.h>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>

namespace kli {

// Settings::OsdPositionMode ile aynı sayısal değerler; App çevirir. Ayrı tutulur
// çünkü OsdWindow, Settings başlığına bağlanamaz (spec §2 bağımlılık yönü).
enum class OsdPlacement {
    Top = 0,
    Center = 1,
    Bottom = 2,
};

struct OsdConfig {
    unsigned durationMs = 1400;                     // 800 – 4000
    OsdPlacement placement = OsdPlacement::Top;
    unsigned topMarginDip = 72;                     // 0 – 400
    bool primaryMonitorOnly = false;
    bool suppressFullscreen = true;
    float cardAlpha = 0.82f;                        // 0.60 – 1.00
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

    void HideImmediate();

    void OnThemeChanged(AppTheme theme);
    void OnDpiOrMonitorChanged();

    [[nodiscard]] HWND Handle() const noexcept { return m_hwnd; }
    [[nodiscard]] bool Visible() const noexcept { return m_phase != Phase::Hidden; }

    // Animasyon süreleri (spec §3.6)
    static constexpr double kEnterDurationSec = 0.130;
    static constexpr double kExitDurationSec  = 0.200;
    static constexpr float  kEnterScaleFrom   = 0.94f;

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

    void PositionForCurrentMonitor();
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
    UINT m_surfacePx = 0;
    Phase m_phase = Phase::Hidden;
    ULONGLONG m_phaseStartTick = 0;
    bool m_deviceChainValid = false;
};

}  // namespace kli
