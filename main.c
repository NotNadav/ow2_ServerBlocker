#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0600

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <commctrl.h>
#include <commdlg.h>
#include "main.h"

#define MAX_SERVERS 5
#define BTN_ID_BASE 100
#define BTN_ID_BROWSE 999
#define MAX_CIDRS_PER_RULE 40
#define CONFIG_FILE_NAME "ow2_path.cfg"

typedef struct {
    const char* name;
    const char** cidrs;
    int cidrCount;
    const char* ruleName;
    bool isBlocked;
    HWND hwndName;
    HWND hwndStatus;
    HWND hwndButton;
} ServerEntry;

static ServerEntry servers[MAX_SERVERS] = {
    { "Middle East", ME_IPS,      ME_IPS_COUNT,      "OW2_Block_Middle_East", false, 0,0,0 },
    { "Europe",      EU_IPS,      EU_IPS_COUNT,      "OW2_Block_Europe",      false, 0,0,0 },
    { "US West",     US_WEST_IPS, US_WEST_IPS_COUNT, "OW2_Block_US_West",     false, 0,0,0 },
    { "US East",     US_EAST_IPS, US_EAST_IPS_COUNT, "OW2_Block_US_East",     false, 0,0,0 },
    { "Asia",        ASIA_IPS,    ASIA_IPS_COUNT,    "OW2_Block_Asia",        false, 0,0,0 }
};

static char g_gamePath[MAX_PATH] = "";
static HWND g_hwndPathLabel = NULL;


bool IsAdmin(void) {
    bool isAdmin = false;
    PSID administratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        BOOL check = FALSE;
        if (CheckTokenMembership(NULL, administratorsGroup, &check)) {
            isAdmin = check;
        }
        FreeSid(administratorsGroup);
    }
    return isAdmin;
}


static void GetConfigFilePath(char* outPath, size_t outSize) {
    char exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);

    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';

    snprintf(outPath, outSize, "%s%s", exePath, CONFIG_FILE_NAME);
}

static bool LoadGamePathFromConfig(void) {
    char cfgPath[MAX_PATH];
    GetConfigFilePath(cfgPath, sizeof(cfgPath));

    FILE* f = fopen(cfgPath, "r");
    if (!f) return false;

    if (fgets(g_gamePath, sizeof(g_gamePath), f) == NULL) {
        fclose(f);
        return false;
    }
    fclose(f);

    size_t len = strlen(g_gamePath);
    if (len > 0 && (g_gamePath[len - 1] == '\n' || g_gamePath[len - 1] == '\r')) {
        g_gamePath[len - 1] = '\0';
    }

    /* אם הקובץ שנשמר כבר לא קיים בפועל, נתעלם ממנו */
    if (GetFileAttributes(g_gamePath) == INVALID_FILE_ATTRIBUTES) {
        g_gamePath[0] = '\0';
        return false;
    }

    return g_gamePath[0] != '\0';
}

static void SaveGamePathToConfig(void) {
    char cfgPath[MAX_PATH];
    GetConfigFilePath(cfgPath, sizeof(cfgPath));

    FILE* f = fopen(cfgPath, "w");
    if (!f) return;
    fputs(g_gamePath, f);
    fclose(f);
}

static bool BrowseForGameExe(HWND hwnd) {
    char fileBuffer[MAX_PATH] = "Overwatch.exe";

    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "Overwatch.exe\0Overwatch.exe\0All Files\0*.*\0";
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = sizeof(fileBuffer);
    ofn.lpstrTitle = "בחר את קובץ Overwatch.exe";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        strncpy(g_gamePath, fileBuffer, sizeof(g_gamePath) - 1);
        g_gamePath[sizeof(g_gamePath) - 1] = '\0';
        SaveGamePathToConfig();
        return true;
    }
    return false;
}

static void UpdatePathLabel(void) {
    if (!g_hwndPathLabel) return;
    if (g_gamePath[0] != '\0') {
        char display[MAX_PATH + 20];
        snprintf(display, sizeof(display), "Game: %s", g_gamePath);
        SetWindowText(g_hwndPathLabel, display);
    } else {
        SetWindowText(g_hwndPathLabel, "Game: not selected");
    }
}


void RunCommandHidden(char* cmd) {
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

bool CheckRuleExists(const char* ruleName) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "netsh advfirewall firewall show rule name=\"%s_Out_0\"", ruleName);

    FILE* pipe = _popen(cmd, "r");
    if (!pipe) return false;

    char buffer[256];
    bool found = false;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        if (strstr(buffer, ruleName) != NULL) {
            found = true;
            break;
        }
    }
    _pclose(pipe);
    return found;
}

void UpdateServerUI(int index) {
    SetWindowText(servers[index].hwndStatus, servers[index].isBlocked ? "BLOCKED" : "ALLOWED");
}

static char* BuildIpListChunk(const char** cidrs, int start, int end) {
    size_t totalLen = 1;
    for (int i = start; i < end; i++) totalLen += strlen(cidrs[i]) + 1;

    char* result = (char*)malloc(totalLen);
    result[0] = '\0';
    for (int i = start; i < end; i++) {
        strcat(result, cidrs[i]);
        if (i != end - 1) strcat(result, ",");
    }
    return result;
}

static void ApplyOrRemoveRules(ServerEntry* srv, bool block) {
    bool haveGamePath = (g_gamePath[0] != '\0');

    int chunkIndex = 0;
    for (int start = 0; start < srv->cidrCount; start += MAX_CIDRS_PER_RULE, chunkIndex++) {
        int end = start + MAX_CIDRS_PER_RULE;
        if (end > srv->cidrCount) end = srv->cidrCount;

        char ruleOut[80], ruleIn[80];
        snprintf(ruleOut, sizeof(ruleOut), "%s_Out_%d", srv->ruleName, chunkIndex);
        snprintf(ruleIn,  sizeof(ruleIn),  "%s_In_%d",  srv->ruleName, chunkIndex);

        if (block) {
            char* ipChunk = BuildIpListChunk(srv->cidrs, start, end);
            size_t cmdLen = strlen(ipChunk) + strlen(g_gamePath) + 320;

            char* cmdOut = (char*)malloc(cmdLen);
            char* cmdIn  = (char*)malloc(cmdLen);

            if (haveGamePath) {
                snprintf(cmdOut, cmdLen,
                    "netsh advfirewall firewall add rule name=\"%s\" dir=out action=block program=\"%s\" remoteip=\"%s\"",
                    ruleOut, g_gamePath, ipChunk);
                snprintf(cmdIn, cmdLen,
                    "netsh advfirewall firewall add rule name=\"%s\" dir=in action=block program=\"%s\" remoteip=\"%s\"",
                    ruleIn, g_gamePath, ipChunk);
            } else {
                /* אם עדיין לא נבחר נתיב - נופלים חזרה לחסימה כללית (כמו קודם) */
                snprintf(cmdOut, cmdLen,
                    "netsh advfirewall firewall add rule name=\"%s\" dir=out action=block remoteip=\"%s\"",
                    ruleOut, ipChunk);
                snprintf(cmdIn, cmdLen,
                    "netsh advfirewall firewall add rule name=\"%s\" dir=in action=block remoteip=\"%s\"",
                    ruleIn, ipChunk);
            }

            RunCommandHidden(cmdOut);
            RunCommandHidden(cmdIn);

            free(cmdOut);
            free(cmdIn);
            free(ipChunk);
        } else {
            char cmdOut[256], cmdIn[256];
            snprintf(cmdOut, sizeof(cmdOut), "netsh advfirewall firewall delete rule name=\"%s\"", ruleOut);
            snprintf(cmdIn,  sizeof(cmdIn),  "netsh advfirewall firewall delete rule name=\"%s\"", ruleIn);
            RunCommandHidden(cmdOut);
            RunCommandHidden(cmdIn);
        }
    }
}

void ToggleServerBlock(int index) {
    ServerEntry* srv = &servers[index];
    bool newState = !srv->isBlocked;

    ApplyOrRemoveRules(srv, newState);

    srv->isBlocked = newState;
    UpdateServerUI(index);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HFONT hFont = CreateFont(18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, VARIABLE_PITCH, "Segoe UI");
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hFont);

            LoadGamePathFromConfig();

           
            HWND hwndBrowseBtn = CreateWindow("BUTTON", "Select Overwatch.exe", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                              20, 15, 180, 28, hwnd, (HMENU)(INT_PTR)BTN_ID_BROWSE, NULL, NULL);
            SendMessage(hwndBrowseBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

            g_hwndPathLabel = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD,
                                           20, 48, 350, 18, hwnd, NULL, NULL, NULL);
            SendMessage(g_hwndPathLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
            UpdatePathLabel();

            for (int i = 0; i < MAX_SERVERS; i++) {
                int yPos = 80 + (i * 45);

                servers[i].hwndName = CreateWindow("STATIC", servers[i].name, WS_VISIBLE | WS_CHILD,
                                                   20, yPos+5, 120, 20, hwnd, NULL, NULL, NULL);
                SendMessage(servers[i].hwndName, WM_SETFONT, (WPARAM)hFont, TRUE);

                servers[i].hwndStatus = CreateWindow("STATIC", "Checking...", WS_VISIBLE | WS_CHILD,
                                                     150, yPos+5, 80, 20, hwnd, NULL, NULL, NULL);
                SendMessage(servers[i].hwndStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

                servers[i].hwndButton = CreateWindow("BUTTON", "Toggle", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                                     250, yPos, 100, 32, hwnd, (HMENU)(INT_PTR)(BTN_ID_BASE + i), NULL, NULL);
                SendMessage(servers[i].hwndButton, WM_SETFONT, (WPARAM)hFont, TRUE);

                servers[i].isBlocked = CheckRuleExists(servers[i].ruleName);
                UpdateServerUI(i);
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            static HBRUSH hBg = NULL;
            if (!hBg) hBg = CreateSolidBrush(RGB(245, 245, 245));
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(30, 30, 30));
            SetBkColor(hdcStatic, RGB(245, 245, 245));
            return (INT_PTR)hBg;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == BTN_ID_BROWSE) {
                if (BrowseForGameExe(hwnd)) {
                    UpdatePathLabel();
                }
            } else if (wmId >= BTN_ID_BASE && wmId < BTN_ID_BASE + MAX_SERVERS) {
                ToggleServerBlock(wmId - BTN_ID_BASE);
            }
            break;
        }
        case WM_DESTROY: {
            HFONT hFont = (HFONT)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            if (hFont) DeleteObject(hFont);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!IsAdmin()) {
        char szPath[MAX_PATH];
        GetModuleFileName(NULL, szPath, MAX_PATH);
        ShellExecute(NULL, "runas", szPath, NULL, NULL, SW_SHOWNORMAL);
        return 0;
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const char CLASS_NAME[] = "OW2BlockerClass";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(245, 245, 245));

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "OW2 Server Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 400,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}