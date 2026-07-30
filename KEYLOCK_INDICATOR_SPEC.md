# KeyLock Indicator — Yapay Zeka Geliştirme Spesifikasyonu

> Bu belge bir yapay zeka ajanının sıfırdan uygulanabilir bir Windows masaüstü uygulaması üretmesi için yazılmıştır. Belirsiz bırakılan hiçbir karar yoktur; "kendi tercihini yap" denen yerler açıkça işaretlenmiştir.

---

## 0. Özet

Windows için Caps Lock / Num Lock / Scroll Lock durumunu ekranın üst kısmında **kare bir OSD (on-screen display)** ile gösteren, tray'de çalışan, Windows'un açık/koyu temasını canlı takip eden hafif bir yardımcı uygulama.

**Hedefler:**
- Tek dosya, bağımlılıksız `.exe` (< 500 KB), runtime kurulumu gerektirmez
- Boşta CPU %0.0–0.1, RAM < 8 MB
- OSD görsel kalitesi Windows 11 sistem OSD'si (ses/parlaklık) seviyesinde olmalı — bulanık kenar, titrek animasyon, keskin olmayan metin kabul edilemez

---

## 1. Teknoloji Yığını — Bunlar tartışmaya kapalıdır

| Alan | Seçim | Gerekçe |
|---|---|---|
| Dil | **C++20**, saf Win32 | Runtime bağımlılığı yok, en düşük bellek ayak izi |
| Derleyici | MSVC v143 (VS 2022) | `/std:c++20 /permissive- /W4 /MT` |
| Build | **CMake 3.25+** | `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded` (statik CRT) |
| Çizim | **Direct2D + DirectWrite** | GPU hızlandırmalı, ClearType/grayscale AA |
| Kompozisyon | **DirectComposition** | Gerçek per-pixel alpha + GPU üzerinde akıcı fade |
| Tema/UI | **WinDark** (git submodule) | Ayarlar penceresi için mevcut kütüphane |
| Paketleme | MSIX (Store) + portable ZIP | İkisi de aynı koddan üretilir |

**Kullanılmayacaklar:** .NET, WinUI 3, WPF, Qt, Electron, herhangi bir vcpkg/conan paketi, `WH_KEYBOARD_LL` hook.

Link edilecek kütüphaneler: `d2d1.lib dwrite.lib dcomp.lib dxgi.lib d3d11.lib shell32.lib user32.lib gdi32.lib advapi32.lib uxtheme.lib dwmapi.lib shcore.lib`

---

## 2. Proje Yapısı

```
keylock-indicator/
├── CMakeLists.txt
├── .gitmodules
├── README.md
├── external/
│   └── WinDark/                    # git submodule
├── src/
│   ├── main.cpp                    # WinMain, tek-instance, mesaj döngüsü
│   ├── App.h / App.cpp             # Uygulama koordinatörü + gizli mesaj penceresi
│   ├── KeyMonitor.h / .cpp         # Tuş durumu polling + değişim olayı
│   ├── ThemeWatcher.h / .cpp       # Açık/koyu tema + accent color takibi
│   ├── OsdWindow.h / .cpp          # DComp katmanlı pencere, yaşam döngüsü
│   ├── OsdRenderer.h / .cpp        # Direct2D çizim (kare kart + ikon + metin)
│   ├── IconGeometry.h / .cpp       # Vektör ikon path'leri (D2D geometry)
│   ├── TrayIcon.h / .cpp           # Shell_NotifyIcon + bağlam menüsü
│   ├── Settings.h / .cpp           # HKCU registry oku/yaz
│   ├── SettingsDialog.h / .cpp     # WinDark'lı ayar penceresi
│   ├── Autostart.h / .cpp          # Run key / StartupTask
│   ├── MonitorUtil.h / .cpp        # DPI, çalışma alanı, aktif monitör
│   └── Util.h                      # ComPtr benzeri RAII, HR kontrolü, log
├── res/
│   ├── app.rc
│   ├── app.manifest
│   ├── resource.h
│   └── icons/                      # tray için .ico (16/20/24/32/48/256)
└── packaging/
    ├── AppxManifest.xml
    └── Assets/
```

**Bağımlılık yönü tek yönlüdür ve ihlal edilemez:**

```
KeyMonitor ──┐
ThemeWatcher─┼──► App ──► TrayIcon
             │        └──► OsdWindow ──► OsdRenderer ──► IconGeometry
Settings ────┘
```

`OsdWindow`, `KeyMonitor`'ı tanımaz. `KeyMonitor`, `Settings`'i tanımaz. Tüm bağlantı `App` içinde `std::function` callback'lerle kurulur.

---

## 3. OSD Görsel Tasarımı — En kritik bölüm

### 3.1 Ölçüler (tamamı DIP = 96 DPI birimi, render sırasında DPI ile çarpılır)

```
Kart boyutu:        180 × 180 DIP  (KARE)
Köşe yarıçapı:      12 DIP
Kenarlık kalınlığı: 1 DIP  (kartın iç kenarında, inset 0.5)
Üstten boşluk:      ekran çalışma alanının üstünden 72 DIP
Yatay konum:        aktif monitörün çalışma alanında ortalanmış
```

### 3.2 Kart içi yerleşim (180×180 kutu içinde)

```
┌─────────────────────────────┐  ← y=0
│                             │
│         (boşluk 34)         │
│                             │
│      ┌───────────┐          │  ← y=34, ikon kutusu 72×72, yatay ortalı
│      │   İKON    │          │
│      └───────────┘          │  ← y=106
│                             │
│         (boşluk 14)         │
│                             │
│        CAPS LOCK            │  ← y=120, baseline hizalı, 15 DIP semibold
│           Açık              │  ← y=144, 13 DIP regular, %60 opaklık
│                             │
└─────────────────────────────┘  ← y=180
```

- Başlık metni: `CAPS LOCK` / `NUM LOCK` / `SCROLL LOCK` — büyük harf, harf aralığı +0.5 DIP
- Durum metni: `Açık` / `Kapalı` (dil kaynağından)
- Metinler `DWRITE_TEXT_ALIGNMENT_CENTER`, `DWRITE_WORD_WRAPPING_NO_WRAP`

### 3.3 Renkler

Sistem accent rengi **kullanılmaz** — nötr kalır, sadece "açık" durumdaki ikon dolgusu tema vurgusunu alır.

**Koyu tema:**
| Öğe | Renk | Alfa |
|---|---|---|
| Kart zemini | `#1F1F1F` | **0.82** |
| Kenarlık | `#FFFFFF` | 0.09 |
| İkon — açık | `#FFFFFF` | 1.00 |
| İkon — kapalı | `#FFFFFF` | 0.38 |
| Başlık metni | `#FFFFFF` | 0.95 |
| Durum metni | `#FFFFFF` | 0.58 |

**Açık tema:**
| Öğe | Renk | Alfa |
|---|---|---|
| Kart zemini | `#F7F7F7` | **0.85** |
| Kenarlık | `#000000` | 0.08 |
| İkon — açık | `#1A1A1A` | 1.00 |
| İkon — kapalı | `#1A1A1A` | 0.32 |
| Başlık metni | `#000000` | 0.90 |
| Durum metni | `#000000` | 0.55 |

Kart zemin alfası ayarlardan **0.60 – 1.00** arası değiştirilebilir; varsayılan yukarıdaki değerlerdir. "Az şeffaf" hedefi budur: arkası hafifçe seçilir ama metin okunabilirliği bozulmaz.

> **Önemli:** Alfa değerleri `UpdateLayeredWindow` benzeri global alfa DEĞİL, D2D fırçalarının kendi opaklığıdır. Yani metin, kart zeminine göre opaktır; kart bütünüyle saydamlaşmaz. Bu, ucuz görünen "tüm pencereyi %80 yap" yaklaşımından kaçınmak için şarttır.

### 3.4 Gölge

DirectComposition ile ayrı bir gölge görseli çizmek yerine, D2D üzerinde kartın altına yumuşak gölge çizilir:

- `ID2D1Effect` (`CLSID_D2D1Shadow`) ile: `BlurStandardDeviation = 9.0`, renk `#000000` alfa 0.28 (koyu tema) / 0.18 (açık tema)
- Gölge offset: Y ekseninde +6 DIP
- Bu yüzden **pencere yüzeyi karttan büyüktür**: her yönde 32 DIP payı bırak → yüzey `244 × 244 DIP`. Kart bu yüzeyin merkezine çizilir.

### 3.5 İkonlar

Bitmap kullanılmaz. `IconGeometry.cpp` içinde her ikon `ID2D1PathGeometry` olarak **kod ile** üretilir ve önbelleğe alınır. 72×72 DIP kutuya normalize edilmiş koordinatlarla çiz. Segoe Fluent Icons fontuna bağımlı olma — kullanıcıda olmayabilir.

**Caps Lock:** yukarı bakan içi boş ok (aşağıda düz taban çizgisi). Açıkken taban çizgisi dolu ve okun içi doludur; kapalıyken sadece 2 DIP kalınlıkta dış hat.

**Num Lock:** yuvarlatılmış kare çerçeve içinde "1 2 3" yerine tek bir `#` benzeri sayısal tuş takımı sembolü — 3×3 nokta ızgarası, ortada büyük nokta. Açıkken tüm noktalar dolu, kapalıyken sadece dış çerçeve.

**Scroll Lock:** dikey çift yönlü ok + yatay çizgi (kilit sembolü ile birleşik). Kapalıyken açık asma kilit, açıkken kapalı asma kilit kancası.

Her ikonun "açık" ve "kapalı" hâli **ayrı geometri** olarak tanımlanır, sadece renk değişimiyle yetinilmez — bu, renk körü kullanıcılar için erişilebilirlik gereğidir.

### 3.6 Animasyon

DirectComposition visual üzerinde `IDCompositionAnimation` ile:

| Faz | Süre | Eğri |
|---|---|---|
| Giriş — opaklık 0→1 | 130 ms | cubic ease-out |
| Giriş — scale 0.94→1.0 | 130 ms | cubic ease-out, merkez orijinli |
| Bekleme | **1400 ms** (ayarlanabilir 800–4000) | — |
| Çıkış — opaklık 1→0 | 200 ms | cubic ease-in |

OSD görünürken yeni bir tuş olayı gelirse: animasyon **baştan başlatılmaz**; içerik anında güncellenir ve bekleme sayacı sıfırlanır. Fade-out başlamışsa opaklık mevcut değerinden 1'e geri çıkar.

---

## 4. Modül Sözleşmeleri

### 4.1 `KeyMonitor`

```cpp
enum class LockKey { Caps, Num, Scroll };

struct LockState {
    bool caps = false, num = false, scroll = false;
    bool Get(LockKey k) const;
};

class KeyMonitor {
public:
    using ChangeCallback = std::function<void(LockKey changed, LockState now)>;
    void Start(HWND hostWnd, ChangeCallback cb);   // WM_TIMER tabanlı
    void Stop();
    LockState Current() const;
};
```

**Uygulama kuralları:**
- `SetTimer(hostWnd, TIMER_POLL, 70, nullptr)` ile 70 ms aralık
- Her tick'te `GetKeyState(VK_CAPITAL) & 0x0001` şeklinde **düşük bit** okunur (yüksek bit "basılı" demektir, "açık" demek değildir)
- Sadece önceki tick'e göre **değişen** tuş için callback tetiklenir
- Uygulama ilk açıldığında mevcut durum okunur ama **callback tetiklenmez** (açılışta OSD çıkmasın)
- Birden fazla tuş aynı tick'te değişirse her biri için ayrı callback — `App` bunları sıraya alıp son geleni gösterir
- `WM_WTSSESSION_CHANGE` / `WM_POWERBROADCAST` (resume) sonrası durum yeniden okunur ve sessizce senkronlanır

**Neden hook değil:** low-level klavye hook'u AV yazılımlarında ve Store sertifikasyonunda gereksiz risk yaratır, harici klavye/KVM/RDP kaynaklı durum değişimlerini kaçırır ve UIPI nedeniyle yükseltilmiş pencerelerde çalışmaz. Polling'in ölçülen maliyeti tick başına < 2 µs'dir.

### 4.2 `ThemeWatcher`

```cpp
enum class AppTheme { Light, Dark };

class ThemeWatcher {
public:
    void Start(HWND hostWnd, std::function<void(AppTheme)> cb);
    AppTheme Current() const;
};
```

- Kaynak: `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize` → `AppsUseLightTheme` (DWORD, 0 = koyu)
- Değişim tespiti: `RegNotifyChangeKeyValue` ile ayrı thread **veya** `hostWnd`'de `WM_SETTINGCHANGE` + `lParam == "ImmersiveColorSet"` yakalanır. **İkisi birden uygulanır** — `WM_SETTINGCHANGE` bazı Windows sürümlerinde tetiklenmez.
- Tema değiştiğinde: D2D fırçaları yeniden oluşturulur, OSD o an görünürse anında yeni renklerle yeniden çizilir (geçiş animasyonu yok), tray ikonu yeniden yüklenir, ayarlar penceresi açıksa WinDark'a bildirilir.
- Ayarlarda `ThemeMode` = `System` (varsayılan) / `Light` / `Dark` seçeneği; `System` dışında watcher yok sayılır.

### 4.3 `OsdWindow`

```cpp
class OsdWindow {
public:
    bool Create();
    void Show(LockKey key, bool isOn);   // yeniden tetiklenebilir
    void HideImmediate();
    void OnThemeChanged(AppTheme t);
    void OnDpiOrMonitorChanged();
};
```

**Pencere stilleri (tam olarak bunlar):**
```cpp
exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW
        | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;
style   = WS_POPUP;
```

**Kompozisyon zinciri:**
```
D3D11CreateDevice (BGRA_SUPPORT, hardware → WARP fallback)
  → IDXGIDevice
    → D2D1CreateDevice → ID2D1DeviceContext
    → DCompositionCreateDevice → IDCompositionDevice
       → CreateTargetForHwnd(hwnd, topmost=TRUE)
       → CreateVisual → SetContent(IDCompositionSurface)
       → root->AddVisual(visual)
```

Yüzey `DXGI_FORMAT_B8G8R8A8_UNORM`, alfa modu `D2D1_ALPHA_MODE_PREMULTIPLIED`. Çizimden önce `context->Clear(D2D1::ColorF(0,0,0,0))`.

`WM_NCHITTEST` → `HTTRANSPARENT` döndür (ek güvence). Pencere hiçbir koşulda odak almaz; `Show` içinde `SetWindowPos(..., SWP_NOACTIVATE | SWP_SHOWWINDOW)` kullanılır, `ShowWindow(SW_SHOW)` değil.

**Cihaz kaybı:** `D2DERR_RECREATE_TARGET` veya `DXGI_ERROR_DEVICE_REMOVED` alındığında tüm zincir yıkılıp yeniden kurulur ve çizim bir kez tekrar denenir.

### 4.4 `MonitorUtil`

- OSD, **imlecin bulunduğu monitörde** gösterilir (`GetCursorPos` → `MonitorFromPoint`). Ayarlarda "her zaman ana monitör" seçeneği bulunur.
- `MONITORINFO.rcWork` kullanılır (görev çubuğu üstte olabilir)
- DPI: `GetDpiForMonitor(..., MDT_EFFECTIVE_DPI, ...)`. Manifest'te **Per-Monitor DPI Aware V2** zorunlu.
- Tam ekran tespiti: `SHQueryUserNotificationState()` → `QUNS_BUSY`, `QUNS_RUNNING_D3D_FULL_SCREEN` veya `QUNS_PRESENTATION_MODE` ise ayara göre OSD bastırılır (varsayılan: **bastır**, oyun içinde rahatsız etmesin).

### 4.5 `TrayIcon`

- `Shell_NotifyIcon` + `NIF_ICON | NIF_TIP | NIF_MESSAGE`, `uVersion = NOTIFYICON_VERSION_4`
- İkon, aktif tuş durumlarını yansıtır (örn. Caps açıksa dolu ikon). Hangi tuşun tray'de gösterileceği ayarlanabilir; varsayılan Caps Lock.
- Tooltip: çok satırlı, üç tuşun da durumu
- `RegisterWindowMessage(L"TaskbarCreated")` dinlenir → Explorer çöktüğünde ikon yeniden eklenir
- Sağ tık menüsü: Ayarlar / Windows ile başlat (onay işaretli) / OSD'yi geçici kapat / Hakkında / Çıkış
- Sol tık: ilgili tuşu **değiştirmez** (yanlışlıkla tetiklenme riski), sadece anlık OSD gösterir
- Menü `TrackPopupMenu` öncesi `SetForegroundWindow(hostWnd)` çağrılır, sonrası `PostMessage(hostWnd, WM_NULL, 0, 0)` — klasik menü kapanmama hatası için

### 4.6 `Settings`

Kök: `HKCU\Software\ShadesOfDeath\KeyLockIndicator`

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
| `TrayIconKey` | DWORD | 0 | 0=Caps,1=Num,2=Scroll |
| `Language` | SZ | `auto` | `auto`/`tr`/`en` |

Değişiklikler anında uygulanır, yeniden başlatma gerekmez.

### 4.7 `Autostart`

- **Portable derleme:** `HKCU\...\CurrentVersion\Run` altında `KeyLockIndicator` = `"<exe yolu>"`
- **MSIX derleme:** `AppxManifest.xml` içinde `windows.startupTask` uzantısı; kod tarafında `StartupTask` WinRT API'si kullanılır
- Hangi yolun aktif olduğu derleme zamanı `#ifdef KLI_PACKAGED` ile ayrılır; çalışma zamanında `GetCurrentPackageFullName()` ile de doğrulanır

---

## 5. OEM Çakışma Kontrolü

ASUS, Lenovo, HP gibi üreticiler kendi OSD ajanlarını (çoğu tray'siz) önyüklü gönderir. İkisi aynı anda çıkarsa kullanıcı uygulamayı bozuk sanır.

İlk çalıştırmada aşağıdaki proses adları taranır (`CreateToolhelp32Snapshot`), büyük/küçük harf duyarsız, kısmi eşleşme:

```
ATKOSD2, ATKOSD, HControlUser, ASUSOSD          → ASUS
utility.exe, LenovoUtilityService, LenovoUtility → Lenovo
HPSysInfo, HPSystemEventUtility, HotKeyServiceUWP → HP
QuickAccess.exe                                  → Acer
QuickSet.exe, Dell.QuickSet                      → Dell
```

Eşleşme varsa tek seferlik bir bilgi kutusu: *"Cihazınızda üreticinin kendi tuş göstergesi çalışıyor olabilir. İki gösterge aynı anda çıkarsa OSD'yi kapatıp yalnızca tray ikonunu kullanabilirsiniz."* + "Bir daha gösterme" onay kutusu (`HKCU\...\OemWarningShown`).

---

## 6. Uygulama Yaşam Döngüsü

```
WinMain
 ├─ SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)   // manifest yedeği
 ├─ CreateMutex(L"Local\\KeyLockIndicator_SingleInstance")
 │   └─ ERROR_ALREADY_EXISTS → mevcut örneğe WM_SHOWSETTINGS gönder, çık
 ├─ CoInitializeEx(APARTMENTTHREADED)
 ├─ Settings::Load()
 ├─ App::Initialize()
 │   ├─ gizli mesaj penceresi (HWND_MESSAGE değil — WM_SETTINGCHANGE için normal pencere, WS_POPUP + gizli)
 │   ├─ ThemeWatcher::Start()
 │   ├─ OsdWindow::Create()      // gizli, önceden hazırlanır (ilk gösterimde gecikme olmasın)
 │   ├─ TrayIcon::Add()
 │   ├─ WTSRegisterSessionNotification()
 │   └─ KeyMonitor::Start()
 ├─ GetMessage döngüsü
 └─ temizlik: KeyMonitor::Stop, TrayIcon::Remove, COM uninit
```

**Kritik:** `OsdWindow` uygulama açılışında oluşturulur ve gizli tutulur; her gösterimde yeniden yaratılmaz. İlk tuş basımında D3D/D2D kurulumu beklenirse ~200 ms gecikme oluşur ve bu fark edilir.

---

## 7. Çoklu Dil

`res/app.rc` içinde iki string tablosu (TR, EN). `Language=auto` ise `GetUserDefaultUILanguage()` primary language ID'si `LANG_TURKISH` ise TR, değilse EN.

Çevrilecek dizeler: tuş adları, "Açık"/"Kapalı", tray menü öğeleri, ayarlar penceresi tüm etiketleri, OEM uyarısı.

---

## 8. Manifest (`res/app.manifest`)

```xml
<?xml version="1.0" encoding="utf-8"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/> <!-- Win10/11 -->
    </application>
  </compatibility>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2</dpiAwareness>
      <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>
    </windowsSettings>
  </application>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls"
        version="6.0.0.0" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/>
    </dependentAssembly>
  </dependency>
</assembly>
```

`uiAccess="false"` olduğu için OSD, UAC yükseltilmiş pencerelerin veya Güvenli Masaüstü'nün üzerinde çizilemez. Bu kabul edilen bir sınırlamadır; `uiAccess="true"` imzalı sertifika ve Program Files kurulumu gerektirir, Store dağıtımıyla uyumsuzdur.

---

## 9. Kodlama Kuralları

- Ham `new`/`delete` yok. COM için `Microsoft::WRL::ComPtr` (`<wrl/client.h>`, ek bağımlılık değil), HANDLE için özel `unique_handle`
- Her COM çağrısının `HRESULT`'ı kontrol edilir: `HR(expr)` makrosu — hata durumunda debug'da `_CrtDbgBreak`, release'de logla ve zarifçe devam et
- Exception kullanılmaz (`/EHsc` açık ama fırlatma yok); hata dönüşleri `bool`/`HRESULT`
- Global değişken yok; `App` singleton'ı `WinMain` yığınında yaşar, pencere prosedürleri `GWLP_USERDATA` üzerinden erişir
- Tüm dizeler `wchar_t` / `std::wstring`
- Her `.cpp` dosyası 400 satırı geçmemeli; geçiyorsa modül bölünmeli
- Warning-free derlenmeli (`/W4`)

---

## 10. Geliştirme Sırası

Aşağıdaki sırayla ilerle; her adım sonunda derlenebilir ve çalışır bir hâl olmalı.

1. **İskelet** — CMake, manifest, boş `WinMain`, tek-instance mutex, gizli pencere, mesaj döngüsü
2. **Tray** — `TrayIcon` + bağlam menüsü + çıkış. Buraya kadar OSD yok
3. **KeyMonitor** — polling, callback, tray tooltip'inin canlı güncellenmesi. Konsol/OutputDebugString ile doğrula
4. **D3D/D2D/DComp zinciri** — `OsdWindow` düz renkli bir kare çizsin, 2 saniye görünüp kaybolsun. Şeffaflığın gerçekten çalıştığını doğrula
5. **OsdRenderer** — kart, gölge, kenarlık, metin. İkonlar henüz basit dikdörtgen placeholder
6. **IconGeometry** — üç tuş × iki durum = altı geometri
7. **Animasyon** — DComp opacity + scale, yeniden tetikleme mantığı
8. **ThemeWatcher** — canlı tema geçişi
9. **Settings + SettingsDialog** — WinDark entegrasyonu, tüm ayarların anında etki etmesi
10. **Autostart, OEM kontrolü, tam ekran bastırma, çoklu monitör, dil**
11. **MSIX paketleme**

---

## 11. Kabul Kriterleri

Aşağıdakilerin tamamı sağlanmadan proje bitmiş sayılmaz.

**İşlevsel**
- [ ] Caps/Num/Scroll her değişiminde 100 ms içinde OSD belirir
- [ ] Harici klavyeyle, ekran klavyesiyle ve RDP oturumu içinden yapılan değişimler yakalanır
- [ ] Uygulama açılışında OSD gösterilmez
- [ ] Uyku/hazırda bekleme sonrası durum sessizce senkronlanır, sahte OSD çıkmaz
- [ ] Hızlı ardışık basımlarda (5 basım / 1 sn) OSD birikmez, tek pencere içeriği güncellenir
- [ ] Explorer sonlandırılıp yeniden başlatıldığında tray ikonu geri gelir

**Görsel**
- [ ] Kart tam kare, kenarları keskin (yarım piksel kayması yok), köşeler pürüzsüz
- [ ] 100 / 125 / 150 / 175 / 200 % ölçeklemede metin ve ikon net; bulanıklık yok
- [ ] Farklı DPI'daki iki monitör arasında pencere sürüklenmese de doğru monitörde doğru boyutta çıkar
- [ ] Arka planda beyaz ve siyah zemin üzerinde okunabilirlik korunur
- [ ] Fade giriş/çıkışta kare veya siyah kenar artefaktı görünmez
- [ ] Tema değişimi OSD görünürken yapılırsa anında yeni renge geçer

**Davranışsal**
- [ ] OSD hiçbir zaman odak çalmaz — üzerinde yazı yazılırken imleç kaybolmaz
- [ ] OSD üzerine tıklanınca altındaki pencere tıklamayı alır
- [ ] Tam ekran oyun/sunumda (ayar açıkken) OSD çıkmaz
- [ ] Ayarlar penceresi Windows temasına uygun açılır (WinDark)

**Performans**
- [ ] Boşta 1 saatlik ölçümde CPU ortalaması < %0.1
- [ ] Çalışma seti < 8 MB (OSD gizliyken), < 25 MB (OSD görünürken, GPU yüzeyi dâhil)
- [ ] 24 saat kesintisiz çalışmada handle/GDI nesnesi sızıntısı yok

---

## 12. Bilinen Sınırlar (belgelenecek, çözülmeye çalışılmayacak)

- UAC istemi ve Güvenli Masaüstü üzerinde OSD görünmez (`uiAccess=false`)
- Bazı özel tam ekran DirectX oyunlarında exclusive fullscreen modunda OSD görünmeyebilir
- Sanal makine konsollarında (VMware/VirtualBox misafir penceresi) tuş durumu ana makineninkinden sapabilir
