#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#define UNICODE             // Use Unicode APIs
#define _UNICODE            // Use Unicode C runtime library functions

#include <windows.h>
#include <shellapi.h> // For system tray
#include <shlwapi.h>  // For StrStrIW
#include <wchar.h>    // For wide char functions like _wcsdup, wcscpy_s
#include <stdbool.h>
#include <stdio.h>
#include "resource.h" // Assuming this contains your ICON IDs (IDI_MYICON_BIG, etc.)

// --- Constants ---
#define MAX_HISTORY 128       // TODO: Make this configurable
#define IDC_SEARCH_EDIT 1001
#define IDC_LISTBOX 1002
#define TIMER_ID_FLASH 1      // Timer for flashing background
#define WM_TRAY_ICON (WM_APP + 1) // Message for tray icon events
#define TRAY_ICON_ID 101      // ID for the tray icon itself
#define HOTKEY_ID_TOGGLE 1    // ID for the Alt+; hotkey
#define IDM_ABOUT 10001       // Menu item ID for About

// --- Global Variables ---
HWND hwndList = NULL;
HWND hwndEdit = NULL;
HWND hMainWnd = NULL; // Store main window handle

wchar_t* clipboardHistory[MAX_HISTORY] = {0}; // Store wide strings
int currentHistoryIndex = 0; // Index for circular buffer
size_t historyItemCount = 0; // Number of valid items currently in history

// System Tray
NOTIFYICONDATAW nid = { sizeof(NOTIFYICONDATAW) }; // Use W version
bool windowRestored = TRUE; // Tracks if window is visible or hidden

// Subclassing
WNDPROC g_pOldListProc = NULL;
WNDPROC g_pOldEditProc = NULL;

// GDI Resources
HBRUSH g_hBrushBackground = NULL;
HBRUSH g_hBrushFlash = NULL;
COLORREF g_colorBackground = RGB(235, 237, 202); // Default background
COLORREF g_colorFlash = RGB(255, 100, 100);    // Flash background (less harsh red)
bool g_isFlashing = false;

// Icon Handles
HICON hIconBig = NULL;
HICON hIconSmall = NULL;
HICON hIconTray = NULL;

// --- Function Prototypes ---
void DisplayLastError(const wchar_t *functionName);
void ShowAboutDialog(HWND hwnd);
bool EntryExistsW(const wchar_t *str1, const wchar_t *history[]);
void UpdateListBox(HWND hwndListBox, const wchar_t* searchFilter);
void AddClipboardEntry(HWND hwnd, LPCWSTR clipboardText);
void OnKeyDownHandler(HWND hwnd, WPARAM wParam);
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool InitializeResources(HINSTANCE hInstance, HWND hwnd);
void CleanupResources();
void SetClipboardText(HWND hwndOwner, const wchar_t* text);
void ToggleWindowVisibility(HWND hwnd);


// --- Error Handling ---
void
DisplayLastError(const wchar_t *functionName)
{
    DWORD errorCode = GetLastError();
    LPWSTR errorText = NULL; // Use LPWSTR for Unicode

    FormatMessageW( // Use W version
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
        (LPWSTR)&errorText, // Cast to LPWSTR
        0,
        NULL
    );

    wchar_t buffer[512];
    if (errorText != NULL) {
        swprintf_s(buffer, _countof(buffer), L"Error in function %s: %s (Code: %lu)", functionName, errorText, errorCode);
        LocalFree(errorText);
    } else {
        swprintf_s(buffer, _countof(buffer), L"Error in function %s: Unknown error (Code: %lu)", functionName, errorCode);
    }
    // Output to debugger instead of console which might not be available
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");

    // Optional: Show a message box for critical errors
    // MessageBoxW(NULL, buffer, L"Runtime Error", MB_OK | MB_ICONERROR);
}

// --- About Dialog ---
void
ShowAboutDialog(HWND hwnd)
{
    MessageBoxW(hwnd,
               L"mclip - Clipboard History App\n\nAuthor: Ilija Tatalovic\nVersion: 0.5.0 (Refactored)\nLicence: MIT",
               L"About mclip",
               MB_OK | MB_ICONINFORMATION);
}


// --- History Management ---

// Checks if an entry already exists (case-insensitive). Searches backwards for efficiency.
bool
EntryExistsW(const wchar_t *str1, const wchar_t *history[])
{
    if (!str1 || wcslen(str1) == 0) return true; // Don't add empty strings implicitly

    int checkedCount = 0;
    int idx = (currentHistoryIndex + MAX_HISTORY - 1) % MAX_HISTORY; // Start from last added item

    while(checkedCount < historyItemCount) {
         if (history[idx] != NULL) {
            if (_wcsicmp(str1, history[idx]) == 0) { // Case-insensitive compare
                return true;
            }
        }
        idx = (idx + MAX_HISTORY - 1) % MAX_HISTORY; // Move backwards circularly
        checkedCount++;
    }
    return false;
}


// Adds a new entry to the clipboard history (if it's new)
void AddClipboardEntry(HWND hwnd, LPCWSTR clipboardText) {
    if (clipboardText == NULL || wcslen(clipboardText) == 0) {
        return; // Don't add empty strings
    }

    // Check if entry already exists
    if (!EntryExistsW(clipboardText, (const wchar_t**)clipboardHistory)) {
        // Free the oldest entry if the buffer is full
        if (historyItemCount == MAX_HISTORY && clipboardHistory[currentHistoryIndex]) {
            free(clipboardHistory[currentHistoryIndex]);
            clipboardHistory[currentHistoryIndex] = NULL; // Avoid double free if _wcsdup fails
        } else if (historyItemCount < MAX_HISTORY) {
            // Only increment count if we are not overwriting
            historyItemCount++;
        }

        // Duplicate the string
        clipboardHistory[currentHistoryIndex] = _wcsdup(clipboardText);
        if (!clipboardHistory[currentHistoryIndex]) {
            DisplayLastError(L"AddClipboardEntry _wcsdup");
            // Decrement count if allocation failed and we were adding a new item
             if (historyItemCount > 0 && historyItemCount <= MAX_HISTORY) {
                 historyItemCount--;
             }
            MessageBoxW(hwnd, L"Failed to allocate memory for new clipboard entry.", L"Error", MB_OK | MB_ICONERROR);
            return; // Stop processing this entry
        }

        // Move to the next index (circularly)
        currentHistoryIndex = (currentHistoryIndex + 1) % MAX_HISTORY;

        // Update the list box (only if the window is visible or search isn't active?)
        // For simplicity, always update if possible. Could optimize later.
        wchar_t currentSearch[256] = {0};
        if(hwndEdit) GetWindowTextW(hwndEdit, currentSearch, _countof(currentSearch));
        UpdateListBox(hwndList, currentSearch);

    }
}

// --- UI Update ---

// Updates the listbox based on history and optional filter
void
UpdateListBox(HWND hwndListBox, const wchar_t* searchFilter)
{
    if (!hwndListBox) return;

    SendMessageW(hwndListBox, WM_SETREDRAW, FALSE, 0); // Disable redrawing
    SendMessageW(hwndListBox, LB_RESETCONTENT, 0, 0); // Clear the listbox

    bool hasFilter = (searchFilter != NULL && searchFilter[0] != L'\0');

    int addedCount = 0;
    int historyIdx = (currentHistoryIndex + MAX_HISTORY - 1) % MAX_HISTORY; // Start from last added

    // Iterate backwards through the valid history items
    for (size_t i = 0; i < historyItemCount; ++i) {
        if (clipboardHistory[historyIdx] != NULL) {
            bool match = true;
            if (hasFilter) {
                // Case-insensitive search using StrStrIW
                match = (StrStrIW(clipboardHistory[historyIdx], searchFilter) != NULL);
            }

            if (match) {
                // Insert strings at the top (index 0) to maintain newest-first order
                SendMessageW(hwndListBox, LB_INSERTSTRING, 0, (LPARAM)clipboardHistory[historyIdx]);
                addedCount++;
            }
        }
        historyIdx = (historyIdx + MAX_HISTORY - 1) % MAX_HISTORY; // Move backwards circularly
    }

    SendMessageW(hwndListBox, WM_SETREDRAW, TRUE, 0); // Enable redrawing
    InvalidateRect(hwndListBox, NULL, TRUE); // Force repaint

     // Optionally, select the first item if any were added
    if (addedCount > 0) {
         SendMessageW(hwndListBox, LB_SETCURSEL, 0, 0);
    }
}

// --- Resource Management ---

bool InitializeResources(HINSTANCE hInstance, HWND hwnd) {
    // Load Icons
    hIconBig = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_MYICON_BIG));
    hIconSmall = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_MYICON_SMALL));
    hIconTray = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_MYTRAY_ICON));

    if (!hIconBig || !hIconSmall || !hIconTray) {
        DisplayLastError(L"LoadIconW");
        MessageBoxW(hwnd, L"Failed to load application icons.", L"Initialization Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // Send icons to the window
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    // Setup Tray Icon Data (using W version)
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = hwnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_ICON;
    nid.hIcon = hIconTray;
    nid.uVersion = NOTIFYICON_VERSION_4; // Use newer version for better behavior
    // Use wcscpy_s for safety
    wcscpy_s(nid.szTip, _countof(nid.szTip), L"mclip");

    // Add tray icon
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
         DisplayLastError(L"Shell_NotifyIconW NIM_ADD");
         // Non-fatal, maybe? Continue but log error.
    }
     // Set version
    if (!Shell_NotifyIconW(NIM_SETVERSION, &nid)) {
         DisplayLastError(L"Shell_NotifyIconW NIM_SETVERSION");
    }


    // Create GDI Brushes
    g_hBrushBackground = CreateSolidBrush(g_colorBackground);
    g_hBrushFlash = CreateSolidBrush(g_colorFlash);

    if (!g_hBrushBackground || !g_hBrushFlash) {
        DisplayLastError(L"CreateSolidBrush");
        MessageBoxW(hwnd, L"Failed to create GDI resources.", L"Initialization Error", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

void CleanupResources() {
    // Free history strings
    for (int i = 0; i < MAX_HISTORY; ++i) {
        if (clipboardHistory[i]) {
            free(clipboardHistory[i]);
            clipboardHistory[i] = NULL;
        }
    }

    // Destroy GDI Objects
    if (g_hBrushBackground) DeleteObject(g_hBrushBackground);
    if (g_hBrushFlash) DeleteObject(g_hBrushFlash);

    // Destroy Icons (only if loaded successfully)
    // Note: System manages icons set with WM_SETICON, but tray icon needs care?
    // Let's destroy the ones we loaded explicitly just to be safe.
    if (hIconBig) DestroyIcon(hIconBig);
    if (hIconSmall) DestroyIcon(hIconSmall);
    if (hIconTray) DestroyIcon(hIconTray);

    // Remove tray icon explicitly on exit
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

// --- Clipboard Interaction ---
void SetClipboardText(HWND hwndOwner, const wchar_t* text) {
    if (!text) return;
    size_t textLen = wcslen(text);
    if (textLen == 0) return;

    if (!OpenClipboard(hwndOwner)) {
        DisplayLastError(L"SetClipboardText OpenClipboard");
        return;
    }

    if (!EmptyClipboard()) {
         DisplayLastError(L"SetClipboardText EmptyClipboard");
         CloseClipboard();
         return;
    }

    // Allocate global memory for the text
    // Use GMEM_MOVEABLE as recommended for SetClipboardData
    HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, (textLen + 1) * sizeof(wchar_t));
    if (hClipboardData == NULL) {
        DisplayLastError(L"SetClipboardText GlobalAlloc");
        CloseClipboard();
        return;
    }

    // Lock the memory and copy the string
    LPWSTR clipboardPtr = (LPWSTR)GlobalLock(hClipboardData);
    if (clipboardPtr == NULL) {
        DisplayLastError(L"SetClipboardText GlobalLock");
        GlobalFree(hClipboardData); // Free if lock fails
        CloseClipboard();
        return;
    }

    wcscpy_s(clipboardPtr, textLen + 1, text); // Safe copy
    GlobalUnlock(hClipboardData);

    // Set the clipboard data
    if (!SetClipboardData(CF_UNICODETEXT, hClipboardData)) {
        DisplayLastError(L"SetClipboardText SetClipboardData");
        GlobalFree(hClipboardData); // MUST free if SetClipboardData fails
    }
    // If SetClipboardData succeeds, the system now owns hClipboardData.

    CloseClipboard();
}


// --- Window Management ---
void ToggleWindowVisibility(HWND hwnd) {
     if (!windowRestored) { // If hidden, show it
        ShowWindow(hwnd, SW_SHOW); // Use SW_SHOW instead of SW_RESTORE if it might be minimized
        SetForegroundWindow(hwnd); // Bring to front
        if (hwndEdit) SetFocus(hwndEdit);   // Focus the search box
        windowRestored = true;
    } else { // If shown, hide it
        ShowWindow(hwnd, SW_HIDE);
        windowRestored = false;
    }
}

// --- Event Handlers ---

// Handles key presses primarily for list navigation and copying
void OnKeyDownHandler(HWND hwnd, WPARAM wParam)
{
    HWND focusedWnd = GetFocus();
    int selectedIndex = SendMessageW(hwndList, LB_GETCURSEL, 0, 0);
    int itemCount = SendMessageW(hwndList, LB_GETCOUNT, 0, 0);

    switch (wParam) {
        case VK_UP:
             if (focusedWnd == hwndList) {
                 if (selectedIndex > 0) {
                     SendMessageW(hwndList, LB_SETCURSEL, selectedIndex - 1, 0);
                 }
             } else {
                 // If focus is elsewhere (e.g., edit box) and UP is pressed,
                 // move focus to list and select last item if possible.
                 if (itemCount > 0) {
                     SendMessageW(hwndList, LB_SETCURSEL, itemCount - 1, 0);
                     SetFocus(hwndList);
                 }
             }
            break;

        case VK_DOWN:
             if (focusedWnd == hwndList) {
                  if (selectedIndex != LB_ERR && selectedIndex < itemCount - 1) {
                     SendMessageW(hwndList, LB_SETCURSEL, selectedIndex + 1, 0);
                 }
             } else {
                  // If focus is elsewhere (e.g., edit box) and DOWN is pressed,
                  // move focus to list and select first item if possible.
                  if (itemCount > 0) {
                     SendMessageW(hwndList, LB_SETCURSEL, 0, 0);
                     SetFocus(hwndList);
                 }
             }
            break;

        case VK_RETURN: // Enter key - copies selected item to clipboard
            if (focusedWnd == hwndList && selectedIndex != LB_ERR) {
                int itemLengthW = SendMessageW(hwndList, LB_GETTEXTLEN, selectedIndex, 0);
                if (itemLengthW != LB_ERR && itemLengthW > 0) {
                    wchar_t *bufferW = malloc((itemLengthW + 1) * sizeof(wchar_t));
                    if (bufferW) {
                        SendMessageW(hwndList, LB_GETTEXT, selectedIndex, (LPARAM)bufferW);
                        SetClipboardText(hwnd, bufferW); // Use helper function
                        free(bufferW);
                        // Optionally hide window after selection
                        // ToggleWindowVisibility(hwnd);
                    } else {
                         DisplayLastError(L"OnKeyDownHandler malloc");
                         MessageBoxW(hwnd, L"Failed to allocate memory to copy selection.", L"Error", MB_OK | MB_ICONERROR);
                    }
                }
            } else if (focusedWnd == hwndEdit) {
                 // Optional: If enter is pressed in edit box, maybe select first match in list?
                 if (itemCount > 0) {
                     SendMessageW(hwndList, LB_SETCURSEL, 0, 0);
                     SetFocus(hwndList);
                     // Could optionally copy it immediately too by replicating the listbox logic above
                 }
            }
            break;

        // Note: Ctrl+P removed as it seemed like debug code.
        // Tab is handled by subclassing.
        // Esc could be used to hide the window?
        case VK_ESCAPE:
             ToggleWindowVisibility(hwnd);
             break;

        default:
            // Handle other keys if needed
            break;
    }
}

// Subclass procedure for the Edit control
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_KEYDOWN:
            if (wParam == VK_TAB) {
                // Shift+Tab could go backwards, but simple Tab goes to List
                if (hwndList) SetFocus(hwndList);
                return 0; // Handled
            }
            // Let OnKeyDownHandler handle Up/Down/Enter/Esc in the main WndProc
            // if ((wParam == VK_UP || wParam == VK_DOWN || wParam == VK_RETURN || wParam == VK_ESCAPE)) {
            //    SendMessage(GetParent(hwnd), message, wParam, lParam); // Forward relevant keys? Or handle directly in OnKeyDownHandler
            //    return 0;
            // }
             break;

         case WM_GETDLGCODE:
             // Ensure Edit control processes arrow keys itself if needed,
             // and Enter key for potential actions (like triggering search/select)
             return CallWindowProcW(g_pOldEditProc, hwnd, message, wParam, lParam) | DLGC_WANTARROWS | DLGC_WANTCHARS;


        // Add other cases as needed
    }
    // Call the original window procedure for any messages not handled
    return CallWindowProcW(g_pOldEditProc, hwnd, message, wParam, lParam);
}

// Subclass procedure for the ListBox control
LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_KEYDOWN:
            if (wParam == VK_TAB) {
                 if (hwndEdit) SetFocus(hwndEdit);
                return 0; // Handled
            }
             // Let OnKeyDownHandler handle Up/Down/Enter/Esc in the main WndProc
             // if ((wParam == VK_UP || wParam == VK_DOWN || wParam == VK_RETURN || wParam == VK_ESCAPE)) {
             //   SendMessage(GetParent(hwnd), message, wParam, lParam); // Forward to parent
             //   return 0;
             //}
            break;

         case WM_GETDLGCODE:
            // Ensure ListBox processes arrow keys for selection
            return CallWindowProcW(g_pOldListProc, hwnd, message, wParam, lParam) | DLGC_WANTARROWS;

        // Add other cases as needed
    }
    // Call the original window procedure for any messages not handled
    return CallWindowProcW(g_pOldListProc, hwnd, message, wParam, lParam);
}


// Main Window Procedure
LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_CREATE:
        {
            hMainWnd = hwnd; // Store main window handle
            HINSTANCE hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

            // Initialize Icons, Brushes, Tray Icon
            if (!InitializeResources(hInstance, hwnd)) {
                // Critical failure during resource initialization
                return -1; // Prevent window creation
            }

            // Create Menu Bar
            HMENU hMenu = CreateMenu();
            HMENU hSubMenuHelp = CreatePopupMenu();
            if (!hMenu || !hSubMenuHelp) {
                 DisplayLastError(L"CreateMenu/CreatePopupMenu");
                 return -1; // Fail creation
            }
            // Use defined ID
            AppendMenuW(hSubMenuHelp, MF_STRING, IDM_ABOUT, L"&About"); // Use W version
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMenuHelp, L"&Help"); // Use W version
            if (!SetMenu(hwnd, hMenu)) {
                 DisplayLastError(L"SetMenu");
                 // Non-fatal? Menu might just not appear.
            }

            // Create ListBox (Use W version of class name)
            hwndList = CreateWindowW(L"LISTBOX", NULL,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS, // Added LBS_HASSTRINGS
                                    10, 10, 360, 210, // Adjusted height slightly
                                    hwnd, (HMENU)IDC_LISTBOX, hInstance, NULL);
            if (!hwndList) {
                DisplayLastError(L"CreateWindowW LISTBOX"); return -1;
            }

            // Create Edit Control (Use W version of class name)
            hwndEdit = CreateWindowW(L"EDIT", NULL,
                                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_LEFT,
                                    10, 225, 360, 25, // Adjusted Y position
                                    hwnd, (HMENU)IDC_SEARCH_EDIT, hInstance, NULL);
             if (!hwndEdit) {
                 DisplayLastError(L"CreateWindowW EDIT"); return -1;
             }

             // Set Font (Optional but recommended for consistency)
             HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
             SendMessageW(hwndList, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
             SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));


            // Subclass Edit and ListBox for Tab navigation etc.
            g_pOldEditProc = (WNDPROC)SetWindowLongPtrW(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            g_pOldListProc = (WNDPROC)SetWindowLongPtrW(hwndList, GWLP_WNDPROC, (LONG_PTR)ListSubclassProc);

            if (g_pOldEditProc == NULL || g_pOldListProc == NULL) {
                DisplayLastError(L"SetWindowLongPtrW (Subclassing)");
                MessageBoxW(hwnd, L"Failed to subclass controls.", L"Error", MB_OK | MB_ICONERROR);
                // Potentially non-fatal, Tab might just not work as expected
            }

             // Add initial items to listbox
             UpdateListBox(hwndList, NULL);

             // Set initial focus
             SetFocus(hwndEdit);

             // Listen for clipboard changes
             if (!AddClipboardFormatListener(hwnd)) {
                 DisplayLastError(L"AddClipboardFormatListener");
                  MessageBoxW(hwnd, L"Failed to register clipboard listener.", L"Error", MB_OK | MB_ICONERROR);
                  // Potentially fatal depending on requirements
             }
        }
        break; // End WM_CREATE

        case WM_TIMER:
            if (wParam == TIMER_ID_FLASH) {
                KillTimer(hwnd, TIMER_ID_FLASH); // Stop the timer
                g_isFlashing = false;
                InvalidateRect(hwndEdit, NULL, TRUE); // Redraw Edit with original color
                // Optional: Redraw main window background too if it was also flashing
                // InvalidateRect(hwnd, NULL, TRUE);
            }
            break;

        case WM_KEYDOWN:
            OnKeyDownHandler(hwnd, wParam);
            // Don't return 0 here, let DefWindowProc handle system keys if not processed by OnKeyDownHandler
             break;

        case WM_HOTKEY:
            if (wParam == HOTKEY_ID_TOGGLE) {
                ToggleWindowVisibility(hwnd);
            }
            break;

        case WM_CTLCOLOREDIT:
             // Color the background of the Edit control
            {
                HDC hdcEdit = (HDC)wParam;
                if ((HWND)lParam == hwndEdit) { // Check if it's our edit control
                    if (g_isFlashing) {
                        SetBkColor(hdcEdit, g_colorFlash);
                        return (LRESULT)g_hBrushFlash;
                    } else {
                        SetBkColor(hdcEdit, g_colorBackground);
                        return (LRESULT)g_hBrushBackground;
                    }
                }
                 // For other edit controls (if any), use default processing
                 return DefWindowProcW(hwnd, msg, wParam, lParam);
            }
            break;

         // Optional: Handle main window background color if desired
         // case WM_CTLCOLORSTATIC: // For static controls, or main dialog background
         // case WM_ERASEBKGND:
         case WM_PAINT: // Basic background painting for main window gaps
             {
                 PAINTSTRUCT ps;
                 HDC hdc = BeginPaint(hwnd, &ps);
                 // Use the background brush, could also flash main window if desired
                  FillRect(hdc, &ps.rcPaint, g_hBrushBackground);
                 EndPaint(hwnd, &ps);
             }
             return 0; // We handled painting


        case WM_TRAY_ICON: // Message received from the tray icon
            if (wParam == TRAY_ICON_ID) {
                 // Check the mouse message
                 switch (LOWORD(lParam)) {
                    case WM_LBUTTONUP: // Left click
                        ToggleWindowVisibility(hwnd);
                        break;

                    // Could add WM_RBUTTONUP for a context menu
                     case WM_RBUTTONUP:
                         // TODO: Show context menu (e.g., "About", "Exit")
                          break;
                 }
            }
            break;

        case WM_SYSCOMMAND:
            if (wParam == SC_MINIMIZE) {
                // Hide window instead of minimizing
                ShowWindow(hwnd, SW_HIDE);
                windowRestored = false;
                // No need to re-add tray icon if it's already there
                return 0; // Handled
            }
             // Important: Let default processing handle other commands (like SC_CLOSE)
            return DefWindowProcW(hwnd, msg, wParam, lParam);


        case WM_CLIPBOARDUPDATE:
            // Check format availability and attempt to open clipboard
             if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
                int retryCount = 0;
                const int maxRetries = 5;
                const int retryDelayMs = 50;

                while (retryCount < maxRetries) {
                    if (OpenClipboard(hwnd)) {
                        HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
                        if (hClipboardData != NULL) {
                             LPCWSTR clipboardText = (LPCWSTR)GlobalLock(hClipboardData);
                             if (clipboardText != NULL) {
                                 // Process and add the entry (includes duplicate check)
                                 AddClipboardEntry(hwnd, clipboardText);
                                 GlobalUnlock(hClipboardData);
                             } else {
                                 DisplayLastError(L"WM_CLIPBOARDUPDATE GlobalLock");
                             }
                        } else {
                             // GetClipboardData returning NULL might be okay if format changed quickly
                             // DisplayLastError(L"WM_CLIPBOARDUPDATE GetClipboardData");
                        }
                        CloseClipboard();
                        break; // Success, exit retry loop
                    } else {
                        // Failed to open clipboard
                         DWORD errorCode = GetLastError();
                        if (errorCode == ERROR_ACCESS_DENIED) {
                            retryCount++;
                            if (retryCount < maxRetries) {
                                Sleep(retryDelayMs); // Wait before retrying
                            } else {
                                // Max retries reached - indicate error visually
                                if (!g_isFlashing) { // Flash only if not already flashing
                                     g_isFlashing = true;
                                     InvalidateRect(hwndEdit, NULL, TRUE); // Redraw edit background
                                     // Optional: Flash main window background too
                                     // InvalidateRect(hwnd, NULL, TRUE);
                                     SetTimer(hwnd, TIMER_ID_FLASH, 1000, NULL); // Timer to stop flash

                                     // Optional: Flash taskbar icon (can be annoying)
                                     // FLASHWINFO flashInfo = { sizeof(FLASHWINFO) };
                                     // flashInfo.hwnd = hwnd;
                                     // flashInfo.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG;
                                     // flashInfo.uCount = 3;
                                     // flashInfo.dwTimeout = 0;
                                     // FlashWindowEx(&flashInfo);

                                      // Play sound
                                     Beep(750, 300);
                                }
                            }
                        } else {
                            // Different error opening clipboard
                            DisplayLastError(L"WM_CLIPBOARDUPDATE OpenClipboard");
                            break; // Don't retry on unexpected errors
                        }
                    }
                } // End retry loop
             }
            break; // End WM_CLIPBOARDUPDATE

        case WM_COMMAND:
            {
                WORD controlId = LOWORD(wParam);
                WORD notificationCode = HIWORD(wParam);
                HWND hwndCtl = (HWND)lParam;

                switch (controlId) {
                    case IDM_ABOUT: // Menu item
                        ShowAboutDialog(hwnd);
                        break;

                    case IDC_SEARCH_EDIT:
                         if (notificationCode == EN_CHANGE) {
                            wchar_t searchText[256] = {0};
                            GetWindowTextW(hwndCtl, searchText, _countof(searchText));
                            UpdateListBox(hwndList, searchText); // Pass filter to update function
                         }
                        break;

                     // Optional: Handle LBN_DBLCLK on listbox to copy and maybe hide
                     case IDC_LISTBOX:
                         if (notificationCode == LBN_DBLCLK) {
                              OnKeyDownHandler(hwnd, VK_RETURN); // Simulate Enter press logic
                              // Maybe hide the window after double click?
                              // ToggleWindowVisibility(hwnd);
                         }
                          // Note: LBN_SELCHANGE to copy automatically removed as it can be annoying.
                          // User must explicitly press Enter or double-click.
                         break;
                }
            }
            break; // End WM_COMMAND

        case WM_CLOSE:
            // Trigger cleanup and initiate exit
             DestroyWindow(hwnd); // This will eventually lead to WM_DESTROY
             break;

         case WM_NCDESTROY:
             // Good place to unsubclass if needed, though often okay if window is truly destroyed
             if (g_pOldEditProc) SetWindowLongPtrW(hwndEdit, GWLP_WNDPROC, (LONG_PTR)g_pOldEditProc);
             if (g_pOldListProc) SetWindowLongPtrW(hwndList, GWLP_WNDPROC, (LONG_PTR)g_pOldListProc);
             break;


        case WM_DESTROY:
            // Remove clipboard listener
             RemoveClipboardFormatListener(hwnd);
             // Unregister hotkey
             UnregisterHotKey(hwnd, HOTKEY_ID_TOGGLE);
             // Clean up all global resources
             CleanupResources();
             // Post the quit message to terminate the message loop
             PostQuitMessage(0);
            break;

        default:
            // Use the Unicode version of DefWindowProc
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0; // Message processed
}

// Entry Point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Optional: Allocate console for debugging (remove for release)
    // AllocConsole();
    // FILE* pCout;
    // freopen_s(&pCout, "CONOUT$", "w", stdout);

    // Register the window class (Use W version)
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ClipboardHistoryAppWClass"; // Use W suffix for clarity
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_MYICON_BIG)); // Load default icon for class
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Or use custom painting

    if (!RegisterClassW(&wc)) {
         DisplayLastError(L"RegisterClassW");
         MessageBoxW(NULL, L"Failed to register window class!", L"Fatal Error", MB_OK | MB_ICONERROR);
         return 1;
    }

    // Create the main window (Use W version)
    HWND hwnd = CreateWindowExW(
        0,                              // Optional window styles.
        wc.lpszClassName,               // Window class
        L"mclip - Clipboard History",   // Window text
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, // Window style (non-resizable)
        CW_USEDEFAULT, CW_USEDEFAULT,   // Position
        400, 310,                       // Size (width, height matching controls)
        NULL,                           // Parent window
        NULL,                           // Menu
        hInstance,                      // Instance handle
        NULL                            // Additional application data
    );

    if (hwnd == NULL) {
        DisplayLastError(L"CreateWindowExW");
        MessageBoxW(NULL, L"Failed to create main window!", L"Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Register the hotkey (Alt + ;) VK_OEM_1 is often ';'
    // Add MOD_NOREPEAT to prevent multiple triggers when held down
    if (!RegisterHotKey(hwnd, HOTKEY_ID_TOGGLE, MOD_ALT | MOD_NOREPEAT, VK_OEM_1)) {
        DisplayLastError(L"RegisterHotKey");
        MessageBoxW(hwnd, L"Failed to register hotkey (Alt+;). It might be in use by another application.", L"Warning", MB_OK | MB_ICONWARNING);
        // Continue execution even if hotkey fails? Or exit? Depends on requirements.
    }

    // Show the window
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Main message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) { // Use W version
         // Optional: Add IsDialogMessage check if treating as a dialog for Tab navigation etc.
         // if (!IsDialogMessage(hwnd, &msg)) {
             TranslateMessage(&msg);
             DispatchMessageW(&msg); // Use W version
         // }
    }

    // Return the exit code from PostQuitMessage
    return (int)msg.wParam;
}
