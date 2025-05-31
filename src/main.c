#include "resource.h"

#include <mni/mni.h>
#include <VoicemeeterRemote.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#pragma region "Voicemeeter Remote"

static HMODULE g_VMR_Module = NULL;
static T_VBVMR_INTERFACE iVMR;
static wchar_t uninstDirKey[] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
static wchar_t uninstValueKey[] = L"VB:Voicemeeter {17359A74-1236-5467}";

#ifndef KEY_WOW64_32KEY
    #define KEY_WOW64_32KEY 0x0200
#endif

enum {
    VMR_OK                              = 0,

    VMR_ERROR_VOICEMEETER_NOT_INSTALLED = -1,
    VMR_ERROR_FAILED_TO_LOAD_DLL        = -2,

    // Range [-100 : -200] reserved for GetProcAddr errors.
};

static void _RemoveNameInPath(wchar_t* szPath)
{
    int ll = (int)wcslen(szPath);

    while ((ll > 0) && (szPath[ll] != L'\\')) {
        ll--;
    }

    if (szPath[ll] == L'\\') {
        szPath[ll] = L'\0';
    }
}

static BOOL _RegistryGetVoicemeeterFolder(wchar_t* szDir, size_t maxCch) {
    // Build Voicemeeter uninstallation key.
    wchar_t szKey[256];
    memset(szKey, 0, sizeof(szKey));
    
    if (0 != wcsncpy_s(szKey, ARRAYSIZE(szKey), uninstDirKey, wcslen(uninstDirKey))) {
        return FALSE;
    }

    wchar_t pathSeparator[] = L"\\";
    if (0 != wcsncat_s(szKey, ARRAYSIZE(szKey), pathSeparator, wcslen(pathSeparator))) {
        return FALSE;
    }
    
    if (0 != wcsncat_s(szKey, ARRAYSIZE(szKey), uninstValueKey, wcslen(uninstValueKey))) {
        return FALSE;
    }

    // Open key.
    HKEY hkResult;
    {
        LSTATUS rep = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ, &hkResult);
        if (rep != ERROR_SUCCESS) {
            // If not present we consider running in 64bit mode and force to read 32bit registry.
            rep = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_READ | KEY_WOW64_32KEY, &hkResult);
        }

        if (rep != ERROR_SUCCESS) {
            return FALSE;
        }
    }

    // Read uninstall program path.
    wchar_t buf[1024];
    memset(buf, 0, sizeof(buf));

    DWORD pType = REG_SZ;
    DWORD cbData = sizeof(buf); // in bytes
    LSTATUS rep = RegQueryValueEx(hkResult, L"UninstallString", 0, &pType, (LPBYTE)buf, &cbData);
    RegCloseKey(hkResult);

    if (pType != REG_SZ) {
        return FALSE;
    }

    if (rep != ERROR_SUCCESS) {
        return FALSE;
    }

    // Remove name to get the path only.
    _RemoveNameInPath(buf);

    if (0 != wcsncpy_s(szDir, maxCch, buf, ARRAYSIZE(buf))) {
        return FALSE;
    }

    return TRUE;
}

//if we directly link source code (for development only)
#ifdef VBUSE_LOCALLIB

    int VMR_InitializeDLLInterfaces(void) {
        iVMR.VBVMR_Login = VBVMR_Login;
        iVMR.VBVMR_Logout = VBVMR_Logout;
        iVMR.VBVMR_RunVoicemeeter = VBVMR_RunVoicemeeter;
        iVMR.VBVMR_GetVoicemeeterType = VBVMR_GetVoicemeeterType;
        iVMR.VBVMR_GetVoicemeeterVersion = VBVMR_GetVoicemeeterVersion;
        iVMR.VBVMR_IsParametersDirty = VBVMR_IsParametersDirty;
        iVMR.VBVMR_GetParameterFloat = VBVMR_GetParameterFloat;
        iVMR.VBVMR_GetParameterStringA = VBVMR_GetParameterStringA;
        iVMR.VBVMR_GetParameterStringW = VBVMR_GetParameterStringW;

        iVMR.VBVMR_GetLevel = VBVMR_GetLevel;
        iVMR.VBVMR_GetMidiMessage = VBVMR_GetMidiMessage;
        iVMR.VBVMR_SetParameterFloat = VBVMR_SetParameterFloat;
        iVMR.VBVMR_SetParameters = VBVMR_SetParameters;
        iVMR.VBVMR_SetParametersW = VBVMR_SetParametersW;
        iVMR.VBVMR_SetParameterStringA = VBVMR_SetParameterStringA;
        iVMR.VBVMR_SetParameterStringW = VBVMR_SetParameterStringW;

        iVMR.VBVMR_Output_GetDeviceNumber = VBVMR_Output_GetDeviceNumber;
        iVMR.VBVMR_Output_GetDeviceDescA = VBVMR_Output_GetDeviceDescA;
        iVMR.VBVMR_Output_GetDeviceDescW = VBVMR_Output_GetDeviceDescW;
        iVMR.VBVMR_Input_GetDeviceNumber = VBVMR_Input_GetDeviceNumber;
        iVMR.VBVMR_Input_GetDeviceDescA = VBVMR_Input_GetDeviceDescA;
        iVMR.VBVMR_Input_GetDeviceDescW = VBVMR_Input_GetDeviceDescW;

    #ifdef VMR_INCLUDE_AUDIO_PROCESSING_EXAMPLE
        iVMR.VBVMR_AudioCallbackRegister = VBVMR_AudioCallbackRegister;
        iVMR.VBVMR_AudioCallbackStart = VBVMR_AudioCallbackStart;
        iVMR.VBVMR_AudioCallbackStop = VBVMR_AudioCallbackStop;
        iVMR.VBVMR_AudioCallbackUnregister = VBVMR_AudioCallbackUnregister;
    #endif
    #ifdef	VMR_INCLUDE_MACROBUTTONS_REMOTING
        iVMR.VBVMR_MacroButton_IsDirty = VBVMR_MacroButton_IsDirty;
        iVMR.VBVMR_MacroButton_GetStatus = VBVMR_MacroButton_GetStatus;
        iVMR.VBVMR_MacroButton_SetStatus = VBVMR_MacroButton_SetStatus;
    #endif

        return 0;
    }

//Dynamic link to DLL in 'C' (regular use)
#else

    int VMR_InitializeDLLInterfaces(void) {
        wchar_t szDllName[1024];
        memset(&iVMR, 0, sizeof(T_VBVMR_INTERFACE));

        // Get folder where is installed Voicemeeter.
        if (_RegistryGetVoicemeeterFolder(szDllName, ARRAYSIZE(szDllName)) == FALSE) {
            // Voicemeeter not installed.
            return VMR_ERROR_VOICEMEETER_NOT_INSTALLED;
        }

        // Use right dll according O/S type.
        if (sizeof(void*) == 8) {
            wcscat_s(szDllName, ARRAYSIZE(szDllName), L"\\VoicemeeterRemote64.dll");
        } else {
            wcscat_s(szDllName, ARRAYSIZE(szDllName), L"\\VoicemeeterRemote.dll");
        }

        // Load Dll
        g_VMR_Module = LoadLibrary(szDllName);
        if (g_VMR_Module == NULL) {
            return VMR_ERROR_FAILED_TO_LOAD_DLL;
        }

        // Get function pointers.
        iVMR.VBVMR_Login = (T_VBVMR_Login)GetProcAddress(g_VMR_Module, "VBVMR_Login");
        iVMR.VBVMR_Logout = (T_VBVMR_Logout)GetProcAddress(g_VMR_Module, "VBVMR_Logout");
        iVMR.VBVMR_RunVoicemeeter = (T_VBVMR_RunVoicemeeter)GetProcAddress(g_VMR_Module, "VBVMR_RunVoicemeeter");
        iVMR.VBVMR_GetVoicemeeterType = (T_VBVMR_GetVoicemeeterType)GetProcAddress(g_VMR_Module, "VBVMR_GetVoicemeeterType");
        iVMR.VBVMR_GetVoicemeeterVersion = (T_VBVMR_GetVoicemeeterVersion)GetProcAddress(g_VMR_Module, "VBVMR_GetVoicemeeterVersion");

        iVMR.VBVMR_IsParametersDirty = (T_VBVMR_IsParametersDirty)GetProcAddress(g_VMR_Module, "VBVMR_IsParametersDirty");
        iVMR.VBVMR_GetParameterFloat = (T_VBVMR_GetParameterFloat)GetProcAddress(g_VMR_Module, "VBVMR_GetParameterFloat");
        iVMR.VBVMR_GetParameterStringA = (T_VBVMR_GetParameterStringA)GetProcAddress(g_VMR_Module, "VBVMR_GetParameterStringA");
        iVMR.VBVMR_GetParameterStringW = (T_VBVMR_GetParameterStringW)GetProcAddress(g_VMR_Module, "VBVMR_GetParameterStringW");
        iVMR.VBVMR_GetLevel = (T_VBVMR_GetLevel)GetProcAddress(g_VMR_Module, "VBVMR_GetLevel");
        iVMR.VBVMR_GetMidiMessage = (T_VBVMR_GetMidiMessage)GetProcAddress(g_VMR_Module, "VBVMR_GetMidiMessage");

        iVMR.VBVMR_SetParameterFloat = (T_VBVMR_SetParameterFloat)GetProcAddress(g_VMR_Module, "VBVMR_SetParameterFloat");
        iVMR.VBVMR_SetParameters = (T_VBVMR_SetParameters)GetProcAddress(g_VMR_Module, "VBVMR_SetParameters");
        iVMR.VBVMR_SetParametersW = (T_VBVMR_SetParametersW)GetProcAddress(g_VMR_Module, "VBVMR_SetParametersW");
        iVMR.VBVMR_SetParameterStringA = (T_VBVMR_SetParameterStringA)GetProcAddress(g_VMR_Module, "VBVMR_SetParameterStringA");
        iVMR.VBVMR_SetParameterStringW = (T_VBVMR_SetParameterStringW)GetProcAddress(g_VMR_Module, "VBVMR_SetParameterStringW");

        iVMR.VBVMR_Output_GetDeviceNumber = (T_VBVMR_Output_GetDeviceNumber)GetProcAddress(g_VMR_Module, "VBVMR_Output_GetDeviceNumber");
        iVMR.VBVMR_Output_GetDeviceDescA = (T_VBVMR_Output_GetDeviceDescA)GetProcAddress(g_VMR_Module, "VBVMR_Output_GetDeviceDescA");
        iVMR.VBVMR_Output_GetDeviceDescW = (T_VBVMR_Output_GetDeviceDescW)GetProcAddress(g_VMR_Module, "VBVMR_Output_GetDeviceDescW");
        iVMR.VBVMR_Input_GetDeviceNumber = (T_VBVMR_Input_GetDeviceNumber)GetProcAddress(g_VMR_Module, "VBVMR_Input_GetDeviceNumber");
        iVMR.VBVMR_Input_GetDeviceDescA = (T_VBVMR_Input_GetDeviceDescA)GetProcAddress(g_VMR_Module, "VBVMR_Input_GetDeviceDescA");
        iVMR.VBVMR_Input_GetDeviceDescW = (T_VBVMR_Input_GetDeviceDescW)GetProcAddress(g_VMR_Module, "VBVMR_Input_GetDeviceDescW");

    #ifdef VMR_INCLUDE_AUDIO_PROCESSING_EXAMPLE
        iVMR.VBVMR_AudioCallbackRegister = (T_VBVMR_AudioCallbackRegister)GetProcAddress(g_VMR_Module, "VBVMR_AudioCallbackRegister");
        iVMR.VBVMR_AudioCallbackStart = (T_VBVMR_AudioCallbackStart)GetProcAddress(g_VMR_Module, "VBVMR_AudioCallbackStart");
        iVMR.VBVMR_AudioCallbackStop = (T_VBVMR_AudioCallbackStop)GetProcAddress(g_VMR_Module, "VBVMR_AudioCallbackStop");
        iVMR.VBVMR_AudioCallbackUnregister = (T_VBVMR_AudioCallbackUnregister)GetProcAddress(g_VMR_Module, "VBVMR_AudioCallbackUnregister");
    #endif
    #ifdef	VMR_INCLUDE_MACROBUTTONS_REMOTING
        iVMR.VBVMR_MacroButton_IsDirty = (T_VBVMR_MacroButton_IsDirty)GetProcAddress(g_VMR_Module, "VBVMR_MacroButton_IsDirty");
        iVMR.VBVMR_MacroButton_GetStatus = (T_VBVMR_MacroButton_GetStatus)GetProcAddress(g_VMR_Module, "VBVMR_MacroButton_GetStatus");
        iVMR.VBVMR_MacroButton_SetStatus = (T_VBVMR_MacroButton_SetStatus)GetProcAddress(g_VMR_Module, "VBVMR_MacroButton_SetStatus");
    #endif

        // Check pointers are valid.
        if (iVMR.VBVMR_Login == NULL) return -100;
        if (iVMR.VBVMR_Logout == NULL) return -101;
        if (iVMR.VBVMR_RunVoicemeeter == NULL) return -102;
        if (iVMR.VBVMR_GetVoicemeeterType == NULL) return -103;
        if (iVMR.VBVMR_GetVoicemeeterVersion == NULL) return -104;
        if (iVMR.VBVMR_IsParametersDirty == NULL) return -105;
        if (iVMR.VBVMR_GetParameterFloat == NULL) return -106;
        if (iVMR.VBVMR_GetParameterStringA == NULL) return -107;
        if (iVMR.VBVMR_GetParameterStringW == NULL) return -108;
        if (iVMR.VBVMR_GetLevel == NULL) return -109;
        if (iVMR.VBVMR_SetParameterFloat == NULL) return -110;
        if (iVMR.VBVMR_SetParameters == NULL) return -111;
        if (iVMR.VBVMR_SetParametersW == NULL) return -112;
        if (iVMR.VBVMR_SetParameterStringA == NULL) return -113;
        if (iVMR.VBVMR_SetParameterStringW == NULL) return -114;
        if (iVMR.VBVMR_GetMidiMessage == NULL) return -115;

        if (iVMR.VBVMR_Output_GetDeviceNumber == NULL) return -130;
        if (iVMR.VBVMR_Output_GetDeviceDescA == NULL) return -131;
        if (iVMR.VBVMR_Output_GetDeviceDescW == NULL) return -132;
        if (iVMR.VBVMR_Input_GetDeviceNumber == NULL) return -133;
        if (iVMR.VBVMR_Input_GetDeviceDescA == NULL) return -134;
        if (iVMR.VBVMR_Input_GetDeviceDescW == NULL) return -135;

    #ifdef VMR_INCLUDE_AUDIO_PROCESSING_EXAMPLE
        if (iVMR.VBVMR_AudioCallbackRegister == NULL) return -140;
        if (iVMR.VBVMR_AudioCallbackStart == NULL) return -141;
        if (iVMR.VBVMR_AudioCallbackStop == NULL) return -142;
        if (iVMR.VBVMR_AudioCallbackUnregister == NULL) return -143;
    #endif
    #ifdef	VMR_INCLUDE_MACROBUTTONS_REMOTING
        if (iVMR.VBVMR_MacroButton_IsDirty == NULL) return -150;
        if (iVMR.VBVMR_MacroButton_GetStatus == NULL) return -151;
        if (iVMR.VBVMR_MacroButton_SetStatus == NULL) return -152;
    #endif

        return VMR_OK;
    }

#endif // VBUSE_LOCALLIB

BOOL VMR_Init(HWND hWnd) {
    wchar_t szTitle[] = L"VMR Init Error";
    
    // Get DLL interface.
    int rep = VMR_InitializeDLLInterfaces();
    if (rep < 0) {
        if (rep == VMR_ERROR_VOICEMEETER_NOT_INSTALLED) {
            MessageBox(hWnd, L"Voicemeeter is not installed.", szTitle, MB_APPLMODAL | MB_OK | MB_ICONERROR);
        } else {
            const wchar_t *msg = NULL;
            if (sizeof(void*) == 8) {
                msg = L"Failed to link to VoicemeeterRemote64.dll";
            } else {
                msg = L"Failed to link to VoicemeeterRemote.dll";
            }
            MessageBox(hWnd, msg, szTitle, MB_APPLMODAL | MB_OK | MB_ICONERROR);
        }

        return FALSE;
    }

    // Log in. This should succeed even if Voicemeeter is not running.
    rep = iVMR.VBVMR_Login();
    if (rep < 0) {
        MessageBox(hWnd, L"Failed to login to Voicemeeter.", szTitle, MB_APPLMODAL | MB_OK | MB_ICONERROR);
        return FALSE;
    }

    //if (rep == 1) {
    //    iVMR.VBVMR_RunVoicemeeter(2);
    //    Sleep(1000);
    //}

    // Call this to get first parameters state (if server already launched).
    // 3 times to be sure to get the last settings on startup.
    //for (int vi=0; vi<3; vi++) {
    //    iVMR.VBVMR_IsParametersDirty();
    //#ifdef	VMR_INCLUDE_MACROBUTTONS_REMOTING
    //    iVMR.VBVMR_MacroButton_IsDirty();
    //#endif
    //}

    return TRUE;
}

BOOL VMR_End(HWND hWnd) {
    if (iVMR.VBVMR_Logout != NULL) {
        iVMR.VBVMR_Logout();
    }

    if (iVMR.VBVMR_AudioCallbackUnregister != NULL) {
        iVMR.VBVMR_AudioCallbackUnregister();
    }

    return TRUE;
}

#pragma endregion

#define VMMSI_MENU_MIC_ID               1000
#define VMMSI_MENU_MUTE_UNMUTE          1001
#define VMMSI_MENU_ABOUT                1002
#define VMMSI_MENU_EXIT                 1003

#define VMMSI_TIMER_REFRESH_VOICEMEETER_STATE   MNI_USER_TIMER_ID

typedef struct VMMicStatusIndicator {
    int     stripe_id;
    int     mic_state;
    char    stripe_str[16];
    bool    is_connected;

    HICON   mic_muted_light;
    HICON   mic_muted_dark;
    HICON   mic_unmuted_light;
    HICON   mic_unmuted_dark;
} VMMicStatusIndicator;


static BOOL _IsColorLight(DWORD color) {
    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);

    return (((5 * g) + (2 * r) + b) > (8 * 128));
}

static void VMMSI_RefreshIcon(Mni4 *mni, VMMicStatusIndicator *vmmsi) {
    if (!vmmsi->is_connected) {
        int wh = MulDiv(16, mni->dpi, 96);
        HICON ico = (HICON)LoadImageW(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDI_PROGRAM_ICON),
            IMAGE_ICON,
            wh,
            wh,
            LR_DEFAULTSIZE
        );
        MniSetIcon(mni, ico, MNI_TRUE);
    } else {
        BOOL use_light_icon = TRUE;
        if (mni->system_theme.theme == MNI_THEME_LIGHT) {
             use_light_icon = FALSE;
        } else {
            if (mni->system_theme.theme == MNI_THEME_HIGHCONTRAST && !_IsColorLight(mni->system_theme.text_color)) {
                use_light_icon = FALSE;
            }
        }

        int id = 0;
        if (use_light_icon) {
            id = vmmsi->mic_state ? IDI_DEFAULT_LIGHT_MIC_MUTED: IDI_DEFAULT_LIGHT_MIC_UNMUTED;
        } else {
            id = vmmsi->mic_state ? IDI_DEFAULT_DARK_MIC_MUTED: IDI_DEFAULT_DARK_MIC_UNMUTED;
        }

        int wh = MulDiv(16, mni->dpi, 96);
        HICON ico = (HICON)LoadImageW(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(id),
            IMAGE_ICON,
            wh,
            wh,
            LR_DEFAULTSIZE
        );

        MniSetIcon(mni, ico, MNI_TRUE);
    }
}

static void VMMSI_RefreshTip(Mni4 *mni, VMMicStatusIndicator *vmmsi) {
    if (!vmmsi->is_connected) {
        MniSetTip(mni, L"Voicemeeter Mic Status Indicator - Not Connected");
    } else {
        wchar_t buf[32];
        memset(buf, 0, sizeof(buf));
        swprintf_s(
            buf,
            ARRAYSIZE(buf),
            L"Mic #%d - %s", vmmsi->stripe_id, vmmsi->mic_state ? L"Muted" : L"Unmuted"
        );
        MniSetTip(mni, buf);
    }
}

void VMMSI_OnInit(Mni4 *mni) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }

    memset(vmmsi->stripe_str, 0, sizeof(vmmsi->stripe_str));
    sprintf_s(vmmsi->stripe_str, ARRAYSIZE(vmmsi->stripe_str), "Stripe[%d].Mute", vmmsi->stripe_id);
    
    VMMSI_RefreshIcon(mni, vmmsi);
    VMMSI_RefreshTip(mni, vmmsi);
}

void VMMSI_OnRelease(Mni4 *mni) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }
}

void VMMSI_OnLmbClick(Mni4 *mni, int x, int y) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }
}

void VMMSI_OnTaskbarCreated(Mni4 *mni) {
    MniShow(mni, MNI_TRUE);
}

void VMMSI_OnDpiChange(Mni4 *mni, int dpi) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }

    VMMSI_RefreshIcon(mni, vmmsi);
}

void VMMSI_OnSystemThemeChange(Mni4 *mni, MniThemeInfo mti) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }

    VMMSI_RefreshIcon(mni, vmmsi);
}

void VMMSI_OnTimer(Mni4 *mni, unsigned int timer_id) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }

    int is_dirty = iVMR.VBVMR_IsParametersDirty();
    if (is_dirty < 0) {
        vmmsi->is_connected = false;
    } else if (is_dirty > 0) {
        vmmsi->is_connected = true;

        float fmute = 0.0f;
        iVMR.VBVMR_GetParameterFloat(vmmsi->stripe_str, &fmute);
        vmmsi->mic_state = (fmute != 0.0f) ? 1 : 0;
        
        VMMSI_RefreshIcon(mni, vmmsi);
        VMMSI_RefreshTip(mni, vmmsi);
    }
}

void VMMSI_OnContextMenuOpen(Mni4 *mni) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }

    // Create menus.
    HMENU menu = CreateMenu();
    HMENU popup = CreateMenu();
    
    //wchar_t mic_id_str[8];
    //memset(mic_id_str, 0, sizeof(mic_id_str));
    //swprintf_s(mic_id_str, ARRAYSIZE(mic_id_str), L"Mic #%d", vmmsi->stripe_id);

    //AppendMenuW(menu, MF_STRING | MF_DISABLED, VMMSI_MENU_MIC_ID, mic_id_str);
    //SetMenuItemBitmaps(menu, VMMSI_MENU_MIC_ID, )

    AppendMenuW(menu, MF_STRING, VMMSI_MENU_MUTE_UNMUTE, vmmsi->mic_state ? L"Unmute" : L"Mute");
    AppendMenuW(menu, MF_STRING, VMMSI_MENU_ABOUT, L"About");
    AppendMenuW(menu, MF_STRING, VMMSI_MENU_EXIT, L"Exit");
    AppendMenuW(popup, MF_POPUP, (UINT_PTR)menu, L"");

    MniSetMenu(mni, popup, MNI_TRUE);
}

void VMMSI_OnContextMenuClick(Mni4 *mni, int selected_item) {
    VMMicStatusIndicator *vmmsi = NULL;
    if (MNI_FAILED(MniGetUserData1(mni, &vmmsi))) {
        return;
    }

    switch (selected_item) {
        case VMMSI_MENU_MUTE_UNMUTE:
            iVMR.VBVMR_SetParameterFloat(vmmsi->stripe_str, (vmmsi->mic_state != 0) ? 0.0f : 1.0f);
            vmmsi->mic_state = !vmmsi->mic_state;
            break;
        case VMMSI_MENU_ABOUT:
            
            break;
        case VMMSI_MENU_EXIT:
            MniQuit();
            break;
    }
}

int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_     LPWSTR    lpCmdLine,
    _In_     int       nShowCmd
) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    VMMicStatusIndicator vmmsi;
    memset(&vmmsi, 0, sizeof(vmmsi));

    // Setup MniInfo.
    MniInfo info;
    memset(&info, 0, sizeof(info));

    info.user_data1 = (void *)&vmmsi;
    info.on_init = VMMSI_OnInit;
    info.on_release = VMMSI_OnRelease;
    info.on_taskbar_created = VMMSI_OnTaskbarCreated;
    info.on_dpi_change = VMMSI_OnDpiChange;
    info.on_system_theme_change = VMMSI_OnSystemThemeChange;
    info.on_context_menu_item_click = VMMSI_OnContextMenuClick;
    info.on_context_menu_open = VMMSI_OnContextMenuOpen;
    info.on_timer = VMMSI_OnTimer;
    info.on_lmb_click = VMMSI_OnLmbClick;

    // Init tray icon.
    Mni4 mni;
    if (MNI_FAILED(MniInit(&mni, info)))
    {
        MessageBoxW(NULL, L"Failed to initialize Mni!", L"Error", MB_OK);
        return -1;
    }

    // Show the icon in Notification Area.
    if (MNI_FAILED(MniShow(&mni, MNI_FALSE)))
    {
        MessageBoxW(NULL, L"Failed to show tray icon!", L"Error", MB_OK);
        return -2;
    }
    
    if (!VMR_Init(NULL)) {
        MniRelease(&mni, MNI_TRUE, MNI_TRUE);
        return -3;
    }

    MniStartTimer(&mni, VMMSI_TIMER_REFRESH_VOICEMEETER_STATE, 500);

    int r = MniRunMessageLoop();

    VMR_End(NULL);
    MniRelease(&mni, MNI_TRUE, MNI_TRUE);

    return r;
}
