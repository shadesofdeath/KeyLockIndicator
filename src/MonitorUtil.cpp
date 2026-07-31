// MonitorUtil.cpp — Monitör ölçüleri, DPI ve tam ekran tespiti (spec §4.4).

#include "MonitorUtil.h"

#include "Util.h"

#include <shellapi.h>
#include <shellscalingapi.h>

#include <utility>

namespace kli {
namespace MonitorUtil {
namespace {

// Ölçü alınamadığında dönülecek yedek: boş dikdörtgen çağıranda sıfır boyutlu
// yerleşime yol açacağı için ana ekran ölçüleri kullanılır.
void FillPrimaryFallback(MonitorMetrics& m) noexcept {
    const LONG cx = ::GetSystemMetrics(SM_CXSCREEN);
    const LONG cy = ::GetSystemMetrics(SM_CYSCREEN);
    m.full = RECT{0, 0, cx, cy};
    m.work = m.full;
    if (!::SystemParametersInfoW(SPI_GETWORKAREA, 0, &m.work, 0)) {
        m.work = m.full;
    }
}

[[nodiscard]] MonitorMetrics Primary() {
    return ForHandle(::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));
}

}  // namespace

MonitorMetrics ForHandle(HMONITOR mon) {
    MonitorMetrics m{};
    m.handle = mon;

    MONITORINFO mi{sizeof(mi)};
    if (mon != nullptr && ::GetMonitorInfoW(mon, &mi)) {
        // rcWork kullanılır: görev çubuğu üstte de olabilir, OSD onun altına
        // konumlanmalı (spec §4.4).
        m.work = mi.rcWork;
        m.full = mi.rcMonitor;
    } else {
        LogV(L"GetMonitorInfoW başarısız (hata %lu) — ana ekran ölçülerine düşülüyor",
             ::GetLastError());
        FillPrimaryFallback(m);
        return m;
    }

    // Per-Monitor V2 farkındalığı manifestte zorunlu; bu yüzden monitörün
    // efektif DPI'sı sorulur, sistem DPI'sı değil.
    UINT dpiX = 0;
    UINT dpiY = 0;
    if (SUCCEEDED(::GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX != 0) {
        m.dpi = dpiX;  // X ve Y pratikte eşittir; ölçekleme tek eksene bağlanır
    }
    return m;
}

MonitorMetrics Active(bool primaryMonitorOnly) {
    if (primaryMonitorOnly) {
        return Primary();
    }

    POINT pt{};
    if (!::GetCursorPos(&pt)) {
        // Kilit ekranı / güvenli masaüstü sırasında başarısız olabilir.
        LogV(L"GetCursorPos başarısız (hata %lu) — ana monitör kullanılıyor",
             ::GetLastError());
        return Primary();
    }
    return ForHandle(::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST));
}

MonitorMetrics ForWindow(HWND hwnd) {
    return ForHandle(::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

std::vector<HMONITOR> All() {
    std::vector<HMONITOR> list;

    // EnumDisplayMonitors geri çağrısı C bağlantılı bir fonksiyon işaretçisi
    // ister; durum lParam ile taşınır (spec §9: global değişken yok).
    const MONITORENUMPROC proc = [](HMONITOR mon, HDC, LPRECT, LPARAM param) -> BOOL {
        auto* out = reinterpret_cast<std::vector<HMONITOR>*>(param);
        if (out != nullptr && mon != nullptr) {
            out->push_back(mon);
        }
        return TRUE;
    };
    ::EnumDisplayMonitors(nullptr, nullptr, proc, reinterpret_cast<LPARAM>(&list));

    if (list.empty()) {
        // Sayım başarısız (oturum kilidi, uzak masaüstü geçişi): tek monitörlü
        // davranışa düşülür, çağıran boş listeyle uğraşmasın.
        LogV(L"EnumDisplayMonitors hiçbir monitör döndürmedi — ana monitöre düşülüyor");
        const HMONITOR primary = ::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        list.push_back(primary);
        return list;
    }

    // Ana monitör başa alınır: "hepsi" kipinde mevcut OSD penceresi 0. sıradaki
    // ekrana bağlanır, tek monitörlü kurulumda hiç ek pencere açılmaz.
    const HMONITOR primary = ::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    for (size_t i = 1; i < list.size(); ++i) {
        if (list[i] == primary) {
            std::swap(list[0], list[i]);
            break;
        }
    }
    return list;
}

bool IsPresentationOrFullscreen() {
    QUERY_USER_NOTIFICATION_STATE state = QUNS_ACCEPTS_NOTIFICATIONS;
    if (FAILED(::SHQueryUserNotificationState(&state))) {
        // Sorgu başarısızsa OSD bastırılmaz: yanlış pozitif, göstergeyi hiç
        // görmemekten daha kötüdür.
        return false;
    }

    switch (state) {
        case QUNS_BUSY:                      // tam ekran uygulama / sunum
        case QUNS_RUNNING_D3D_FULL_SCREEN:   // exclusive fullscreen D3D
        case QUNS_PRESENTATION_MODE:
            return true;
        default:
            return false;
    }
}

}  // namespace MonitorUtil
}  // namespace kli
