// OsdAnimation.cpp — OsdWindow'un DirectComposition animasyonları (spec §3.6).
//
// OsdWindow sınıfının parçasıdır; bölünme yalnızca 400 satır sınırı içindir.
//
// | Faz                      | Süre    | Eğri            |
// |--------------------------|---------|-----------------|
// | Giriş — opaklık 0→1      | 130 ms  | cubic ease-out  |
// | Giriş — ölçek 0.94→1.0   | 130 ms  | cubic ease-out  |
// | Çıkış — opaklık 1→0      | 200 ms  | cubic ease-in   |
#include "OsdWindow.h"

namespace kli {
namespace {

// IDCompositionAnimation::AddCubic(beginOffset, c0, c1, c2, c3) segmenti
//   v(t) = c0 + c1*(t-beginOffset) + c2*(t-beginOffset)² + c3*(t-beginOffset)³
// olarak değerlendirir. Tüm segmentlerimiz beginOffset = 0'dan başladığı için
// göreli/mutlak zaman belirsizliği hiç doğmaz.
//
// Cubic ease-out, from → 1.0 (u = t/d):
//   v(u) = from + k·(3u − 3u² + u³),  k = 1 − from
// Katsayılar u = t/d yerine t cinsinden yazılır:
//   c0 = from, c1 = 3k/d, c2 = −3k/d², c3 = k/d³
HRESULT AddEaseOutTo1(IDCompositionAnimation* anim, float from, double d) {
    const double k = 1.0 - static_cast<double>(from);
    HR(anim->AddCubic(0.0,
                      from,
                      static_cast<float>(3.0 * k / d),
                      static_cast<float>(-3.0 * k / (d * d)),
                      static_cast<float>(k / (d * d * d))));
    // End sonrası animasyon bitiş değerinde SABİT kalır (bekleme fazı budur).
    HR(anim->End(d, 1.0f));
    return S_OK;
}

// Cubic ease-out, from → to (ölçek için: 0.94 → 1.0)
HRESULT AddEaseOut(IDCompositionAnimation* anim, float from, float to, double d) {
    const double k = static_cast<double>(to) - static_cast<double>(from);
    HR(anim->AddCubic(0.0,
                      from,
                      static_cast<float>(3.0 * k / d),
                      static_cast<float>(-3.0 * k / (d * d)),
                      static_cast<float>(k / (d * d * d))));
    HR(anim->End(d, to));
    return S_OK;
}

// Cubic ease-in, 1.0 → 0.0:  v(u) = 1 − u³
HRESULT AddEaseInTo0(IDCompositionAnimation* anim, double d) {
    HR(anim->AddCubic(0.0, 1.0f, 0.0f, 0.0f, static_cast<float>(-1.0 / (d * d * d))));
    HR(anim->End(d, 0.0f));
    return S_OK;
}

}  // namespace

void OsdWindow::ApplyEnterAnimation(float fromOpacity) {
    if (!m_deviceChainValid || !m_dcompDevice || !m_effectGroup) {
        return;
    }
    const float from = Clamp(fromOpacity, 0.0f, 1.0f);

    // Zaten tam opaksa animasyona gerek yok; boş bir eğri kurmak yerine sabitle.
    if (from >= 0.999f) {
        SetStaticOpacity(1.0f);
        m_phaseStartTick = ::GetTickCount64();
        return;
    }

    ComPtr<IDCompositionAnimation> opacity;
    if (FAILED(m_dcompDevice->CreateAnimation(&opacity))) {
        SetStaticOpacity(1.0f);
        return;
    }
    if (FAILED(AddEaseOutTo1(opacity.Get(), from, kEnterDurationSec))) {
        SetStaticOpacity(1.0f);
        return;
    }
    HR_LOG(m_effectGroup->SetOpacity(opacity.Get()));

    // Ölçek animasyonu YALNIZCA sıfırdan girişte uygulanır. Fade-out'tan geri
    // dönüşte kart hâlihazırda tam boyuttadır; onu yeniden küçültüp büyütmek
    // gözle görülür bir sıçrama yaratır (spec §3.6 "baştan başlatılmaz").
    if (from <= 0.001f && m_scale) {
        ComPtr<IDCompositionAnimation> scale;
        if (SUCCEEDED(m_dcompDevice->CreateAnimation(&scale)) &&
            SUCCEEDED(AddEaseOut(scale.Get(), kEnterScaleFrom, 1.0f, kEnterDurationSec))) {
            // Aynı animasyon nesnesi iki eksende paylaşılabilir; kart kare
            // kaldığı için X ve Y eğrileri özdeştir.
            HR_LOG(m_scale->SetScaleX(scale.Get()));
            HR_LOG(m_scale->SetScaleY(scale.Get()));
        }
    }

    HR_LOG(m_dcompDevice->Commit());
    m_phaseStartTick = ::GetTickCount64();
}

void OsdWindow::ApplyExitAnimation() {
    if (!m_deviceChainValid || !m_dcompDevice || !m_effectGroup) {
        return;
    }

    ComPtr<IDCompositionAnimation> opacity;
    if (FAILED(m_dcompDevice->CreateAnimation(&opacity))) {
        SetStaticOpacity(0.0f);
        return;
    }
    if (FAILED(AddEaseInTo0(opacity.Get(), kExitDurationSec))) {
        SetStaticOpacity(0.0f);
        return;
    }
    HR_LOG(m_effectGroup->SetOpacity(opacity.Get()));
    HR_LOG(m_dcompDevice->Commit());

    // EstimateCurrentOpacity bu damgadan geriye doğru hesaplar; çıkış
    // animasyonunun başlangıcını kaydetmek yeniden tetikleme için şarttır.
    m_phaseStartTick = ::GetTickCount64();
}

void OsdWindow::SetStaticOpacity(float value) {
    if (!m_effectGroup || !m_dcompDevice) {
        return;
    }
    // Ölçek de sabitlenir: kalan bir ölçek animasyonu bir sonraki gösterimde
    // kartın yanlış boyutta belirmesine yol açar.
    if (m_scale) {
        HR_LOG(m_scale->SetScaleX(1.0f));
        HR_LOG(m_scale->SetScaleY(1.0f));
    }
    HR_LOG(m_effectGroup->SetOpacity(Clamp(value, 0.0f, 1.0f)));
    HR_LOG(m_dcompDevice->Commit());
}

float OsdWindow::EstimateCurrentOpacity() const {
    // DComp animasyonun anlık değerini okumaya izin vermez; çıkış eğrisi
    // bilindiği için geçen süreden aynı değer türetilir.
    if (m_phase != Phase::Exiting) {
        return 1.0f;
    }
    const ULONGLONG now = ::GetTickCount64();
    const ULONGLONG elapsedMs = now >= m_phaseStartTick ? now - m_phaseStartTick : 0ull;
    const double u = Clamp(static_cast<double>(elapsedMs) / 1000.0 / kExitDurationSec,
                           0.0, 1.0);
    return static_cast<float>(1.0 - u * u * u);
}

}  // namespace kli
