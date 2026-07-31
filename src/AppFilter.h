// AppFilter.h — Ön plandaki uygulamanın exe adı + istisna listesi eşleşmesi
// (madde 18). Bağımlılığı olmayan yaprak modül; Settings'i TANIMAZ, listeyi
// çağıran verir (spec §2).
#pragma once

#include <string>
#include <vector>

namespace kli {
namespace AppFilter {

// Ön plandaki pencerenin sahibi olan sürecin exe ADI ("notepad.exe").
// Ön planda pencere yoksa, süreç açılamıyorsa (yükseltilmiş/koruma altındaki
// bir uygulama) ya da yol okunamıyorsa BOŞ dize döner — boş ad hiçbir kurala
// uymaz, yani belirsizlikte OSD gösterilir. Sessizce gizlemek, kullanıcının
// "neden çalışmıyor" diye aradığı bir hata olurdu.
[[nodiscard]] std::wstring ForegroundExeName();

// exeName listede var mı (büyük/küçük harf DUYARSIZ). Boş ad ya da boş liste
// için daima false.
[[nodiscard]] bool Matches(const std::vector<std::wstring>& excluded,
                           const std::wstring& exeName);

// Kısayol: ön plandaki uygulama listedeyse true. Liste boşsa hiçbir sistem
// çağrısı yapılmaz — özelliği kullanmayan kullanıcı bedel ödemez.
[[nodiscard]] bool ForegroundExcluded(const std::vector<std::wstring>& excluded);

}  // namespace AppFilter
}  // namespace kli
