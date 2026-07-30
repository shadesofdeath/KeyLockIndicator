# KeyLock Indicator

Windows için Caps Lock / Num Lock / Scroll Lock durumunu ekranın üstünde **kare bir OSD** ile
gösteren, tepside çalışan, Windows'un açık/koyu temasını canlı takip eden hafif bir yardımcı
uygulama. Saf Win32 + C++20; .NET, WinUI, Qt ya da herhangi bir paket yöneticisi bağımlılığı yok.

- Direct2D + DirectWrite ile GPU hızlandırmalı çizim, DirectComposition ile gerçek per-pixel alfa
- 180×180 DIP kare kart, 12 DIP köşe yarıçapı, D2D gölge efekti, cubic ease giriş/çıkış animasyonu
- İkonlar kod ile üretilen vektör geometrilerdir; "açık"/"kapalı" hâlleri **ayrı geometridir**
  (renk körü erişilebilirliği)
- Klavye hook'u YOK — 70 ms polling; harici klavye, ekran klavyesi ve RDP kaynaklı değişimleri de yakalar
- Tepsi ikonu, çok satırlı ipucu, bağlam menüsü, anında uygulanan ayarlar, TR/EN dil desteği
- Tek dosya, statik CRT, runtime kurulumu gerektirmez

> **Ekran görüntüsü** — `<!-- TODO: OSD ve ayarlar penceresi ekran görüntüsü -->`

## Gereksinimler

**Çalıştırmak için:** Windows 10 1809 (build 17763) veya üstü, x64.

**Derlemek için:** Visual Studio 2022 (MSVC v143) veya 2026 (v145), Windows SDK 10.0.22621+,
CMake 3.25+. Doğrulanan yapılandırma: VS 2026 Community 18.8.2, MSVC 14.51, SDK 10.0.26100,
CMake 4.3.1, Ninja.

## Derleme

```powershell
tools\build.ps1 -Config Release
```

Elle:

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

MSIX değişkeni için `-DKLI_PACKAGED=ON` ekleyin (autostart `windows.startupTask` üzerinden yürür).
Çıktı: `build/release/KeyLockIndicator.exe`.

Simgeler eksikse CMake yapılandırma sırasında `tools/make_icons.ps1`'i bir kez çalıştırır.

## Mimari

Bağımlılık yönü tek yönlüdür ve ihlal edilemez:

```
KeyMonitor ──┐
ThemeWatcher─┼──► App ──► TrayIcon
             │        └──► OsdWindow ──► OsdRenderer ──► IconGeometry
Settings ────┘
```

Modüller birbirini tanımaz; tüm bağlantı `App` içinde `std::function` callback'lerle kurulur.

| Dosya | Görev |
|---|---|
| `main.cpp` | WinMain, DPI, tek-instance mutex, COM, mesaj döngüsü |
| `App.*` | Koordinatör + gizli host penceresi, tüm callback bağlantıları |
| `KeyMonitor.*` | `WM_TIMER` tabanlı 70 ms polling, değişen tuş için olay |
| `ThemeWatcher.*` | `AppsUseLightTheme` + `RegNotifyChangeKeyValue` **ve** `WM_SETTINGCHANGE` |
| `OsdWindow.*` | Katmanlı DComp penceresi, yaşam döngüsü, yerleşim, yeniden tetikleme |
| `OsdDevice.cpp` | D3D11/D2D/DComp zinciri, yüzey, kare çizimi, cihaz kaybı kurtarma |
| `OsdAnimation.cpp` | `IDCompositionAnimation` kübik eğrileri, opaklık/ölçek |
| `OsdRenderer.*` | Kart, gölge, kenarlık, ikon, metin çizimi |
| `IconGeometry.*` / `IconShapes.*` | 3 tuş × 2 durum = 6 `ID2D1PathGeometry` |
| `TrayIcon.*` | `Shell_NotifyIcon`, bağlam menüsü, `TaskbarCreated` kurtarma |
| `Settings.*` | HKCU registry oku/yaz, aralık kırpma |
| `SettingsDialog.*` | WinDark ile temalanmış modeless ayar penceresi |
| `SettingsPaint.*` | Ayar penceresi zemini, Rufus tarzı bölüm başlığı çizgileri, `WM_CTLCOLOR*` renkleri |
| `Autostart.*` | Run anahtarı / `StartupTask` |
| `MonitorUtil.*` | DPI, çalışma alanı, aktif monitör, tam ekran tespiti |
| `OemCheck.*` | OEM OSD ajanı taraması + tek seferlik uyarı |
| `Localization.*` | `RT_STRING` bloklarını dil kimliğiyle doğrudan okur |
| `Theme.h/.cpp` | Palet tabloları |
| `Util.h` | `ComPtr`, `unique_*`, `HR()` makroları, DPI yardımcıları |

## Ayarlar

Kök: `HKCU\Software\ShadesOfDeath\KeyLockIndicator`. Değişiklikler anında uygulanır.

Pencere yerleşimi `GROUPBOX` kullanmaz (Win32 grup çerçevesi koyu temada parlak "etched"
dikdörtgen çizer). Rufus düzeni: bölüm başlığı + metnin sağından pencere kenarına uzayan
1 piksellik çizgi (`src/SettingsPaint.cpp`, `WM_ERASEBKGND`). Diyalog 300×328 DLU —
%150 ölçekte 772×1081 fiziksel piksel (önceki dört çerçeveli yerleşim 296×351 DLU idi).

| Değer | Tip | Varsayılan | Aralık |
|---|---|---|---|
| `ShowOsd` | DWORD | 1 | 0/1 |
| `WatchCaps` | DWORD | 1 | 0/1 |
| `WatchNum` | DWORD | 1 | 0/1 |
| `WatchScroll` | DWORD | 0 | 0/1 |
| `OsdDurationMs` | DWORD | 1400 | 800–4000 |
| `OsdOpacity` | DWORD | 82 | 60–100 |
| `OsdPosition` | DWORD | 0 | 0=Üst, 1=Orta, 2=Alt |
| `OsdTopMarginDip` | DWORD | 72 | 0–400 |
| `ThemeMode` | DWORD | 0 | 0=Sistem, 1=Açık, 2=Koyu |
| `SuppressFullscreen` | DWORD | 1 | 0/1 |
| `PrimaryMonitorOnly` | DWORD | 0 | 0/1 |
| `TrayIconKey` | DWORD | 0 | 0=Caps, 1=Num, 2=Scroll |
| `Language` | SZ | `auto` | `auto`/`tr`/`en` |

## Şartnameden sapmalar

Her biri gerekçesiyle. Ölçümler bu makinede yapıldı (Intel iGPU, 2560×1440 @ %150, x64 Release).

1. **`external/WinDark` bir git submodule değil.** Şartname §1 bunu submodule olarak listeler;
   belirtilen kütüphane hiçbir genel kaynakta (GitHub dahil) bulunamadı ve yerel diskte de yok.
   Aynı rolü gören bağımlılıksız bir uygulama aynı dizine vendor edildi: yalnızca Win32 + uxtheme
   + dwmapi kullanır, belgelenmemiş uxtheme ordinal'lerini "çözülemedi → sessizce no-op" ilkesiyle
   çağırır. Gerçek submodule bulunursa **yalnızca `external/WinDark` değişir**, `src/` dokunulmaz.
   Bu yüzden `.gitmodules` yok.

2. **`LockKey` / `LockState`, `src/LockTypes.h` yaprak başlığına taşındı.** Şartname §4.1 bunları
   `KeyMonitor.h` içinde gösterir, ama §2'deki "OsdWindow, KeyMonitor'ı tanımaz" kuralı ihlal
   edilemez. `KeyMonitor.h` bu başlığı include eder, dolayısıyla §4.1'deki bildirim yüzeyi korunur.

3. **`OsdPlacement` enum'u `OsdWindow.h` içinde ayrıca tanımlı.** `OsdWindow`, `Settings.h`'ye
   bağlanamaz (§2); `App` iki enum arasında çeviri yapar (sayısal değerler aynı).

4. **Link listesine üç kütüphane eklendi:** `comctl32` (§8 manifestinin zorunlu kıldığı Common
   Controls 6 API'leri: trackbar, updown, `TaskDialogIndirect`), `wtsapi32`
   (§6'daki `WTSRegisterSessionNotification`), `dxguid` (`CLSID_D2D1Shadow`, `d2d1effects.h`
   içinde `DEFINE_GUID` ile bildirildiği için tanımı bu kütüphanededir).

5. **Metin kenar yumuşatma `D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE`.** Saydam per-pixel alfa yüzeyde
   ClearType alt piksel filtresi çalışmaz ve §11'in yasakladığı siyah kenar artefaktını üretir.

6. **`ID2D1DeviceContext::SetDpi` açıkça çağrılır.** `SetTarget`, bağlamın DPI'sını hedef
   bitmap'ten devralmaz (bitmap DPI'sı yalnızca bitmap bir *kaynak* olduğunda geçerlidir).
   Çağrılmazsa bağlam 96 DPI'da kalır ve tüm DIP ölçüleri piksele 1:1 eşlenir — kart %150
   ölçekte 366 piksellik yüzeyde 270 değil 180 piksel çizilir.

7. **`IDCompositionSurface::EndDraw`, `Commit`'ten önce gelir.** DComp, yüzey çizimi hâlâ açıkken
   yapılan `Commit`'i `DCOMPOSITION_ERROR_SURFACE_BEING_RENDERED` (0x88980801) ile reddeder.

8. **`IDCompositionScaleTransform` ölçeği açıkça 1.0'a kurulur;** aksi hâlde görsel hiç çizilmez.

9. **Cihaz zinciri boşta kalınca bırakılır (§6'dan sapma).** §6 zinciri açılışta kurup hiç
   bırakmamayı, gerekçe olarak "ilk basımda ~200 ms gecikme" olur demeyi öngörür. Ölçülen:

   | Aşama | Çalışma seti | Özel bellek |
   |---|---|---|
   | D3D öncesi | 7,5 MB | 1,4 MB |
   | D3D11 cihazı | 33,0 MB | 34,3 MB |
   | Tüm zincir + kare | 51,7 MB | 48,5 MB |
   | Zincir bırakıldı | 28,6 MB | 14,6 MB |
   | + çalışma seti trim | **0,2 MB** | 14,6 MB |

   Maliyetin neredeyse tamamı sürücünün kendi yığınları (`igc-default64.dll` shader derleyicisi
   74 MB dosya, `igd10um64xe.dll` UMD 19 MB) — uygulamanın kendi tahsisleri önemsiz. Yeniden
   kurulum + ilk kare **18–22 ms** ölçüldü (sürücü DLL'leri yüklü kalır, shader önbelleği sıcak),
   yani §6'nın 200 ms gerekçesi geçerli değil ve §11'in "gizliyken < 8 MB" hedefi canlı bir D3D11
   cihazıyla hiçbir koşulda tutturulamaz. Bu yüzden OSD gizlendikten **5 saniye** sonra zincir
   bırakılır ve çalışma seti boşaltılır; hızlı ardışık basımlar zinciri paylaşır. En kötü hâlde
   gecikme ~22 ms, §11'in 100 ms bütçesinin çok altında. Süre `kOsdIdleTeardownMs`
   (`src/Messages.h`) ile ayarlanır.

10. **`< 500 KB` exe hedefi tutulmuyor: 588 KB.** Kod + statik CRT yalnızca **248 KB**; kalan
    **340 KB** ikon kaynağı (12 tepsi ikonu × ~24 KB). Her tepsi ikonu tepsinin hiç istemediği
    48 px ve 256 px karelerini taşıyor (tepsi en fazla `SM_CXSMICON` = %200 ölçekte 32 px ister).
    Bu iki kareyi 12 tepsi ikonundan çıkarmak ~177 KB kazandırır → ~411 KB, hedefin altı.
    İkonlar kullanıcı tarafından sağlandığı için bu kırpma **uygulanmadı**; istenirse tek adımdır.

11. **Derleme MSVC v145 (VS 2026) ile doğrulandı;** şartname v143 (VS 2022) der. Kaynakta sürüme
    özgü hiçbir şey yok, ikisi de `/std:c++20` ile uyumludur.

## Bilinen sınırlar

Şartname §12 uyarınca belgelenir, çözülmeye çalışılmaz:

- UAC istemi ve Güvenli Masaüstü üzerinde OSD görünmez (`uiAccess=false`). `uiAccess="true"`
  imzalı sertifika ve Program Files kurulumu gerektirir, Store dağıtımıyla uyumsuzdur.
- Bazı exclusive fullscreen DirectX oyunlarında OSD görünmeyebilir.
- Sanal makine konsollarında (VMware/VirtualBox misafir penceresi) tuş durumu ana makineninkinden
  sapabilir.

## Kabul kriterleri

`[x]` = bu makinede doğrulandı, `[~]` = kısmen/elle doğrulanmalı, `[ ]` = doğrulanmadı.

**İşlevsel**
- [x] Caps/Num/Scroll değişiminde OSD belirir (ölçülen gecikme ~20 ms + polling payı)
- [~] Harici klavye / ekran klavyesi / RDP — polling tasarımı gereği yakalar, elle test edilmeli
- [x] Açılışta OSD gösterilmez
- [~] Uyku/hazırda bekleme sonrası sessiz senkron (kod yolu var, elle test edilmeli)
- [x] Hızlı ardışık basımlarda tek pencere, birikme yok
- [~] Explorer yeniden başlatıldığında tepsi ikonu geri gelir (`TaskbarCreated` işlenir)

**Görsel**
- [x] Kart tam kare, kenarlar keskin, köşeler pürüzsüz
- [x] %150 ölçeklemede metin ve ikon net (kart 270 px, doğru)
- [ ] 100/125/175/200 % ölçekleme — elle test edilmeli
- [ ] Farklı DPI'daki iki monitör — bu makinede tek monitör var
- [x] Beyaz ve koyu zemin üzerinde okunabilirlik korunur
- [x] Fade'de siyah kenar artefaktı yok (grayscale AA + premultiplied alfa)
- [~] Tema değişimi OSD görünürken anında geçer (kod yolu var)

**Davranışsal**
- [x] OSD odak çalmaz (`WS_EX_NOACTIVATE`, `SWP_NOACTIVATE`, `ShowWindow(SW_SHOW)` kullanılmaz)
- [x] Tıklama altındaki pencereye gider (`WS_EX_TRANSPARENT` + `HTTRANSPARENT`)
- [x] Tam ekran/sunum modunda OSD bastırılır (`SHQueryUserNotificationState`)
- [x] Ayarlar penceresi Windows temasına uygun açılır

**Performans**
- [ ] 1 saatlik boşta CPU < %0,1 — uzun süreli ölçüm yapılmadı
- [~] Çalışma seti: gizliyken **0,2 MB** (hedef < 8 MB ✓), görünürken **61 MB**
      (hedef < 25 MB ✗ — sapma 9'daki sürücü maliyeti nedeniyle)
- [ ] 24 saat sızıntı testi yapılmadı

## Lisans

MIT. Telif © ShadesOfDeath.
