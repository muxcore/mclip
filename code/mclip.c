#include <windows.h>
#include <assert.h>
#include <string.h>
#include <shellapi.h>
#include "resource.h"
#include <wingdi.h>
#include <stdbool.h>
#include <stdio.h>
#include <shlwapi.h>

// Number of items in clipboard history.
// Note: TODO: Should be option in app itself.
#define MAX_HISTORY 128

#define IDC_SEARCH_EDIT 1001
#define IDC_LISTBOX 1002
#define TIMER_ID 1

HWND hwndList;
HWND hwndEdit;

char* clipboardHistory[MAX_HISTORY] = {0};
int currentHistoryIndex = 0; // If 

//systray icon part
static NOTIFYICONDATA nid = { sizeof(NOTIFYICONDATA) };
bool windowRestored = TRUE;
bool changeListBox = TRUE;

// Global variable to store the original window procedure address
WNDPROC g_pOldListProc = NULL;
WNDPROC g_pOldEditProc = NULL;


// Global brush for the background color:
HBRUSH g_hBrush = NULL;
COLORREF color = RGB(235, 237, 202);  // Yellow color

HBRUSH g_hRedBrush = NULL; // Red brush for error indication
bool g_isFlashing = false; // Track if we're in error state


// Just so that i can break on this function
int
incCurrentHistoryIndex(void)
{
  currentHistoryIndex++;
  return currentHistoryIndex;
}


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
  MessageBox(hwnd, "mclip - Clipboard History App\n\nAuthor:Ilija Tatalovic\nVersion: 0.4.2\nLicence:MIT", "About", MB_OK | MB_ICONINFORMATION);
}


//if we have anything in history, returns TRUE
bool
historyExists()
{
  bool result = FALSE;
  for (size_t i = 0; i < MAX_HISTORY; i++) {
    if (clipboardHistory[i] != NULL)
      result = TRUE;
  }
  return result;
}

//returns number of items in history
size_t
lenHistory()
{
  size_t result = 0;
  for (size_t i = 0; i < MAX_HISTORY; i++) {
    if (clipboardHistory[i] != NULL)
      result++;
  }
  return result;
}


bool
entryExists(const char *str1, const char *history[])
{
  assert(str1);
  for (size_t i = 0; i < MAX_HISTORY; i++) {
    if (history[i] != NULL) {
      if (strcmp(str1, history[i]) == 0)
	return TRUE;
    }
  }  
  return FALSE;
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

void
HandleSearch(HWND hwndListBox, const TCHAR* searchBuffer)
{ 
  // Clear the LISTBOX
  SendMessage(hwndListBox, LB_RESETCONTENT, 0, 0);

  // If the search text is empty, display all items
  if (strlen(searchBuffer) == 0 && historyExists()) {
    // Add all items from your dynamic list to the LISTBOX
    for (int i = MAX_HISTORY-1; i >= 0; --i) {
      if (clipboardHistory[i] != NULL)
	SendMessage(hwndListBox, LB_INSERTSTRING, 0, (LPARAM)clipboardHistory[i]);
    }
  }
  else {
    // Filter and add items that match the search text
    for (int i = MAX_HISTORY-1; i >= 0; --i) {
      //      assert(clipboardHistory[i]);
      assert(searchBuffer);	
      if (clipboardHistory[i] != NULL) {
	if (StrStrIA(clipboardHistory[i], searchBuffer) != NULL) {
	  SendMessage(hwndListBox, LB_INSERTSTRING, 0, (LPARAM)clipboardHistory[i]);
	}
      }
    }
  }
}


// Function to handle WM_KEYDOWN message
void OnKeyDown(HWND hwnd, WPARAM wParam, HWND hEditBox, HWND hListBox)
{
  HWND listBox = GetDlgItem(hwnd, IDC_LISTBOX); // Replace IDC_LISTBOX with your list box ID
  HWND editBox = GetDlgItem(hwnd, IDC_SEARCH_EDIT); // Replace IDC_LISTBOX with your list box ID  

  int selectedIndex = SendMessage(listBox, LB_GETCURSEL, 0, 0);

  if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'P')) {
    MessageBox(hwnd, "Ctrl+P Pressed!", "Key Pressed", MB_OK | MB_ICONINFORMATION);
    HWND currentFocus = GetFocus(); //GetForegroundWindow();
    if (currentFocus == hEditBox) 
      SetFocus(listBox);
    else
      SetFocus(editBox);
  }  
  
  switch (wParam) {
  case VK_UP:    
    if (selectedIndex > 0) {
      SendMessage(listBox, LB_SETCURSEL, selectedIndex - 1, 0);
      SetFocus(listBox);
    }
    break;
    
  case VK_DOWN:
    int itemCount = SendMessage(listBox, LB_GETCOUNT, 0, 0);
    if (selectedIndex < itemCount - 1) {
      SendMessage(listBox, LB_SETCURSEL, selectedIndex + 1, 0);
      SetFocus(listBox);      
    }
    break;

  case VK_RETURN: // Enter key - copies selected item to clipboard
    if (selectedIndex != LB_ERR) {
      // Get the length of the selected item
      int itemLength = SendMessage(listBox, LB_GETTEXTLEN, selectedIndex, 0);
      // Allocate memory to store the selected item
      char *buffer = malloc(sizeof(char) * (itemLength + 1));
      // Get the selected item
      SendMessage(listBox, LB_GETTEXT, selectedIndex, (LPARAM)buffer);
      // Open the clipboard, empty it, and set the data
      if (OpenClipboard(hwnd)) {
	EmptyClipboard();
	HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, (itemLength + 1) * sizeof(TCHAR));
	if (hClipboardData != NULL) {
	  LPTSTR clipboardPtr = (LPTSTR)GlobalLock(hClipboardData);
	  lstrcpy(clipboardPtr, buffer);
	  GlobalUnlock(hClipboardData);
	  SetClipboardData(CF_UNICODETEXT, hClipboardData);
	}
	CloseClipboard();
      }
      // Clean up
      free(buffer);
    }
    break;

    // case VK_TAB: //cannot be here since system is intersepting it first
        
    // Handle other keys as needed
  default:
    // Handle other key presses
    break;
  }
}



LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {      
    case WM_KEYDOWN:
      // Check if Ctrl key is pressed and 'P' key is pressed
      if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'P')) {
	MessageBox(hwnd, TEXT("Ctrl+P Pressed in Edit Box!"), TEXT("Key Pressed"), MB_OK | MB_ICONINFORMATION);
	// You can add more custom behavior here
	return 0; // We handled the message
      }
      if (wParam == VK_TAB)
        {
	  SetFocus(hwndList);
	  return 0;
        }
      
      break;    
      
      // Add other cases as needed
    }
    
    // Call the original window procedure for any messages not handled
    return CallWindowProc(g_pOldEditProc, hwnd, message, wParam, lParam);
}

LRESULT CALLBACK ListSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
    {
    case WM_KEYDOWN:
      if (wParam == VK_TAB)
	  {
	  SetFocus(hwndEdit);
            return 0;
        }
        break;
	
	// Call the original window procedure for any messages not handled    
    }
  return CallWindowProc(g_pOldListProc, hwnd, message, wParam, lParam);
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
			    10, 10, 360, 220, hwnd,  (HMENU)IDC_LISTBOX, NULL, NULL);
    hwndEdit = CreateWindow(
			    "EDIT",                      // Predefined class for EDIT
			    NULL,                            // No window text
    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			    10, 220, 360, 25,                // x, y, width, height
			    hwnd,                            // Parent window
			    (HMENU)IDC_SEARCH_EDIT,          // Control ID
			    NULL,                       // Application instance handle
			    NULL                             // No window creation data
			    );    
    SetWindowIcons(hwnd);
    SendMessage(hwndEdit, EM_SETTABSTOPS, 1, 0);
    SetFocus(hwndEdit);   
      //Subclass both ListBox and  Editbox since i want to intersept TAB in both
    g_pOldEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    g_pOldListProc = (WNDPROC)SetWindowLongPtr(hwndList, GWLP_WNDPROC, (LONG_PTR)ListSubclassProc);   


    if (g_pOldEditProc == NULL || g_pOldListProc == NULL) {
      // Handle error
      MessageBox(hwnd, TEXT("Failed to subclass edit control"), TEXT("Error"), MB_OK | MB_ICONERROR);
    }

    g_hRedBrush = CreateSolidBrush(RGB(255, 0, 0)); // Red brush for error
    g_hBrush = CreateSolidBrush(color);  // Yellow background
    break;

  case WM_TIMER:
    if (wParam == TIMER_ID) {
      g_isFlashing = false;
      KillTimer(hwnd, TIMER_ID);
      InvalidateRect(hwnd, NULL, TRUE); // Redraw with original color
    }
    break;


    
  case WM_KILLFOCUS:
    // Handle the window losing focus here
    windowRestored = FALSE;
    break;    
    
    /* Start Moving withing Mclip */
 case WM_KEYDOWN:
   OnKeyDown(hwnd, wParam, hwndEdit, hwndList);
   break;
  
 
  case WM_HOTKEY:
    // Check if the hotkey ID matches the registered hotkey (1 in this example).
    if (wParam == 1) {
      // Maximize the window.
      if (!windowRestored) {
	  ShowWindow(hwnd, SW_SHOW);
	  SetForegroundWindow(hwnd);
	  SetFocus(hwndEdit);
	  windowRestored = !windowRestored;
	}
      else {
	ShowWindow(hwnd, SW_HIDE);
	windowRestored = !windowRestored;
      }
    }
    break;
    
    
  case WM_CTLCOLOREDIT:
    {
      HDC hdcEdit = (HDC)wParam;
      if (g_isFlashing) {
        SetBkColor(hdcEdit, RGB(255, 0, 0)); // Red when flashing
        return (LRESULT)g_hRedBrush;
      } else {
        SetBkColor(hdcEdit, color); // Normal yellow
        return (LRESULT)g_hBrush;
      }
    }
    break;

  case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (g_isFlashing) {
        FillRect(hdc, &ps.rcPaint, g_hRedBrush);
      } else {
        FillRect(hdc, &ps.rcPaint, g_hBrush);
      }
      EndPaint(hwnd, &ps);
    }
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
              char mbBuffer[1024];
              WideCharToMultiByte(CP_ACP, 0, clipboardText, -1, mbBuffer, sizeof(mbBuffer), NULL, NULL);

              if (!entryExists(mbBuffer, clipboardHistory)) {
                if (clipboardHistory[currentHistoryIndex]) {
                  free(clipboardHistory[currentHistoryIndex]);
                }
                clipboardHistory[currentHistoryIndex] = _strdup(mbBuffer);
                currentHistoryIndex = (currentHistoryIndex + 1) % MAX_HISTORY;

                SendMessage(hwndList, LB_RESETCONTENT, 0, 0);
                for (int i = MAX_HISTORY - 1; i >= 0; i--) {
                  if (clipboardHistory[i] != NULL) {
                    SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)clipboardHistory[i]);
                  }
                }
              }
              GlobalUnlock(hClipboardData);
            }
          }
          CloseClipboard();
          break; // Success
        } else {
          DWORD errorCode = GetLastError();
          if (errorCode == ERROR_ACCESS_DENIED) {
            retryCount++;
            if (retryCount < maxRetries) {
              Sleep(retryDelayMs);
            } else {
              // Flash window and play sound
              FLASHWINFO flashInfo = { sizeof(FLASHWINFO) };
              flashInfo.hwnd = hwnd;
              flashInfo.dwFlags = FLASHW_ALL | FLASHW_TIMERNOFG; // Flash until foreground
              flashInfo.uCount = 5; // Flash 5 times
              flashInfo.dwTimeout = 0; // Default flash rate
              FlashWindowEx(&flashInfo);

              // Set red background and start timer to revert
              g_isFlashing = true;
              InvalidateRect(hwnd, NULL, TRUE);
              SetTimer(hwnd, TIMER_ID, 1000, NULL); // 1 second red flash

              // Play sound
              Beep(750, 300); // 750 Hz for 300 ms
            }
          } else {
            displayLastError("OpenClipboard");
            break;
          }
        }
      }
    }
    break;
   
    
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case 1:
      ShowAboutDialog(hwnd);
      break;
    }
    //copies selection for listbox to clipboard
    if (HIWORD(wParam) == LBN_SELCHANGE && (HWND)lParam == hwndList) {
      int selectedIndex = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
      if (selectedIndex != LB_ERR) {
	if (OpenClipboard(hwnd)) {
	  EmptyClipboard();
	  int textLength = SendMessage(hwndList, LB_GETTEXTLEN, selectedIndex, 0);
	  if (textLength != LB_ERR) {
	    char* buffer = (char*)malloc(textLength + 1); // +1 for null terminator
	    if (buffer != NULL) {
	      // Get the text of the selected item.
	      SendMessage(hwndList, LB_GETTEXT, selectedIndex, (LPARAM)buffer);
	      
	      HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, textLength + 1);
	      if (hClipboardData) {
		char* clipboardText = (char*)GlobalLock(hClipboardData);
		if (clipboardText) {
		  strcpy(clipboardText, buffer);
		  free(buffer);	  
		  GlobalUnlock(hClipboardData);
		  SetClipboardData(CF_TEXT, hClipboardData);
		}
	      }
	    }	    
	  }
	}
	CloseClipboard();
      }
    }    
    // Search Box
  case IDC_SEARCH_EDIT:
    if (HIWORD(wParam) == EN_CHANGE) {
      // Handle text changes in the edit control
      CHAR searchText[256] = {0};
      GetWindowText((HWND)lParam, searchText, sizeof(searchText) / sizeof(searchText[0]));
      HandleSearch(GetDlgItem(hwnd, IDC_LISTBOX), searchText);
    }
    break; 
    
  case WM_DESTROY:
    DeleteObject(g_hBrush);
    DeleteObject(g_hRedBrush);
    PostQuitMessage(0);
    break;

  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {  
  //Uncomment this to have console for Windows (== not console) programs
  /* AllocConsole(); */
  /* freopen("CONOUT$", "w", stdout); // Redirect stdout to the console window */
  
  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = "ClipboardHistoryApp";
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYICON_BIG));  // IDI_MYICON is the icon resource ID
  RegisterClass(&wc);

  HWND hwnd = CreateWindow(wc.lpszClassName, "mclip - Clipboard History App", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
			   CW_USEDEFAULT, CW_USEDEFAULT, 400, 310, NULL, NULL, hInstance, NULL);

  // Enables app to monitor change in clipboard without constantly pooling it
  // Window will receive WM_CLIPBOARDUPDATE messages when changes occur.
  AddClipboardFormatListener(hwnd);

  //TODO: bug when other windows have same hotkey
     // hotkey needs to be in main?
  BOOL success = RegisterHotKey(hwnd, 1, MOD_ALT, VK_OEM_1);
  if (!success) {
    MessageBox(NULL, "Failed to register hotkey.", "Error", MB_OK | MB_ICONERROR);
  } 
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
