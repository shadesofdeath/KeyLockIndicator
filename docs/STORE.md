# Publishing to the Microsoft Store

Everything needed to turn `KeyLockIndicator.exe` into a package Partner Center
accepts. The app itself is already Store-shaped — this document covers the
packaging pipeline, the values you must replace, and the certification traps
that are specific to a full-trust Win32 tray app.

For build and architecture notes see [DEVELOPMENT.md](DEVELOPMENT.md).

---

## The short version

```powershell
# 1. Try it locally first (no certificate, no admin — Developer Mode only)
tools\make_msix.ps1 -Register

# 2. Build the package you upload
tools\make_msix.ps1 -Store `
    -IdentityName '12345ShadesOfDeath.KeyLockIndicator' `
    -Publisher    'CN=A1B2C3D4-5E6F-7890-ABCD-EF1234567890' `
    -PublisherDisplayName 'ShadesOfDeath'
```

Output: `build\msix\KeyLockIndicator-1.0.0.0-x64.msix` — around 470 KB, unsigned,
ready to upload.

---

## Why the app is already Store-compatible

These are the four things that usually block a Win32 tray utility. All are
handled in code, not by the packager:

| Requirement | How it is met |
|---|---|
| No `Run` registry key for autostart | `windows.startupTask` extension + `StartupTask` WinRT API — `src/Autostart.cpp` picks the backend from `GetCurrentPackageFullName()` at runtime |
| No low-level keyboard hooks | Lock state is polled with `GetKeyState`, 12 keystroke-free lines in `src/KeyMonitor.cpp` |
| No undocumented static imports | The dark-mode `uxtheme` ordinals are resolved with `GetProcAddress` and degrade to no-ops if missing (`external/WinDark`) |
| No admin rights | `requestedExecutionLevel` is `asInvoker`; nothing writes outside `HKCU` |

The one restricted capability the package declares is `runFullTrust`, which every
packaged desktop app needs. See [Restricted capability](#restricted-capability)
for the justification text.

---

## Packaging modes

`tools\make_msix.ps1` has three modes because "install it on my machine" and
"upload it to the Store" need genuinely different packages.

### `-Register` — the development loop

Registers the staging folder as loose files. **No certificate, no signature, no
admin.** Requires Developer Mode (Settings → Privacy & security → For
developers). This is how you verify the packaged code path — most importantly
that the autostart checkbox drives `StartupTask` instead of the `Run` key.

```powershell
tools\make_msix.ps1 -Register
Get-AppxPackage -Name ShadesOfDeath.KeyLockIndicator | Remove-AppxPackage   # undo
```

### `-Sign` — sideloading

Produces a `.msix` signed with a self-signed certificate, generated on first use
into `build\msix\KeyLockIndicator-test.pfx`. The certificate subject is taken
from the manifest's `Publisher` — they must match exactly or Windows rejects the
package even though the signature is valid.

Any machine installing it must trust that certificate first (admin, once):

```powershell
Import-Certificate -FilePath build\msix\KeyLockIndicator-test.cer `
                   -CertStoreLocation Cert:\LocalMachine\TrustedPeople
```

### `-Store` — the submission

Produces an **unsigned** `.msix`. This is deliberate: the Store re-signs every
package with its own certificate. Uploading a package you signed yourself fails
validation with a publisher mismatch.

---

## Identity: the three values you must replace

The manifest in the repo carries placeholder identity values so the package can
be built and tested without a Partner Center account. Real submissions must use
the values Microsoft assigned to your reservation.

Find them in **Partner Center → your app → Product management → Product
identity**:

| Partner Center field | Script parameter | Looks like |
|---|---|---|
| Package/Identity/Name | `-IdentityName` | `12345ShadesOfDeath.KeyLockIndicator` |
| Package/Identity/Publisher | `-Publisher` | `CN=A1B2C3D4-5E6F-7890-ABCD-EF1234567890` |
| Package/Properties/PublisherDisplayName | `-PublisherDisplayName` | `ShadesOfDeath` |

The script writes them into the staged manifest via the XML DOM — the file in
`packaging/` is never modified, so the placeholders stay in version control and
your real publisher GUID does not.

### Version

Taken from `project(VERSION ...)` in `CMakeLists.txt` and padded to four parts.
**The fourth part must be 0** — the Store reserves the revision field and rejects
anything else. Override with `-Version 1.2.0.0` only if you need a package
version that differs from the source version.

Each submission needs a version strictly higher than the last one accepted; you
cannot re-upload a package with a version the Store has already seen, even if
the previous submission was cancelled.

---

## Restricted capability

`runFullTrust` is a restricted capability, so the submission form asks you to
justify it. Something like:

> KeyLock Indicator is a native Win32 desktop application packaged with the
> Desktop Bridge. It requires full trust to use `Shell_NotifyIcon` for its tray
> icon, Direct2D and DirectComposition for the on-screen card, and `GetKeyState`
> to read lock-key state. It does not install a driver or service, requires no
> administrator rights, and does not access user files or the network.

That last sentence matters — reviewers look for a reason a keyboard-adjacent
utility would need broad access, and this app genuinely has none.

---

## Store listing checklist

Things Partner Center will not let you submit without:

- **Age rating** — complete the IARC questionnaire. No ads, no user-generated
  content, no data collection makes this a two-minute form and a 3+ rating.
- **Privacy policy URL** — required for every app, including ones that collect
  nothing. A short page stating that the app stores settings locally
  (`HKCU\Software\ShadesOfDeath\KeyLockIndicator` or a portable `.ini`), makes no
  network connections, and transmits nothing is enough. GitHub Pages works.
- **Screenshots** — at least one, 1366 × 768 or larger. `docs/screenshots/` has
  the OSD in both themes and the settings window; the settings screenshot is the
  one that shows the app has depth.
- **Description** — the README's "What it does" and "Features" sections transfer
  almost verbatim.
- **Supported languages** — the 25 languages declared in `<Resources>` populate
  this automatically once the package is uploaded.
- **Category** — Utilities & tools.

### Accessibility

The Store has an optional "product is accessible" declaration. This app can
honestly claim it: the indicators encode state as **shape, not just colour**,
high contrast mode is honoured live, and lock changes are announced through UI
Automation for screen readers. Declaring it surfaces the app in accessibility
filters.

---

## Certification check before uploading

The Windows App Certification Kit catches most rejections locally. It needs
administrator rights, so run it from an elevated prompt:

```powershell
tools\make_msix.ps1 -Store -Wack `
    -IdentityName '...' -Publisher 'CN=...'
```

Or point it at an existing package directly:

```powershell
& "${env:ProgramFiles(x86)}\Windows Kits\10\App Certification Kit\appcert.exe" test `
    -appxpackagepath build\msix\KeyLockIndicator-1.0.0.0-x64.msix `
    -reportoutputpath build\msix\wack.xml
```

If `appcert.exe` is missing, install the **Windows App Certification Kit**
component from the Windows SDK installer.

Expect warnings, not failures. The two that show up for this kind of app and are
safe to ignore:

- *Package sanity — file encoding.* Fires on the PNG assets; irrelevant.
- *Supported API test.* Only applies to UWP; a `runFullTrust` desktop app is
  exempt.

---

## Architecture coverage

The pipeline currently produces **x64 only**, which is a complete, publishable
submission — ARM64 devices run x64 packages through emulation.

To add a native ARM64 package you need the *MSVC v143/v145 ARM64 build tools*
component in Visual Studio, and `tools/build.ps1` needs to select
`vcvarsamd64_arm64.bat` instead of `vcvars64.bat`. `make_msix.ps1` already takes
`-Arch arm64` and refuses early with a clear message until that lands. Two
single-architecture `.msix` files can be uploaded to the same submission; a
bundle is not required.

---

## What is *not* in the package

The staging folder contains exactly three things: the executable, `Assets\`, and
the generated `resources.pri`. Two omissions are deliberate:

- **No `README.md`.** It ships in the portable ZIP, not the package.
- **No `KeyLockIndicator.ini`.** An `.ini` next to the executable switches the
  app into portable mode — and an MSIX install directory is read-only, so
  settings would silently fail to save. Packaged installs use the registry,
  which MSIX redirects into the package's private hive and cleans up on
  uninstall.

---

## After the first submission

- Certification usually completes within a few hours for an app this size.
- Bump `project(VERSION ...)` in `CMakeLists.txt` for each update; the package
  version follows automatically.
- The `Identity/Name` and `Publisher` values never change across updates — only
  the version does.
