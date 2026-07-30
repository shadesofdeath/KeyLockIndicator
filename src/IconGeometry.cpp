// IconGeometry.cpp — altı ikon geometrisinin (3 tuş x 2 durum) önbelleği.
//
// Geometriler cihazdan bağımsızdır: ID2D1Factory1 yaşadığı sürece geçerlidir,
// D2DERR_RECREATE_TARGET sonrası yeniden üretilmeleri gerekmez. Bu yüzden
// yalnızca bir kez, OsdRenderer'ın cihazdan bağımsız kaynak kurulumunda
// oluşturulur.
#include "IconGeometry.h"

#include "IconShapes.h"

namespace kli {
namespace {

// Önbellek yerleşimi: key * 2 + (on ? 1 : 0)
constexpr size_t kIconSlots = kLockKeyCount * 2;

[[nodiscard]] constexpr size_t IconIndex(LockKey key, bool on) noexcept {
    return static_cast<size_t>(key) * 2u + (on ? 1u : 0u);
}

using Builder = HRESULT (*)(ID2D1Factory1*, bool, IconShapes::Built&);

struct Entry {
    LockKey key;
    Builder build;
};

// Üretimin tamamı; herhangi bir adım başarısızsa çağıran Reset() yapar. Ayrı
// bir fonksiyon olması HR() makrosunun erken çıkışını kullanabilmek içindir
// (başlık sabit olduğu için özel üye fonksiyon eklenemez).
HRESULT BuildAll(ID2D1Factory1* factory, IconGeometry::Icon* icons) {
    const Entry kEntries[] = {
        {LockKey::Caps, &IconShapes::BuildCaps},
        {LockKey::Num, &IconShapes::BuildNum},
        {LockKey::Scroll, &IconShapes::BuildScroll},
    };
    // Tablo tuşları elle sıralar: başlığa yeni bir LockKey eklenirse buraya da
    // satır gerekir. Yoksa o slot hiç yazılmaz, Get() boş kaydı döndürür ve ikon
    // hatasız biçimde görünmez olur — bunu derleme zamanında yakalıyoruz.
    static_assert(_countof(kEntries) * 2 == kIconSlots,
                  "IconGeometry üretici tablosu LockKey ile senkron değil");

    for (const Entry& entry : kEntries) {
        for (int pass = 0; pass < 2; ++pass) {
            const bool on = (pass == 1);

            IconShapes::Built built;
            HR(entry.build(factory, on, built));
            if (!built.fill && !built.stroke) {
                // Boş geometri sessizce görünmez bir ikona dönüşürdü; bunu
                // hata sayıp OSD'nin placeholder çizmesine izin vermiyoruz.
                return E_FAIL;
            }

            IconGeometry::Icon& icon = icons[IconIndex(entry.key, on)];
            icon.fill = built.fill;
            icon.stroke = built.stroke;
            icon.strokeWidth = built.strokeWidth;
        }
    }
    return S_OK;
}

}  // namespace

HRESULT IconGeometry::Initialize(ID2D1Factory1* factory) {
    Reset();
    if (!factory) {
        return E_INVALIDARG;
    }

    const HRESULT hr = BuildAll(factory, m_icons);
    if (FAILED(hr)) {
        Reset();  // yarım kalmış önbellek bırakma
        return hr;
    }

    m_ready = true;
    return S_OK;
}

void IconGeometry::Reset() {
    for (Icon& icon : m_icons) {
        icon.fill.Reset();
        icon.stroke.Reset();
        icon.strokeWidth = 2.0f;
    }
    m_ready = false;
}

const IconGeometry::Icon& IconGeometry::Get(LockKey key, bool on) const noexcept {
    if (!m_ready) {
        return m_empty;
    }
    const size_t index = IconIndex(key, on);
    if (index >= kIconSlots) {
        return m_empty;
    }
    // İkisi de boşsa çizilecek bir şey yok; çağıranın nullptr denetimi yapmak
    // zorunda kalmaması için boş kaydı döndürürüz.
    const Icon& icon = m_icons[index];
    if (!icon.fill && !icon.stroke) {
        return m_empty;
    }
    return icon;
}

}  // namespace kli
