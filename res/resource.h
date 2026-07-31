// resource.h — Kaynak kimlikleri. app.rc ve src/*.cpp tarafından paylaşılır.
#pragma once

// ---------------------------------------------------------------------------
// İkonlar
// ---------------------------------------------------------------------------
#define IDI_APP                     100

// Tray ikonları: <tuş> x <durum> x <tema için üretilmiş glif rengi>
// "DARK" = koyu tema için açık renkli glif, "LIGHT" = açık tema için koyu glif.
#define IDI_TRAY_CAPS_OFF_DARK      110
#define IDI_TRAY_CAPS_ON_DARK       111
#define IDI_TRAY_NUM_OFF_DARK       112
#define IDI_TRAY_NUM_ON_DARK        113
#define IDI_TRAY_SCROLL_OFF_DARK    114
#define IDI_TRAY_SCROLL_ON_DARK     115
#define IDI_TRAY_CAPS_OFF_LIGHT     120
#define IDI_TRAY_CAPS_ON_LIGHT      121
#define IDI_TRAY_NUM_OFF_LIGHT      122
#define IDI_TRAY_NUM_ON_LIGHT       123
#define IDI_TRAY_SCROLL_OFF_LIGHT   124
#define IDI_TRAY_SCROLL_ON_LIGHT    125

// ---------------------------------------------------------------------------
// Diyaloglar
// ---------------------------------------------------------------------------
#define IDD_SETTINGS                200
#define IDD_ABOUT                   201

// Kontroller ------------------------------------------------------------------
#define IDC_STATIC                  (-1)

// Grup: Genel
#define IDC_GRP_GENERAL             1000
#define IDC_CHK_SHOWOSD             1001
#define IDC_CHK_AUTOSTART           1002
#define IDC_LBL_LANGUAGE            1003
#define IDC_CMB_LANGUAGE            1004
#define IDC_LBL_TRAYCLICK           1005
#define IDC_CMB_TRAYCLICK           1006

// Grup: İzlenen tuşlar
#define IDC_GRP_KEYS                1010
#define IDC_CHK_WATCHCAPS           1011
#define IDC_CHK_WATCHNUM            1012
#define IDC_CHK_WATCHSCROLL         1013
#define IDC_LBL_TRAYKEY             1014
#define IDC_CMB_TRAYKEY             1015

// Grup: OSD görünümü
#define IDC_GRP_OSD                 1020
#define IDC_LBL_DURATION            1021
#define IDC_SLD_DURATION            1022
#define IDC_VAL_DURATION            1023
#define IDC_LBL_OPACITY             1024
#define IDC_SLD_OPACITY             1025
#define IDC_VAL_OPACITY             1026
#define IDC_LBL_POSITION            1027
#define IDC_CMB_POSITION            1028
#define IDC_LBL_MARGIN              1029
#define IDC_EDT_MARGIN              1030
#define IDC_SPN_MARGIN              1031
#define IDC_LBL_THEME               1032
#define IDC_CMB_THEME               1033
#define IDC_LBL_SCALE               1034
#define IDC_SLD_SCALE               1035
#define IDC_VAL_SCALE               1036
#define IDC_CHK_ONLYON              1037
#define IDC_BTN_PICKPOS             1038

// Grup: Davranış
#define IDC_GRP_BEHAVIOR            1040
#define IDC_CHK_SUPPRESSFS          1041
// 1042 sürüm 1.0'da "Her zaman ana monitörde göster" onay kutusuydu; yerini
// üç seçenekli IDC_CMB_MONITOR aldı (imleç / ana / tüm monitörler). Kimlik
// yeniden KULLANILMAZ, kaynakta da artık yoktur — eski değer boşta bırakılır.
#define IDC_LBL_MONITOR             1043
#define IDC_CMB_MONITOR             1044
#define IDC_CHK_HOTKEY              1045
#define IDC_HOT_SHORTCUT            1046
#define IDC_LBL_HOTKEYWARN          1047

// Alt butonlar
#define IDC_BTN_DEFAULTS            1050
#define IDC_BTN_CLOSE               1051
#define IDC_LBL_HINT                1052

// Görünüm kipi + kalıcı rozet + ekran okuyucu duyurusu (madde 13–16, 30).
// Kimlikler 1070'ten devam eder: OSD grubunun 1020–1039 aralığında yalnızca tek
// bir boşluk kalmıştı ve mevcut kimlik DEĞERLERİNİ kaydırmak yasak. 1060–1065
// "Hakkında" penceresine ait, bu yüzden onun da üstünden başlanıyor.
#define IDC_LBL_VIEWMODE            1070
#define IDC_CMB_VIEWMODE            1071
#define IDC_CHK_BADGE               1072
#define IDC_CHK_ANNOUNCE            1073

// Görev 4: istisna listesi (madde 18), klavye düzeni (madde 28), taşınabilir
// kip göstergesi (madde 25), içe/dışa aktarma (madde 32). Kimlikler 1080'den
// devam eder; 1074–1079 boş bırakılır (görünüm grubunun ileride büyümesi için).
#define IDC_GRP_EXCLUDE             1080
#define IDC_LST_EXCLUDE             1081
#define IDC_BTN_EXCL_ADD            1082
#define IDC_BTN_EXCL_DEL            1083
#define IDC_CHK_WATCHLAYOUT         1084
#define IDC_LBL_STORAGE             1085
#define IDC_BTN_EXPORT              1086
#define IDC_BTN_IMPORT              1087

// Hakkında penceresi
#define IDC_ABOUT_ICON              1060
#define IDC_ABOUT_NAME              1061
#define IDC_ABOUT_VERSION           1062
#define IDC_ABOUT_DESC              1063
#define IDC_ABOUT_AUTHOR            1064
#define IDC_ABOUT_OK                1065

// ---------------------------------------------------------------------------
// Dizeler — çift dilli STRINGTABLE (TR + EN), spec §7
// ---------------------------------------------------------------------------
#define IDS_APPNAME                 2000
#define IDS_KEY_CAPS                2001
#define IDS_KEY_NUM                 2002
#define IDS_KEY_SCROLL              2003
#define IDS_STATE_ON                2004
#define IDS_STATE_OFF               2005

#define IDS_MENU_SETTINGS           2010
#define IDS_MENU_AUTOSTART          2011
#define IDS_MENU_TOGGLEOSD          2012
#define IDS_MENU_ABOUT              2013
#define IDS_MENU_EXIT               2014

// Hakkında penceresi (IDD_ABOUT). 2021 eskiden MessageBox gövdesiydi; artık
// yalnızca açıklama satırını taşır — kimlik değeri korunur, adı yenilendi.
#define IDS_ABOUT_TITLE             2020
#define IDS_ABOUT_DESC              2021
#define IDS_ABOUT_VERSION           2022
#define IDS_ABOUT_AUTHOR            2023
#define IDS_ABOUT_OK                2024

#define IDS_OEM_TITLE               2030
#define IDS_OEM_MAIN                2031
#define IDS_OEM_BODY                2032
#define IDS_OEM_DONTSHOW            2033

#define IDS_SET_TITLE               2040
#define IDS_SET_GRP_GENERAL         2041
#define IDS_SET_SHOWOSD             2042
#define IDS_SET_AUTOSTART           2043
#define IDS_SET_LANGUAGE            2044
#define IDS_SET_GRP_KEYS            2045
#define IDS_SET_WATCHCAPS           2046
#define IDS_SET_WATCHNUM            2047
#define IDS_SET_WATCHSCROLL         2048
#define IDS_SET_TRAYKEY             2049
#define IDS_SET_GRP_OSD             2050
#define IDS_SET_DURATION            2051
#define IDS_SET_OPACITY             2052
#define IDS_SET_POSITION            2053
#define IDS_SET_MARGIN              2054
#define IDS_SET_THEME               2055
#define IDS_SET_GRP_BEHAVIOR        2056
#define IDS_SET_SUPPRESSFS          2057
// 2058 (IDS_SET_PRIMARYONLY) 1.0'daki "Her zaman ana monitörde göster" onay
// kutusunun metniydi; kutunun yerini IDC_CMB_MONITOR listesi aldı ve dize hiçbir
// yerden okunmuyor. 24 dilde ölü bir çeviri taşımamak için tablolardan
// çıkarıldı. Kimlik DEĞERİ yeniden KULLANILMAZ.
#define IDS_SET_DEFAULTS            2059
#define IDS_SET_CLOSE               2060
#define IDS_SET_HINT                2061
#define IDS_SET_SCALE               2062
#define IDS_SET_ONLYON              2063
#define IDS_SET_TRAYCLICK           2064

// Konum ızgarası. 2070–2072 sürüm 1.0'dan gelir (Üst / Orta / Alt) ve 3x3
// ızgarada üst-orta, orta, alt-orta karşılığıdır; kimlik DEĞERLERİ korunur,
// yalnızca metinleri eksen belirtecek şekilde netleştirildi. Yeni altı konum
// ve serbest konum 2100'den devam eder (bkz. aşağı).
#define IDS_POS_TOP                 2070
#define IDS_POS_CENTER              2071
#define IDS_POS_BOTTOM              2072
#define IDS_THEME_SYSTEM            2073
#define IDS_THEME_LIGHT             2074
#define IDS_THEME_DARK              2075
#define IDS_LANG_AUTO               2076
// 2077 (IDS_LANG_TR) ve 2078 (IDS_LANG_EN) ARTIK YOK. Dil listesi 24 dile
// çıkınca her dil adının 24 tabloda tekrarlanması gerekirdi; oysa bir dilin
// KENDİ adı ("Deutsch", "日本語") çevrilmez — arayüz hangi dilde olursa olsun
// aynı yazılır. Bu yüzden adlar kli::Loc::Languages() tablosunda literal olarak
// durur ve kaynaklardan çıkarıldı. Kimlik DEĞERLERİ yeniden KULLANILMAZ.

#define IDS_UNIT_MS                 2080
#define IDS_UNIT_PERCENT            2081
// 2082 (IDS_UNIT_DIP) hiçbir yerden okunmuyor: kenar boşluğu kutusunun birimi
// etikete gömülü. Aynı gerekçeyle tablolardan çıkarıldı; kimlik yeniden
// KULLANILMAZ.

// Tepsi sol tık eylemi — öğe sırası kli::TrayClickAction ile birebir.
#define IDS_TRAYCLICK_OSD           2090
#define IDS_TRAYCLICK_SETTINGS      2091
#define IDS_TRAYCLICK_NONE          2092

// 3x3 konum ızgarasının yeni köşe/kenar konumları + serbest konum.
#define IDS_POS_TOPLEFT             2100
#define IDS_POS_TOPRIGHT            2101
#define IDS_POS_MIDLEFT             2102
#define IDS_POS_MIDRIGHT            2103
#define IDS_POS_BOTLEFT             2104
#define IDS_POS_BOTRIGHT            2105
#define IDS_POS_CUSTOM              2106
#define IDS_SET_PICKPOS             2107
#define IDS_SET_PICKHINT            2108

// Monitör hedefi — öğe sırası kli::MonitorTarget ile birebir (0/1/2).
#define IDS_SET_MONITOR             2110
#define IDS_MON_CURSOR              2111
#define IDS_MON_PRIMARY             2112
#define IDS_MON_ALL                 2113

// Global kısayol
#define IDS_SET_HOTKEY              2120
#define IDS_HOTKEY_FAILED           2121

// Görünüm kipleri (madde 13–15) — öğe sırası kli::OsdViewMode ile birebir (0/1/2).
#define IDS_SET_VIEWMODE            2130
#define IDS_VIEW_ICONTEXT           2131
#define IDS_VIEW_ICONONLY           2132
#define IDS_VIEW_BAR                2133
// Kalıcı rozet (madde 16) ve ekran okuyucu duyurusu (madde 30)
#define IDS_SET_BADGE               2134
#define IDS_SET_ANNOUNCE            2135

// Görev 4 dizeleri ------------------------------------------------------------
// Uygulama bazlı istisna listesi (madde 18)
#define IDS_SET_GRP_EXCLUDE         2140
#define IDS_SET_EXCL_ADD            2141
#define IDS_SET_EXCL_DEL            2142
#define IDS_FILTER_EXE              2143
// Ayarları içe/dışa aktarma (madde 32)
#define IDS_FILTER_INI              2144
#define IDS_SET_EXPORT              2145
#define IDS_SET_IMPORT              2146
#define IDS_IMPORT_FAILED           2147
#define IDS_EXPORT_FAILED           2148
// Taşınabilir kip göstergesi (madde 25)
#define IDS_STORAGE_REGISTRY        2149
#define IDS_STORAGE_PORTABLE        2150
// Klavye düzeni göstergesi (madde 28)
#define IDS_SET_WATCHLAYOUT         2151
#define IDS_LAYOUT_TITLE            2152
