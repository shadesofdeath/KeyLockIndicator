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

// Kontroller ------------------------------------------------------------------
#define IDC_STATIC                  (-1)

// Grup: Genel
#define IDC_GRP_GENERAL             1000
#define IDC_CHK_SHOWOSD             1001
#define IDC_CHK_AUTOSTART           1002
#define IDC_LBL_LANGUAGE            1003
#define IDC_CMB_LANGUAGE            1004

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

// Grup: Davranış
#define IDC_GRP_BEHAVIOR            1040
#define IDC_CHK_SUPPRESSFS          1041
#define IDC_CHK_PRIMARYONLY         1042

// Alt butonlar
#define IDC_BTN_DEFAULTS            1050
#define IDC_BTN_CLOSE               1051
#define IDC_LBL_HINT                1052

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

#define IDS_ABOUT_TITLE             2020
#define IDS_ABOUT_TEXT              2021

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
#define IDS_SET_PRIMARYONLY         2058
#define IDS_SET_DEFAULTS            2059
#define IDS_SET_CLOSE               2060
#define IDS_SET_HINT                2061

#define IDS_POS_TOP                 2070
#define IDS_POS_CENTER              2071
#define IDS_POS_BOTTOM              2072
#define IDS_THEME_SYSTEM            2073
#define IDS_THEME_LIGHT             2074
#define IDS_THEME_DARK              2075
#define IDS_LANG_AUTO               2076
#define IDS_LANG_TR                 2077
#define IDS_LANG_EN                 2078

#define IDS_UNIT_MS                 2080
#define IDS_UNIT_PERCENT            2081
#define IDS_UNIT_DIP                2082
