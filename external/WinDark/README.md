# WinDark (vendor edilmiş)

Win32 pencerelerine ve standart kontrollere Windows'un koyu temasını uygulayan
küçük bir statik kütüphane. KeyLock Indicator'da yalnızca **ayarlar penceresi**
kullanır; OSD kendi renklerini Direct2D ile çizer ve buraya bağımlı değildir.

## Neden burada?

`KEYLOCK_INDICATOR_SPEC.md` §1 ve §2, tema/UI katmanı için `external/WinDark`
altında bir **git submodule** öngörür. Bu ada karşılık gelen genel bir depo
bulunamadığı için aynı görevi gören, bağımlılığı olmayan yerel bir uygulama
buraya vendor edilmiştir: aynı dizin konumu, aynı rol, aynı çağrı yüzeyi.
Bağımlılık spec'in link satırını aşmaz — yalnızca `uxtheme`, `dwmapi`, `gdi32`,
`user32`, `advapi32`. vcpkg/conan paketi yoktur.

## API

`include/WinDark/WinDark.h` — tamamı `WinDark` namespace'i içinde:

| Fonksiyon | Görev |
|---|---|
| `Initialize()` | Ordinal'leri bir kez çözer (diğer çağrılar kendiliğinden çağırır) |
| `IsSupported()` | Derleme ≥ 17763 ve ordinal'ler çözüldü mü |
| `IsSystemDarkMode()` | `HKCU\...\Themes\Personalize\AppsUseLightTheme` okur |
| `SetAppMode(AppMode)` | Uygulama genelinde tercih edilen mod |
| `ApplyToWindow(HWND, bool)` | Başlık çubuğu (DWM) + pencere teması |
| `ApplyToChildren(HWND, bool)` | Alt kontroller, sınıflarına göre, özyinelemeli |
| `RefreshWindow(HWND)` / `RefreshImmersiveColorPolicyState()` | Tema sonrası tazeleme |
| `OnCtlColor(...)` | `WM_CTLCOLOR*` için tek giriş noktası, `HBRUSH` döner |
| `ReleaseCachedObjects()` | Tembel oluşturulan fırçaları siler (çıkışta) |
| `TextColor` / `DisabledTextColor` / `BackgroundColor` / `ControlBackgroundColor` | Palet |

## Kaynak sahipliği

`OnCtlColor`, koyu tema için en fazla iki `HBRUSH` üretip süreç boyunca
önbellekte tutar (renkler sabit olduğu için önbellek geçersizleşmez; açık temada
hiç fırça yaratılmaz). Bu hedef `src/` altını görmez — `Util.h`'deki
`unique_hbrush` / `HR()` yardımcıları burada kullanılamaz, sahiplik elle
yönetilir. Bu yüzden uygulama çıkışında **bir kez** `ReleaseCachedObjects()`
çağrılmalıdır (spec §11 "GDI nesnesi sızıntısı yok").

## Gerçek submodule ile değiştirme

1. `external/WinDark` dizinini silin, yerine `git submodule add <url> external/WinDark`.
2. Kök `CMakeLists.txt` zaten `add_subdirectory(external/WinDark)` çağırır ve
   `WinDark` hedefine link eder — dokunmanız gerekmez.
3. Çağrı yüzeyi farklıysa **yalnızca** bu dizine bir uyum başlığı ekleyin;
   `src/` dokunulmaz. Vendor kararının tek amacı budur.

## Desteklenen Windows aralığı

Windows 10 **1809 (derleme 17763)** ve sonrası. Üst sınır yoktur. Daha eski
sürümlerde tüm çağrılar **sessizce no-op** olur; uygulama çalışır, pencere açık
temada kalır. Derleme numarası `ntdll!RtlGetNtVersionNumbers` ile okunur —
`GetVersionEx` manifest uyumluluk katmanına takılıp eşiği yanlış tarafa düşürür.

## Undocumented ordinal'ler ve risk

Koyu tema anahtarları `uxtheme.dll` içinde **isimsiz**, yalnızca ordinal ile
ihraç edilir. Kullanılanlar:

| Ordinal | Fonksiyon | Not |
|---|---|---|
| 104 | `RefreshImmersiveColorPolicyState()` | Mod değişiminden sonra politikayı tazeler |
| 133 | `AllowDarkModeForWindow(HWND, BOOL)` | Pencere başına izin |
| 135 | `AllowDarkModeForApp(BOOL)` (< 18362) / `SetPreferredAppMode(int)` (≥ 18362) | Aynı ordinal, imza değişti |

`ShouldAppsUseDarkMode()` (132) **hiç kullanılmaz**: sistem teması kayıt
defterinden okunur (`IsSystemDarkMode`). Böylece yalnızca o ordinal kaybolsa
bile `IsSupported()` `false`'a düşmez — destek ölçüsü, gerçekten çağrılan
fonksiyonlardır.

**Risk:** Microsoft bu ordinal'leri kaydırabilir veya kaldırabilir. O durumda
`GetProcAddress` `nullptr` döner, `IsSupported()` `false` olur ve kütüphane
tamamen sessizleşir — çökme veya bozuk çizim değil, yalnızca açık tema.
`DWMWA_USE_IMMERSIVE_DARK_MODE` numarası da sürümle değişti (< 18985'te 19,
sonra 20); iki değer de yazılır, geçersiz olan hata döndürüp yok sayılır.
