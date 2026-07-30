// IconShapes.h — IconGeometry'nin dahili path üreticileri.
//
// Yalnızca IconGeometry.cpp tarafından kullanılır; dış API değildir.
// Tüm koordinatlar 72x72 DIP kutuya normalizedir (sol üst 0,0).
#pragma once

#include "Util.h"

#include <d2d1_1.h>

namespace kli {
namespace IconShapes {

struct Built {
    ComPtr<ID2D1Geometry> fill;
    ComPtr<ID2D1Geometry> stroke;
    float strokeWidth = 2.0f;
};

// Caps Lock: yukarı bakan ok + düz taban çizgisi.
// on=false → yalnızca 2 DIP dış hat. on=true → okun içi ve taban dolu.
HRESULT BuildCaps(ID2D1Factory1* factory, bool on, Built& out);

// Num Lock: yuvarlatılmış kare çerçeve + 3x3 nokta ızgarası (ortada büyük nokta).
// on=false → yalnızca çerçeve. on=true → tüm noktalar dolu.
HRESULT BuildNum(ID2D1Factory1* factory, bool on, Built& out);

// Scroll Lock: asma kilit gövdesi + dikey çift yönlü ok.
// on=false → açık kilit kancası. on=true → kapalı kanca + dolu gövde.
HRESULT BuildScroll(ID2D1Factory1* factory, bool on, Built& out);

// Birden fazla path'i tek geometriye toplar (D2D1_FILL_MODE_WINDING).
HRESULT Group(ID2D1Factory1* factory, ID2D1Geometry** items, UINT count,
              D2D1_FILL_MODE mode, ID2D1Geometry** out);

}  // namespace IconShapes
}  // namespace kli
