// Scriptor - a custom built text editor.

#include <windows.h>

// Link the Windows libraries we use, so build.bat can stay a single cl call.
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ---- Look & feel (tweak these freely) --------------------------------------
static const COLORREF kBackgroundColor = RGB(45, 45, 45);   // gray background
static const COLORREF kTextColor       = RGB(255, 255, 255); // white text
static const wchar_t*  kFontFace        = L"Consolas";       // a monospace font
static const int       kFontHeight      = 18;                // pixels

// ---- Global handles --------------------------------------------------------
static HWND    g_editControl = nullptr; // the text area
static HFONT   g_font        = nullptr; // monospace font
static HBRUSH  g_bgBrush     = nullptr; // background brush for the edit control

// Create the child EDIT control that fills the window and holds the text.
static void CreateEditor(HWND parent)
{
    g_editControl = CreateWindowExW(
        0,
        L"EDIT",
        L"",
        // Multiline, word-wrap off would need ES_AUTOHSCROLL; we keep wrapping
        // on and give it vertical scrolling. ES_WANTRETURN lets Enter insert a
        // newline instead of dismissing a dialog.
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL | ES_NOHIDESEL,
        0, 0, 0, 0,          // positioned in WM_SIZE
        parent,
        nullptr,
        (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE),
        nullptr);

    // Apply the monospace font.
    g_font = CreateFontW(
        kFontHeight, 0, 0, 0,
        FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, kFontFace);
    SendMessageW(g_editControl, WM_SETFONT, (WPARAM)g_font, TRUE);
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_bgBrush = CreateSolidBrush(kBackgroundColor);
        CreateEditor(hwnd);
        return 0;

    case WM_SIZE:
        // Make the editor fill the entire client area.
        MoveWindow(g_editControl, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        return 0;

    case WM_SETFOCUS:
        // Clicking the window (or alt-tabbing back) should land the caret in
        // the text area, ready to type.
        SetFocus(g_editControl);
        return 0;

    case WM_CTLCOLOREDIT:
    {
        // Paint the edit control with our colors.
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, kTextColor);
        SetBkColor(hdc, kBackgroundColor);
        return (LRESULT)g_bgBrush;
    }

    case WM_DESTROY:
        if (g_font)    DeleteObject(g_font);
        if (g_bgBrush) DeleteObject(g_bgBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    const wchar_t* kClassName = L"ScriptorMainWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor       = LoadCursor(nullptr, IDC_IBEAM);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"Scriptor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 600,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
        return 1;

    ShowWindow(hwnd, nCmdShow);

    // Standard message loop.
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
