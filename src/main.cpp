// Scriptor - a custom built text editor.

#include <windows.h>
#include <commdlg.h>
#include <string>

// Link the Windows libraries we use, so build.bat can stay a single cl call.
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

// ---- Look & feel (tweak these freely) --------------------------------------
static const COLORREF kBackgroundColor = RGB(45, 45, 45);   // gray background
static const COLORREF kTextColor       = RGB(255, 255, 255); // white text
static const wchar_t*  kFontFace        = L"Consolas";       // a monospace font
static const int       kFontHeight      = 18;                // pixels
static const int       kToolbarHeight   = 36;                // top toolbar strip

// ---- Control IDs -----------------------------------------------------------
#define ID_SAVE_BUTTON   1001
#define ID_FILENAME_EDIT 1002

// ---- Global handles --------------------------------------------------------
static HWND    g_editControl  = nullptr; // main text area
static HWND    g_filenameEdit = nullptr; // filename field in the toolbar
static HWND    g_saveButton   = nullptr; // Save button
static HWND    g_label        = nullptr; // "File name:" label
static HFONT   g_font         = nullptr; // monospace font
static HBRUSH  g_bgBrush      = nullptr; // background brush

// Create the main text area plus the toolbar controls.
static void CreateControls(HWND parent)
{
    HINSTANCE inst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);

    // Main text area: multiline, click-to-position cursor comes for free.
    g_editControl = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_NOHIDESEL,
        0, 0, 0, 0, parent, nullptr, inst, nullptr);

    // Toolbar: label + filename field + Save button.
    g_label = CreateWindowExW(
        0, L"STATIC", L"File name:",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        0, 0, 0, 0, parent, nullptr, inst, nullptr);

    g_filenameEdit = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, parent, (HMENU)ID_FILENAME_EDIT, inst, nullptr);

    g_saveButton = CreateWindowExW(
        0, L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, parent, (HMENU)ID_SAVE_BUTTON, inst, nullptr);

    // Monospace font for the text surfaces.
    g_font = CreateFontW(
        kFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, kFontFace);
    SendMessageW(g_editControl,  WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageW(g_filenameEdit, WM_SETFONT, (WPARAM)g_font, TRUE);
}

// Save the main text area's contents to a .txt file chosen via the native
// Windows save dialog, pre-filled with the toolbar filename.
static void DoSave(HWND hwnd)
{
    // Seed the dialog's buffer with whatever the user typed in the toolbar.
    wchar_t filePath[MAX_PATH] = L"";
    GetWindowTextW(g_filenameEdit, filePath, MAX_PATH);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrDefExt = L"txt";  // append .txt if the user omits an extension
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&ofn))
        return; // user cancelled

    // Read the full contents of the main text area.
    int len = GetWindowTextLengthW(g_editControl);
    std::wstring text(len, L'\0');
    if (len > 0)
        GetWindowTextW(g_editControl, &text[0], len + 1);

    // Convert UTF-16 -> UTF-8 for the file on disk.
    std::string utf8;
    if (len > 0)
    {
        int bytes = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), len,
                                        nullptr, 0, nullptr, nullptr);
        utf8.resize(bytes);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), len,
                            &utf8[0], bytes, nullptr, nullptr);
    }

    HANDLE file = CreateFileW(filePath, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(hwnd, L"Could not open the file for writing.",
                    L"Scriptor", MB_ICONERROR);
        return;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(file, utf8.data(), (DWORD)utf8.size(),
                        &written, nullptr);
    CloseHandle(file);

    if (!ok)
        MessageBoxW(hwnd, L"Failed to write the file.",
                    L"Scriptor", MB_ICONERROR);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        CreateControls(hwnd);
        return 0;

    case WM_SIZE:
    {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        // Toolbar controls along the top strip.
        MoveWindow(g_label,        8,  10,  64, 20, TRUE);
        MoveWindow(g_filenameEdit, 76,  7, 200, 22, TRUE);
        MoveWindow(g_saveButton,   284, 7,  72, 24, TRUE);
        // Main editor fills everything below the toolbar.
        MoveWindow(g_editControl, 0, kToolbarHeight, w, h - kToolbarHeight, TRUE);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_SAVE_BUTTON && HIWORD(wParam) == BN_CLICKED)
        {
            DoSave(hwnd);
            return 0;
        }
        break;

    case WM_SETFOCUS:
        SetFocus(g_editControl);
        return 0;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
    {
        // Theme the edit controls and the label to match the dark background.
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, kTextColor);
        SetBkColor(hdc, kBackgroundColor);
        return (LRESULT)g_bgBrush;
    }

    case WM_DESTROY:
        if (g_font) DeleteObject(g_font);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t* kClassName = L"ScriptorMainWindow";

    g_bgBrush = CreateSolidBrush(kBackgroundColor);

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bgBrush;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, kClassName, L"Scriptor", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
        return 1;

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_bgBrush);
    return (int)msg.wParam;
}
