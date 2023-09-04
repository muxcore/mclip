#include <windows.h>
#include <assert.h>
#include <shellapi.h>
#include "resource.h"

// Number of items in clipboard history.
// Note: TODO: Should be option in app itself.
#define MAX_HISTORY 25

HWND hwndList;
char* clipboardHistory[MAX_HISTORY];
int currentHistoryIndex = 0;
//systray icon part
static NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };

void
displayLastError(const char *functionName)
{
  DWORD errorCode = GetLastError();
  LPVOID errorText = NULL;

  FormatMessageA(
		 FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		 NULL,
		 errorCode,
		 0,
		 (LPSTR)&errorText,
		 0,
		 NULL
		 );

  if (errorText != NULL) {
    printf("Error in function %s: %s\n", functionName, (LPSTR)errorText);
    LocalFree(errorText);
  } else {
    printf("Error in function %s: Unknown error (error code %lu)\n", functionName, errorCode);
  }
}

// Function to show the "About" dialog
void
ShowAboutDialog(HWND hwnd)
{
  MessageBox(hwnd, "mclip - Clipboard History App\n\nAuthor:Ilija Tatalovic\nVersion: 0.1\nLicence:MIT", "About", MB_OK | MB_ICONINFORMATION);
}


void
SetWindowIcons(HWND hwnd)
{
  HICON hIconBig = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MYICON_BIG));
  HICON hIconSmall = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MYICON_SMALL));
  HICON hIconTray = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MYTRAY_ICON));

  if (!hIconBig)
    displayLastError("LoadICon Big"); 
  if (!hIconSmall)
    displayLastError("LoadICon Small"); 
  if (!hIconTray)
    displayLastError("LoadICon Tray"); 
  
  SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
  SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
  SendMessage(hwnd, WM_SETICON, IDI_MYTRAY_ICON, (LPARAM)hIconTray);  

  nid.uID = IDI_MYTRAY_ICON;
  nid.hWnd = hwnd;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_TRAY_ICON;
  nid.hIcon = hIconTray;
  strcpy(nid.szTip, "mclip"); // szTIp is 64, so this is fine
  assert(Shell_NotifyIcon(NIM_ADD, &nid));

}


LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    
  case WM_CREATE:
    // Create menu bar
    HMENU hMenu, hSubMenu;
    hMenu = CreateMenu();
    hSubMenu = CreatePopupMenu();
    AppendMenu(hSubMenu, MF_STRING, 1, "About");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenu, "Help");
    SetMenu(hwnd, hMenu);	  
    hwndList = CreateWindow("LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL,
			    10, 10, 360, 220, hwnd, NULL, NULL, NULL);
    SetWindowIcons(hwnd);
    break;
    
  case WM_TRAY_ICON:
    if (wParam == IDI_MYTRAY_ICON) {
      if (lParam == WM_LBUTTONUP) {
	// Restore the window when the tray icon is left-clicked
	ShowWindow(hwnd, SW_RESTORE);
      }
    }
    break;

  case WM_SYSCOMMAND:
    if (wParam == SC_MINIMIZE) {
      // Minimize to system tray
      ShowWindow(hwnd, SW_HIDE);
      Shell_NotifyIcon(NIM_ADD, &nid);
      return 0;
    }
    // We need default cases so unhandled system commands are processed correctly by the system.
    else
      return DefWindowProc(hwnd, msg, wParam, lParam);    
    break;
    
  case WM_CLIPBOARDUPDATE:
    if (currentHistoryIndex + 1 == MAX_HISTORY) {
      SendMessage(hwndList, LB_DELETESTRING, 0, 0);
      --currentHistoryIndex;
    }	  
    if (IsClipboardFormatAvailable(CF_TEXT)) {
      if (OpenClipboard(hwnd)) {
	HANDLE hClipboardData = GetClipboardData(CF_TEXT);
	if (hClipboardData != NULL) {
	  char* clipboardText = (char*)GlobalLock(hClipboardData);
	  if (clipboardText != NULL) {
	    if (clipboardHistory[currentHistoryIndex]) {
	      GlobalFree(clipboardHistory[currentHistoryIndex]);
	    }
	    clipboardHistory[currentHistoryIndex] = _strdup(clipboardText);
	    SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)clipboardText);
	    currentHistoryIndex = (currentHistoryIndex + 1) % MAX_HISTORY;
	    GlobalUnlock(hClipboardData);
	  }
	}
	CloseClipboard();
      }
    }
    break;
    
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case 1:
      ShowAboutDialog(hwnd);
      break;
    }	  
    if (HIWORD(wParam) == LBN_SELCHANGE && (HWND)lParam == hwndList) {
      int selectedIndex = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
      if (selectedIndex != LB_ERR) {
	if (OpenClipboard(hwnd)) {
	  EmptyClipboard();
	  HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, strlen(clipboardHistory[selectedIndex]) + 1);
	  if (hClipboardData) {
	    char* clipboardText = (char*)GlobalLock(hClipboardData);
	    if (clipboardText) {
	      strcpy(clipboardText, clipboardHistory[selectedIndex]);
	      GlobalUnlock(hClipboardData);
	      SetClipboardData(CF_TEXT, hClipboardData);
	    }
	  }
	  CloseClipboard();
	}
      }
    }
    break;   
	    
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ClipboardHistoryApp";
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYICON_BIG));  // IDI_MYICON is the icon resource ID
    RegisterClass(&wc);

    HWND hwnd = CreateWindow(wc.lpszClassName, "mclip - Clipboard History App", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                             CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);

    AddClipboardFormatListener(hwnd);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    //This is just a cleanup when window is getting closed. Probably not necessary.
    for (int i = 0; i < MAX_HISTORY; ++i) {
        if (clipboardHistory[i]) {
            free(clipboardHistory[i]);
        }
    }

    return 0;
}
