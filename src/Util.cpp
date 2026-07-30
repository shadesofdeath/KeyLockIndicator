// Util.cpp — Util.h'de yalnızca bildirilen (satır içi olmayan) yardımcılar.
//
// Util.h bir yaprak başlıktır ve içeriğinin neredeyse tamamı inline'dır; burada
// tek bir tanım vardır. Dosyanın var olma sebebi ModulePath'in yığın üzerinde
// büyüyen bir tampon istemesi: header'da inline tutulsa her çeviri birimine
// <vector> bulaştırırdı.
#include "Util.h"

#include <vector>

namespace kli {

std::wstring ModulePath() {
    // GetModuleFileNameW kesme durumunda tamponu doldurur, sonlandırır ve
    // ERROR_INSUFFICIENT_BUFFER kurar (Win10+). Bu yüzden dönüş değeriyle
    // yetinmek yetmez: her denemeden önce son hatayı temizleyip sonra bakıyoruz.
    // Uzun yol (\\?\ öneki) etkin bir sistemde yol MAX_PATH'i aşabilir.
    std::vector<wchar_t> buf(MAX_PATH);

    for (;;) {
        ::SetLastError(ERROR_SUCCESS);
        const DWORD copied =
            ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));

        if (copied == 0) {
            // Yol alınamadı — çağıranlar boş dizeyi "yol yok" olarak yorumlar.
            LogV(L"ModulePath: GetModuleFileNameW basarisiz (hata %lu)", ::GetLastError());
            return std::wstring();
        }

        if (copied < buf.size() && ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return std::wstring(buf.data(), copied);
        }

        // 32767 wchar, NT'nin yol üst sınırıdır; burada büyümeyi bırakıp
        // elimizdeki (kesilmiş) değeri döndürmek sonsuz döngüden iyidir.
        // Uzunluk `copied`den alınmaz: kesme durumunda dönüş değeri sonlandırıcıyı
        // da sayar, dolayısıyla dizenin sonuna gömülü bir L'\0' düşerdi.
        if (buf.size() >= 32768u) {
            LogV(L"ModulePath: yol ust sinira ulasti, kesilmis deger donuyor");
            return std::wstring(buf.data());
        }

        buf.resize(buf.size() * 2);
    }
}

}  // namespace kli
