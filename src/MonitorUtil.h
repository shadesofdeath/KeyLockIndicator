// MonitorUtil.h — DPI, çalışma alanı, aktif monitör, tam ekran tespiti (spec §4.4).
#pragma once

#include <windows.h>

#include <vector>

namespace kli {

struct MonitorMetrics {
    HMONITOR handle = nullptr;
    RECT work{};        // MONITORINFO.rcWork — görev çubuğu hariç
    RECT full{};        // rcMonitor
    UINT dpi = 96;      // MDT_EFFECTIVE_DPI
};

namespace MonitorUtil {

// primaryMonitorOnly=false → imlecin bulunduğu monitör (GetCursorPos +
// MonitorFromPoint). true → ana monitör.
[[nodiscard]] MonitorMetrics Active(bool primaryMonitorOnly);

[[nodiscard]] MonitorMetrics ForHandle(HMONITOR mon);

[[nodiscard]] MonitorMetrics ForWindow(HWND hwnd);

// Bağlı tüm monitörler (EnumDisplayMonitors sırası). Ana monitör her zaman
// listenin BAŞINDADIR: "tüm monitörlerde göster" kipinde 0. sıradaki tutamaç
// zaten var olan OSD penceresine düşer, ek pencereler sonrakiler için açılır.
// Liste boş dönmez; sayım başarısız olursa ana monitörle tek öğeli döner.
[[nodiscard]] std::vector<HMONITOR> All();

// SHQueryUserNotificationState() → QUNS_BUSY / QUNS_RUNNING_D3D_FULL_SCREEN /
// QUNS_PRESENTATION_MODE ise true. Sorgu başarısızsa false (OSD bastırılmaz).
[[nodiscard]] bool IsPresentationOrFullscreen();

}  // namespace MonitorUtil
}  // namespace kli
