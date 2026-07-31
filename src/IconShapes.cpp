// IconShapes.cpp — IconGeometry'nin dahili path üreticileri (spec §3.5).
//
// Tüm koordinatlar 72x72 DIP kutuya normalizedir (sol üst 0,0); ölçekleme
// OsdRenderer'ın işidir, burada hep aynı kutuda konuşulur. Kontur genişliği
// 2.0 DIP'tir (Scroll Lock kancası hariç — orada 2.5 DIP daha dengeli durur).
//
// "Açık" ve "kapalı" hâller ayrı geometridir: renk körü kullanıcılar salt renk
// farkını ayırt edemeyeceği için siluetin kendisi değişmek zorundadır.
#include "IconShapes.h"

#include <d2d1helper.h>

namespace kli {
namespace IconShapes {
namespace {

// Kapalı, dolu bir çokgen. Nokta dizisinin ilk elemanı başlangıçtır; figür
// otomatik kapatıldığı için son noktayı tekrarlamaya gerek yoktur.
HRESULT MakePolygon(ID2D1Factory1* factory, const D2D1_POINT_2F* pts, UINT32 count,
                    ID2D1PathGeometry** out) {
    ComPtr<ID2D1PathGeometry> path;
    HR(factory->CreatePathGeometry(&path));

    ComPtr<ID2D1GeometrySink> sink;
    HR(path->Open(&sink));
    // Sink'in varsayılanı ALTERNATE; şekillerimiz kendi kendini kesmediği için
    // fark yok, ama grup düzeyindeki dolgu kipiyle karışmasın diye açık yazılır.
    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLines(pts + 1, count - 1);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    HR(sink->Close());

    *out = path.Detach();
    return S_OK;
}

HRESULT MakeRoundedRect(ID2D1Factory1* factory, float left, float top, float right,
                        float bottom, float radius, ID2D1Geometry** out) {
    const D2D1_ROUNDED_RECT rr{D2D1::RectF(left, top, right, bottom), radius, radius};
    ComPtr<ID2D1RoundedRectangleGeometry> geom;
    HR(factory->CreateRoundedRectangleGeometry(&rr, &geom));
    *out = geom.Detach();  // ID2D1Geometry'ye üst-tür dönüşümü; QueryInterface gerekmez
    return S_OK;
}

HRESULT MakeCircle(ID2D1Factory1* factory, float cx, float cy, float radius,
                   ID2D1Geometry** out) {
    const D2D1_ELLIPSE e{D2D1::Point2F(cx, cy), radius, radius};
    ComPtr<ID2D1EllipseGeometry> geom;
    HR(factory->CreateEllipseGeometry(&e, &geom));
    *out = geom.Detach();
    return S_OK;
}

// Asma kilit kancası: konturlanacağı için AÇIK figür (FIGURE_BEGIN_HOLLOW +
// FIGURE_END_OPEN), yoksa D2D uçları birleştirip yalancı bir kiriş çizer.
//
// closedLock=true  → gövdenin tam üstünde simetrik yarım daire (kilit kapalı)
// closedLock=false → yay sağ üste kaçar ve gövdeden ayrık biter (kilit açık)
HRESULT MakeHook(ID2D1Factory1* factory, bool closedLock, ID2D1PathGeometry** out) {
    ComPtr<ID2D1PathGeometry> path;
    HR(factory->CreatePathGeometry(&path));

    ComPtr<ID2D1GeometrySink> sink;
    HR(path->Open(&sink));
    sink->BeginFigure(D2D1::Point2F(25.0f, 33.0f), D2D1_FIGURE_BEGIN_HOLLOW);

    D2D1_ARC_SEGMENT arc{};
    if (closedLock) {
        arc.point = D2D1::Point2F(47.0f, 33.0f);
        arc.size = D2D1::SizeF(11.0f, 11.0f);
    } else {
        arc.point = D2D1::Point2F(50.0f, 22.0f);
        arc.size = D2D1::SizeF(12.0f, 12.0f);
    }
    arc.rotationAngle = 0.0f;
    arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
    arc.arcSize = D2D1_ARC_SIZE_LARGE;
    sink->AddArc(&arc);

    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    HR(sink->Close());

    *out = path.Detach();
    return S_OK;
}

}  // namespace

// ---------------------------------------------------------------------------
// Group
// ---------------------------------------------------------------------------
HRESULT Group(ID2D1Factory1* factory, ID2D1Geometry** items, UINT count,
              D2D1_FILL_MODE mode, ID2D1Geometry** out) {
    if (!factory || !items || count == 0 || !out) {
        return E_INVALIDARG;
    }
    *out = nullptr;

    ComPtr<ID2D1GeometryGroup> group;
    HR(factory->CreateGeometryGroup(mode, items, count, &group));
    // ID2D1GeometryGroup zaten ID2D1Geometry'den türer: sahiplik doğrudan
    // aktarılır, QueryInterface'e gerek yok.
    *out = group.Detach();
    return S_OK;
}

// ---------------------------------------------------------------------------
// Caps Lock — yukarı ok + taban çizgisi
// ---------------------------------------------------------------------------
HRESULT BuildCaps(ID2D1Factory1* factory, bool on, Built& out) {
    if (!factory) {
        return E_INVALIDARG;
    }
    out = Built{};

    // Ok gövdesi: uç (36,8), kanatlar y=34'te, sap y=50'de biter.
    constexpr D2D1_POINT_2F kArrow[] = {
        {36.0f, 8.0f},  {62.0f, 34.0f}, {48.0f, 34.0f}, {48.0f, 50.0f},
        {24.0f, 50.0f}, {24.0f, 34.0f}, {10.0f, 34.0f},
    };

    ComPtr<ID2D1PathGeometry> arrow;
    HR(MakePolygon(factory, kArrow, static_cast<UINT32>(_countof(kArrow)), &arrow));

    ComPtr<ID2D1Geometry> baseLine;
    HR(MakeRoundedRect(factory, 22.0f, 56.0f, 50.0f, 66.0f, 3.0f, &baseLine));

    ID2D1Geometry* items[] = {arrow.Get(), baseLine.Get()};
    ComPtr<ID2D1Geometry> combined;
    HR(Group(factory, items, static_cast<UINT>(_countof(items)),
             D2D1_FILL_MODE_WINDING, &combined));

    // Aynı silueti açıkken doldurur, kapalıyken yalnızca konturlarız.
    out.strokeWidth = 2.0f;
    if (on) {
        out.fill = combined;
    } else {
        out.stroke = combined;
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
// Num Lock — yuvarlatılmış kare çerçeve + 3x3 nokta ızgarası
// ---------------------------------------------------------------------------
HRESULT BuildNum(ID2D1Factory1* factory, bool on, Built& out) {
    if (!factory) {
        return E_INVALIDARG;
    }
    out = Built{};
    out.strokeWidth = 2.0f;

    // Çerçeve her iki durumda da konturlanır: ikon kimliği ondan okunur.
    ComPtr<ID2D1Geometry> frame;
    HR(MakeRoundedRect(factory, 9.0f, 9.0f, 63.0f, 63.0f, 10.0f, &frame));
    out.stroke = frame;

    if (!on) {
        return S_OK;  // kapalı: yalnızca çerçeve
    }

    constexpr float kGrid[] = {22.0f, 36.0f, 50.0f};
    constexpr size_t kDotCount = _countof(kGrid) * _countof(kGrid);

    ComPtr<ID2D1Geometry> dots[kDotCount];
    ID2D1Geometry* items[kDotCount]{};
    size_t n = 0;
    for (size_t row = 0; row < _countof(kGrid); ++row) {
        for (size_t col = 0; col < _countof(kGrid); ++col) {
            // Merkez nokta belirgin olarak büyük — sayısal tuş takımı okunurluğu.
            const float r = (row == 1u && col == 1u) ? 6.0f : 3.2f;
            HR(MakeCircle(factory, kGrid[col], kGrid[row], r, &dots[n]));
            items[n] = dots[n].Get();
            ++n;
        }
    }

    HR(Group(factory, items, static_cast<UINT>(n), D2D1_FILL_MODE_WINDING, &out.fill));
    return S_OK;
}

// ---------------------------------------------------------------------------
// Scroll Lock — asma kilit + dikey çift yönlü ok
// ---------------------------------------------------------------------------
HRESULT BuildScroll(ID2D1Factory1* factory, bool on, Built& out) {
    if (!factory) {
        return E_INVALIDARG;
    }
    out = Built{};

    ComPtr<ID2D1Geometry> body;
    HR(MakeRoundedRect(factory, 15.0f, 33.0f, 57.0f, 65.0f, 7.0f, &body));

    // Çift yönlü ok tek kapalı figürdür: yukarı başlık → gövde → aşağı başlık.
    constexpr D2D1_POINT_2F kArrows[] = {
        {36.0f, 38.0f}, {43.0f, 46.0f}, {38.0f, 46.0f}, {38.0f, 52.0f}, {43.0f, 52.0f},
        {36.0f, 60.0f}, {29.0f, 52.0f}, {34.0f, 52.0f}, {34.0f, 46.0f}, {29.0f, 46.0f},
    };
    ComPtr<ID2D1PathGeometry> arrows;
    HR(MakePolygon(factory, kArrows, static_cast<UINT32>(_countof(kArrows)), &arrows));

    if (on) {
        // ALTERNATE: ok, dolu gövdenin içinde DELİK olur — kilit "dolu ve kapalı"
        // görünürken ok yine de zeminden okunabilir kalır.
        ID2D1Geometry* filled[] = {body.Get(), arrows.Get()};
        HR(Group(factory, filled, static_cast<UINT>(_countof(filled)),
                 D2D1_FILL_MODE_ALTERNATE, &out.fill));

        ComPtr<ID2D1PathGeometry> hook;
        HR(MakeHook(factory, true, &hook));
        out.stroke = hook;
        out.strokeWidth = 2.5f;
    } else {
        // Kapalı durumda hiçbir şey doldurulmaz; gövde, ayrık kanca ve ok
        // birlikte konturlanır.
        ComPtr<ID2D1PathGeometry> hook;
        HR(MakeHook(factory, false, &hook));

        ID2D1Geometry* outlined[] = {body.Get(), hook.Get(), arrows.Get()};
        HR(Group(factory, outlined, static_cast<UINT>(_countof(outlined)),
                 D2D1_FILL_MODE_WINDING, &out.stroke));
        out.strokeWidth = 2.0f;
    }
    return S_OK;
}

// ---------------------------------------------------------------------------
// Klavye düzeni rozeti — konturlu gövde + dolu tuşlar (madde 28)
// ---------------------------------------------------------------------------
//
// Kilit tuşu ikonlarıyla aynı görsel dilde durur: dış hat 2 DIP kontur, iç
// öğeler dolu. Tuş ızgarası bilerek 4-4-boşluk düzenindedir; daha fazla tuş
// 32 DIP'lik çubuk kipinde lapa gibi görünüyordu (ölçüldü, bkz. OsdMetrics.h
// iconBox değerleri).
HRESULT BuildKeyboard(ID2D1Factory1* factory, Built& out) {
    if (!factory) {
        return E_INVALIDARG;
    }
    out = Built{};
    out.strokeWidth = 2.0f;

    ComPtr<ID2D1Geometry> frame;
    HR(MakeRoundedRect(factory, 6.0f, 17.0f, 66.0f, 57.0f, 7.0f, &frame));
    out.stroke = frame;

    // Üst iki sıra dörder tuş, altta boşluk çubuğu. Koordinatlar sol/üst/sağ/alt.
    constexpr float kKeys[][4] = {
        {13.0f, 24.0f, 21.0f, 30.0f}, {25.0f, 24.0f, 33.0f, 30.0f},
        {37.0f, 24.0f, 45.0f, 30.0f}, {49.0f, 24.0f, 57.0f, 30.0f},
        {13.0f, 33.0f, 21.0f, 39.0f}, {25.0f, 33.0f, 33.0f, 39.0f},
        {37.0f, 33.0f, 45.0f, 39.0f}, {49.0f, 33.0f, 57.0f, 39.0f},
        {21.0f, 42.0f, 49.0f, 48.0f},   // boşluk çubuğu
    };
    constexpr size_t kKeyCount = _countof(kKeys);

    ComPtr<ID2D1Geometry> keys[kKeyCount];
    ID2D1Geometry* items[kKeyCount]{};
    for (size_t i = 0; i < kKeyCount; ++i) {
        HR(MakeRoundedRect(factory, kKeys[i][0], kKeys[i][1], kKeys[i][2], kKeys[i][3],
                           2.0f, &keys[i]));
        items[i] = keys[i].Get();
    }

    HR(Group(factory, items, static_cast<UINT>(kKeyCount), D2D1_FILL_MODE_WINDING,
             &out.fill));
    return S_OK;
}

}  // namespace IconShapes
}  // namespace kli
