cmake_minimum_required(VERSION 3.25)

# CMP0091: MSVC runtime library, CMAKE_MSVC_RUNTIME_LIBRARY üzerinden seçilir.
cmake_policy(SET CMP0091 NEW)

project(keylock-indicator
        VERSION 1.0.0
        DESCRIPTION "Caps/Num/Scroll Lock OSD indicator for Windows"
        LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "KeyLockIndicator yalnızca MSVC ile derlenir (spec §1).")
endif()

# --- Global derleme ayarları -------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Statik CRT — runtime kurulumu gerektirmeyen tek dosya .exe (spec §1).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

option(KLI_PACKAGED "MSIX paketli derleme (windows.startupTask kullanır)" OFF)

# --- WinDark (ayarlar penceresi teması) --------------------------------------
add_subdirectory(external/WinDark)

# --- Uygulama ----------------------------------------------------------------
set(KLI_SOURCES
    src/main.cpp
    src/App.cpp
    src/AppCommands.cpp
    src/AppEvents.cpp
    src/AppFilter.cpp
    src/KeyMonitor.cpp
    src/LayoutMonitor.cpp
    src/ThemeWatcher.cpp
    src/MonitorUtil.cpp
    src/IconGeometry.cpp
    src/IconShapes.cpp
    src/OsdRenderer.cpp
    src/OsdText.cpp
    src/OsdWindow.cpp
    src/OsdDevice.cpp
    src/OsdAnimation.cpp
    src/OsdPosition.cpp
    src/OsdBadge.cpp
    src/OsdShow.cpp
    src/ScreenReader.cpp
    src/TrayIcon.cpp
    src/HotkeyBox.cpp
    src/Settings.cpp
    src/SettingsStore.cpp
    src/SettingsDialog.cpp
    src/SettingsFields.cpp
    src/SettingsExclude.cpp
    src/SettingsPaint.cpp
    src/AboutDialog.cpp
    src/Autostart.cpp
    src/OemCheck.cpp
    src/Localization.cpp
    src/Theme.cpp
    src/Util.cpp
)

set(KLI_HEADERS
    src/AboutDialog.h
    src/App.h
    src/AppFilter.h
    src/Autostart.h
    src/HotkeyBox.h
    src/IconGeometry.h
    src/IconShapes.h
    src/KeyMonitor.h
    src/LayoutMonitor.h
    src/Localization.h
    src/LockTypes.h
    src/Messages.h
    src/MonitorUtil.h
    src/OemCheck.h
    src/OsdMetrics.h
    src/OsdRenderer.h
    src/OsdWindow.h
    src/ScreenReader.h
    src/Settings.h
    src/SettingsDialog.h
    src/SettingsFields.h
    src/SettingsPaint.h
    src/SettingsStore.h
    src/Theme.h
    src/ThemeWatcher.h
    src/TrayIcon.h
    src/Util.h
    res/resource.h
)

add_executable(keylock-indicator WIN32
    ${KLI_SOURCES}
    ${KLI_HEADERS}
    res/app.rc
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${KLI_SOURCES} ${KLI_HEADERS})

target_include_directories(keylock-indicator PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/res"
)

target_compile_definitions(keylock-indicator PRIVATE
    UNICODE
    _UNICODE
    STRICT
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    $<$<BOOL:${KLI_PACKAGED}>:KLI_PACKAGED>
)

target_compile_options(keylock-indicator PRIVATE
    /W4             # spec §9: warning-free derlenmeli
    /permissive-
    /utf-8
    /Zc:__cplusplus
    /Zc:inline
    /EHsc           # açık ama fırlatma yok
    /GS
    /Gy             # COMDAT — /OPT:ICF ile boyut düşürür
    /Gw
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/GL>
    $<$<CONFIG:Debug>:/JMC>
)

target_link_options(keylock-indicator PRIVATE
    # Manifest RC üzerinden gömülür (res/app.rc → RT_MANIFEST); linker'ın
    # kendi manifestini üretmesi engellenir, yoksa çakışır.
    /MANIFEST:NO
    /SUBSYSTEM:WINDOWS
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/LTCG>
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/OPT:REF>
    $<$<CONFIG:Release,MinSizeRel,RelWithDebInfo>:/OPT:ICF>
    $<$<CONFIG:Release,MinSizeRel>:/INCREMENTAL:NO>
)

# spec §1 link listesi + comctl32 (manifest'te zorunlu kılınan Common Controls 6
# API'leri: InitCommonControlsEx, TaskDialogIndirect, trackbar/updown).
target_link_libraries(keylock-indicator PRIVATE
    WinDark
    d2d1
    dwrite
    dcomp
    dxgi
    d3d11
    shell32
    user32
    gdi32
    advapi32
    uxtheme
    dwmapi
    shcore
    comctl32
    wtsapi32          # WTSRegisterSessionNotification (spec §6)
    dxguid            # CLSID_D2D1Shadow, d2d1effects.h'de DEFINE_GUID ile bildirilir
    oleaut32          # SysAllocString/SysFreeString — UIA duyurusunun BSTR'leri
    comdlg32          # GetOpenFileNameW/GetSaveFileNameW (madde 18, 32)
    shlwapi           # SHLoadIndirectString — klavye düzeni adı (madde 28)
)
# uiautomationcore.lib BİLEREK bağlanmaz: duyuru API'si çalışma zamanında
# LoadLibrary ile çözülür (bkz. src/ScreenReader.cpp gerekçesi).

if(KLI_PACKAGED)
    # StartupTask WinRT API'si (ham ABI ile; C++/WinRT bağımlılığı yok)
    target_link_libraries(keylock-indicator PRIVATE runtimeobject)
endif()

set_target_properties(keylock-indicator PROPERTIES
    OUTPUT_NAME "KeyLockIndicator"
    VS_DPI_AWARE "PerMonitor"
)

# --- Simge üretimi -----------------------------------------------------------
# res/icons/*.ico dosyaları tools/make_icons.ps1 ile üretilir. Eksikse
# yapılandırma sırasında bir kez çalıştırılır (RC derlemesi onlara bağımlıdır).
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/icons/app.ico")
    message(STATUS "  Simgeler eksik — tools/make_icons.ps1 çalıştırılıyor...")
    execute_process(
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/make_icons.ps1"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE KLI_ICON_RESULT
        OUTPUT_VARIABLE KLI_ICON_OUTPUT
        ERROR_VARIABLE KLI_ICON_OUTPUT
    )
    if(NOT KLI_ICON_RESULT EQUAL 0)
        message(FATAL_ERROR "Simge üretimi başarısız:\n${KLI_ICON_OUTPUT}")
    endif()
endif()

# --- Kurulum / portable ZIP --------------------------------------------------
install(TARGETS keylock-indicator RUNTIME DESTINATION .)
install(FILES README.md DESTINATION .)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "KeyLockIndicator")
set(CPACK_PACKAGE_FILE_NAME "KeyLockIndicator-${PROJECT_VERSION}-portable-x64")
set(CPACK_PACKAGE_VENDOR "ShadesOfDeath")
include(CPack)

message(STATUS "KeyLockIndicator ${PROJECT_VERSION}")
message(STATUS "  Toolset : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  CRT     : ${CMAKE_MSVC_RUNTIME_LIBRARY}")
message(STATUS "  Packaged: ${KLI_PACKAGED}")
