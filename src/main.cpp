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
static const COLORREF kBannerColor     = RGB(210, 210, 210); // light-grey toolbar
static const COLORREF kBannerTextColor = RGB(25, 25, 25);   // dark text on banner
static const COLORREF kFieldColor      = RGB(255, 255, 255); // filename field fill
static const COLORREF kSavedColor      = RGB(30, 130, 60);  // "Saved" indicator
static const COLORREF kUnsavedColor    = RGB(190, 55, 55);  // "Unsaved" indicator
static const wchar_t*  kFontFace        = L"Consolas";       // a monospace font
static const int       kFontHeight      = 18;                // pixels
static const int       kToolbarHeight   = 36;                // top toolbar strip

// ---- Control / command IDs -------------------------------------------------
#define ID_SAVE_BUTTON   1001
#define ID_FILENAME_EDIT 1002
#define ID_LOAD_BUTTON   1003
#define ID_SELECT_ALL    1004 // Ctrl+A (accelerator only, no visible control)
#define ID_NEW_FILE      1005 // Ctrl+N (accelerator only, no visible control)

// ---- Global handles --------------------------------------------------------
static HWND    g_editControl  = nullptr; // main text area
static HWND    g_filenameEdit = nullptr; // filename field in the toolbar
static HWND    g_saveButton   = nullptr; // Save button
static HWND    g_loadButton   = nullptr; // Load button
static HWND    g_label        = nullptr; // "File Name:" label
static HWND    g_saveState    = nullptr; // "Saved"/"Unsaved" indicator
static HFONT   g_font         = nullptr; // monospace font
static HBRUSH  g_bgBrush      = nullptr; // dark editor background brush
static HBRUSH  g_bannerBrush  = nullptr; // light-grey toolbar banner brush
static HBRUSH  g_fieldBrush   = nullptr; // filename field background brush

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

    // Toolbar: label + filename field + save-state indicator + Save + Load.
    g_label = CreateWindowExW(
        0, L"STATIC", L"File Name:",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        0, 0, 0, 0, parent, nullptr, inst, nullptr);

    // WS_EX_CLIENTEDGE gives the field a sunken outline so it reads as an
    // editable box against the light-grey banner.
    g_filenameEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, parent, (HMENU)ID_FILENAME_EDIT, inst, nullptr);

    g_saveState = CreateWindowExW(
        0, L"STATIC", L"Saved",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER,
        0, 0, 0, 0, parent, nullptr, inst, nullptr);

    g_saveButton = CreateWindowExW(
        0, L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, parent, (HMENU)ID_SAVE_BUTTON, inst, nullptr);

    g_loadButton = CreateWindowExW(
        0, L"BUTTON", L"Load",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, parent, (HMENU)ID_LOAD_BUTTON, inst, nullptr);

    // Monospace font for the text surfaces.
    g_font = CreateFontW(
        kFontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, kFontFace);
    SendMessageW(g_editControl,  WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageW(g_filenameEdit, WM_SETFONT, (WPARAM)g_font, TRUE);
}

// Refresh the toolbar save-state indicator from the editor's modify flag.
static void UpdateSaveIndicator()
{
    bool dirty = SendMessageW(g_editControl, EM_GETMODIFY, 0, 0) != 0;
    SetWindowTextW(g_saveState, dirty ? L"Unsaved" : L"Saved");
    // Force a repaint so WM_CTLCOLORSTATIC re-picks the text colour.
    InvalidateRect(g_saveState, nullptr, TRUE);
}

// Save the main text area's contents to a .txt file chosen via the native
// Windows save dialog, pre-filled with the toolbar filename. Returns true if
// the file was written, false if the user cancelled or the write failed.
static bool DoSave(HWND hwnd)
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
        return false; // user cancelled

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
        return false;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(file, utf8.data(), (DWORD)utf8.size(),
                        &written, nullptr);
    CloseHandle(file);

    if (!ok)
    {
        MessageBoxW(hwnd, L"Failed to write the file.",
                    L"Scriptor", MB_ICONERROR);
        return false;
    }

    // The document now matches what's on disk, so it's no longer "dirty".
    SendMessageW(g_editControl, EM_SETMODIFY, FALSE, 0);
    UpdateSaveIndicator();
    return true;
}

// Load a .txt file chosen via the native Windows open dialog into the main
// text area, and mirror the chosen name back into the toolbar field.
static void DoLoad(HWND hwnd)
{
    // Seed the dialog buffer with the toolbar filename (may be blank).
    wchar_t filePath[MAX_PATH] = L"";
    GetWindowTextW(g_filenameEdit, filePath, MAX_PATH);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn))
        return; // user cancelled

    HANDLE file = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(hwnd, L"Could not open the file for reading.",
                    L"Scriptor", MB_ICONERROR);
        return;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 0x7FFFFFFF)
    {
        CloseHandle(file);
        MessageBoxW(hwnd, L"The file is too large to open.",
                    L"Scriptor", MB_ICONERROR);
        return;
    }

    // Read the raw bytes (UTF-8 on disk, as written by DoSave).
    std::string utf8((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(file, utf8.empty() ? nullptr : &utf8[0],
                       (DWORD)utf8.size(), &read, nullptr);
    CloseHandle(file);
    if (!ok)
    {
        MessageBoxW(hwnd, L"Failed to read the file.",
                    L"Scriptor", MB_ICONERROR);
        return;
    }
    utf8.resize(read);

    // Skip a UTF-8 byte-order mark if one is present.
    if (utf8.size() >= 3 &&
        (unsigned char)utf8[0] == 0xEF &&
        (unsigned char)utf8[1] == 0xBB &&
        (unsigned char)utf8[2] == 0xBF)
        utf8.erase(0, 3);

    // Convert UTF-8 -> UTF-16 for the edit control.
    std::wstring text;
    if (!utf8.empty())
    {
        int chars = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                        (int)utf8.size(), nullptr, 0);
        text.resize(chars);
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(),
                            &text[0], chars);
    }

    // The EDIT control only breaks lines on CRLF, so normalize any lone LFs
    // (Unix-style files) to CRLF; leave existing CRLFs untouched.
    std::wstring normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == L'\n' && (i == 0 || text[i - 1] != L'\r'))
            normalized.push_back(L'\r');
        normalized.push_back(text[i]);
    }

    SetWindowTextW(g_editControl, normalized.c_str());

    // Keep the toolbar field in sync so a follow-up Save targets this file.
    SetWindowTextW(g_filenameEdit, filePath);

    // A just-loaded document matches disk, so start from a clean state.
    SendMessageW(g_editControl, EM_SETMODIFY, FALSE, 0);
    UpdateSaveIndicator();
}

// Select all text in whichever edit box currently has focus (Ctrl+A): the
// filename field when the caret is there, otherwise the main text area.
// Standard EDIT controls do not bind Ctrl+A themselves, so we drive it.
static void DoSelectAll()
{
    HWND focused = GetFocus();
    HWND target  = (focused == g_filenameEdit) ? g_filenameEdit : g_editControl;
    SendMessageW(target, EM_SETSEL, 0, -1);
}

// Start a fresh document (Ctrl+N): clear the text area and the toolbar
// filename so a following Save prompts for a new destination. If the current
// document has unsaved changes, prompt to save first via the save dialog and
// abort the new-file action if that save is cancelled or fails.
static void DoNew(HWND hwnd)
{
    if (SendMessageW(g_editControl, EM_GETMODIFY, 0, 0))
    {
        if (!DoSave(hwnd))
            return; // keep the current document if the save didn't complete
    }

    SetWindowTextW(g_editControl, L"");
    SetWindowTextW(g_filenameEdit, L"");
    SendMessageW(g_editControl, EM_SETMODIFY, FALSE, 0);
    UpdateSaveIndicator();
    SetFocus(g_editControl);
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

        const int margin  = 10;
        const int btnW    = 72, btnH = 24, rowY = 6;
        const int indW    = 84, indH = 22, indY = 7;
        const int labelW  = 78, labelH = 20, labelY = 8;

        // Right-aligned group: Load at the far right, then Save, then the
        // save-state indicator just to the left of the Save button.
        int loadX  = w - margin - btnW;
        int saveX  = loadX - margin - btnW;
        int indX   = saveX - margin - indW;

        // Left group: label, then the filename field stretching to fill the
        // gap up to the indicator so nothing clusters on the left.
        int labelX = margin;
        int editX  = labelX + labelW + 6;
        int editW  = indX - margin - editX;
        if (editW < 80) editW = 80; // keep the field usable when very narrow

        MoveWindow(g_label,        labelX, labelY, labelW, labelH, TRUE);
        MoveWindow(g_filenameEdit, editX,  rowY,   editW,  22,     TRUE);
        MoveWindow(g_saveState,    indX,   indY,   indW,   indH,   TRUE);
        MoveWindow(g_saveButton,   saveX,  rowY,   btnW,   btnH,   TRUE);
        MoveWindow(g_loadButton,   loadX,  rowY,   btnW,   btnH,   TRUE);

        // Main editor fills everything below the toolbar.
        MoveWindow(g_editControl, 0, kToolbarHeight, w, h - kToolbarHeight, TRUE);

        // Repaint the banner strip so no stale pixels linger where controls
        // moved as the window resized.
        RECT banner = { 0, 0, w, kToolbarHeight };
        InvalidateRect(hwnd, &banner, TRUE);
        return 0;
    }

    case WM_COMMAND:
        // Editor text changed: reflect the new dirty state in the indicator.
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_editControl)
        {
            UpdateSaveIndicator();
            return 0;
        }
        // These commands arrive both from button clicks (BN_CLICKED) and from
        // the accelerator table (notification code 1), so route on the ID.
        switch (LOWORD(wParam))
        {
        case ID_SAVE_BUTTON:  DoSave(hwnd);  return 0;
        case ID_LOAD_BUTTON:  DoLoad(hwnd);  return 0;
        case ID_SELECT_ALL:   DoSelectAll(); return 0;
        case ID_NEW_FILE:     DoNew(hwnd);   return 0;
        }
        break;

    case WM_SETFOCUS:
        SetFocus(g_editControl);
        return 0;

    case WM_ERASEBKGND:
    {
        // Paint the top strip as the light-grey toolbar banner and the rest
        // as the dark editor background. (The editor control repaints its own
        // area, so this mainly gives the toolbar its banner.)
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        RECT banner = { rc.left, rc.top, rc.right, kToolbarHeight };
        RECT below  = { rc.left, kToolbarHeight, rc.right, rc.bottom };
        FillRect(hdc, &banner, g_bannerBrush);
        FillRect(hdc, &below,  g_bgBrush);
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    {
        // The label and save-state indicator sit on the light-grey banner.
        HDC  hdc = (HDC)wParam;
        HWND ctl = (HWND)lParam;
        SetBkColor(hdc, kBannerColor);
        if (ctl == g_saveState)
        {
            bool dirty = SendMessageW(g_editControl, EM_GETMODIFY, 0, 0) != 0;
            SetTextColor(hdc, dirty ? kUnsavedColor : kSavedColor);
        }
        else
        {
            SetTextColor(hdc, kBannerTextColor);
        }
        return (LRESULT)g_bannerBrush;
    }

    case WM_CTLCOLOREDIT:
    {
        // The filename field is a light input box on the banner; the main
        // editor keeps its dark theme.
        HDC  hdc = (HDC)wParam;
        HWND ctl = (HWND)lParam;
        if (ctl == g_filenameEdit)
        {
            SetTextColor(hdc, kBannerTextColor);
            SetBkColor(hdc, kFieldColor);
            return (LRESULT)g_fieldBrush;
        }
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

    g_bgBrush     = CreateSolidBrush(kBackgroundColor);
    g_bannerBrush = CreateSolidBrush(kBannerColor);
    g_fieldBrush  = CreateSolidBrush(kFieldColor);

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

    // Keyboard shortcuts: map Ctrl+key to the matching WM_COMMAND IDs.
    ACCEL accels[] = {
        { FCONTROL | FVIRTKEY, 'A', ID_SELECT_ALL  },
        { FCONTROL | FVIRTKEY, 'S', ID_SAVE_BUTTON },
        { FCONTROL | FVIRTKEY, 'O', ID_LOAD_BUTTON },
        { FCONTROL | FVIRTKEY, 'N', ID_NEW_FILE    },
    };
    HACCEL hAccel = CreateAcceleratorTableW(accels, ARRAYSIZE(accels));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (!TranslateAcceleratorW(hwnd, hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (hAccel) DestroyAcceleratorTable(hAccel);
    DeleteObject(g_bgBrush);
    DeleteObject(g_bannerBrush);
    DeleteObject(g_fieldBrush);
    return (int)msg.wParam;
}
