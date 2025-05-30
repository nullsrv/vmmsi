#include <VoicemeeterRemote.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

int main(int argc, char* argv[]) {
    if (!VMR_Init(NULL)) {
        return -1;
    }

    VMR_End(NULL);

    return 0;
}
