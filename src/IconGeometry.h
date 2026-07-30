// IconGeometry.h — Vektör ikon path'leri (spec §3.5).
//
// Bitmap ve Segoe Fluent Icons kullanılmaz; her ikon kod ile ID2D1PathGeometry
// olarak üretilir ve önbelleğe alınır. Koordinatlar 72x72 DIP kutuya
// normalize edilmiştir (sol üst 0,0).
//
// "Açık" ve "kapalı" hâller AYRI geometridir — yalnızca renk değişimiyle
// yetinilmez; bu renk körü kullanıcılar için erişilebilirlik gereğidir.
#pragma once

#include "LockTypes.h"
#include "Util.h"

#include <d2d1_1.h>

namespace kli {

class IconGeometry {
public:
    struct Icon {
        ComPtr<ID2D1Geometry> fill;    // Doldurulacak parçalar; nullptr olabilir
        ComPtr<ID2D1Geometry> stroke;  // Konturlanacak parçalar; nullptr olabilir
        float strokeWidth = 2.0f;      // 72 birimlik kutu ölçeğinde DIP
    };

    IconGeometry() = default;
    IconGeometry(const IconGeometry&) = delete;
    IconGeometry& operator=(const IconGeometry&) = delete;

    // Altı geometriyi (3 tuş x 2 durum) üretir. Cihazdan bağımsızdır;
    // ID2D1Factory1 yaşadığı sürece geçerlidir, cihaz kaybında yeniden
    // oluşturulması gerekmez.
    HRESULT Initialize(ID2D1Factory1* factory);
    void Reset();

    [[nodiscard]] bool Ready() const noexcept { return m_ready; }
    [[nodiscard]] const Icon& Get(LockKey key, bool on) const noexcept;

    static constexpr float kBoxSize = 72.0f;

private:
    Icon m_icons[kLockKeyCount * 2]{};
    Icon m_empty{};
    bool m_ready = false;
};

}  // namespace kli
