#include <cstdio>
#include <mutex>
#include <thread>

#include "hooks.hpp"

#include "backend/dx10/hook_directx10.hpp"
#include "backend/dx11/hook_directx11.hpp"
#include "backend/dx12/hook_directx12.hpp"
#include "backend/dx9/hook_directx9.hpp"

#include "backend/opengl/hook_opengl.hpp"
#include "backend/vulkan/hook_vulkan.hpp"

#include "../console/console.hpp"
#include "../menu/menu.hpp"
#include "../utils/utils.hpp"

#include "../dependencies/minhook/MinHook.h"

#include "input/hook_mouse.hpp"
#include "input/hook_dinput8.hpp"

static HWND g_hWindow = NULL;
static std::mutex g_mReinitHooksGuard;

static DWORD WINAPI ReinitializeGraphicalHooks( LPVOID lpParam ) {
    std::lock_guard<std::mutex> guard{ g_mReinitHooksGuard };

    LOG( "[!] Hooks will reinitialize!\n" );

    HWND hNewWindow = U::GetProcessWindow();
    while ( hNewWindow == reinterpret_cast<HWND>( lpParam ) ) {
        hNewWindow = U::GetProcessWindow();
    }

    H::bShuttingDown = true;

    H::Free();
    H::Init();

    H::bShuttingDown = false;
    Menu::bShowMenu = true;

    return 0;
}

#define TILDE_KEY VK_OEM_3
//
#define KEY_TOGGLE_MENU TILDE_KEY // VK_INSERT
#define KEY_REINITIALIZE_HOOKS VK_HOME
#define KEY_UNLOAD_DLL VK_END
#define KEY_HIDE_MOUSE VK_DELETE

void forceHideCursor() {
    while (ShowCursor(false) >= 0);
}
void forceShowCursor() {
    while (ShowCursor(true) < 0);
}

static WNDPROC oWndProc;
static LRESULT WINAPI WndProc( const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
    Mouse::enableApis = true;

    if ( uMsg == WM_KEYDOWN ) {
        if ( wParam == KEY_HIDE_MOUSE ) {
            bool ctrlKey = GetAsyncKeyState( VK_CONTROL );
            if ( ctrlKey ) {
                LOG( "[+] Showing mouse!\n" );
                ShowCursor(true);
            } else {
                LOG( "[+] Hidding mouse!\n" );
                ShowCursor(false);
            }
        } else if ( wParam == KEY_TOGGLE_MENU ) {
            Menu::bShowMenu = !Menu::bShowMenu;
            return 0;
        } else if ( wParam == KEY_REINITIALIZE_HOOKS ) {
            HANDLE hHandle = CreateThread( NULL, 0, ReinitializeGraphicalHooks, NULL, 0, NULL );
            if ( hHandle != NULL )
                CloseHandle( hHandle );
            return 0;
        } else if ( wParam == KEY_UNLOAD_DLL ) {
            H::bShuttingDown = true;
            U::UnloadDLL();
            return 0;
        }
    } else if ( uMsg == WM_DESTROY ) {
        HANDLE hHandle = CreateThread( NULL, 0, ReinitializeGraphicalHooks, hWnd, 0, NULL );
        if ( hHandle != NULL )
            CloseHandle( hHandle );
    }

    bool callOriginal = true;

    LRESULT result = NULL;

    LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    if ( Menu::bShowMenu ) {

            switch (uMsg) {
                case WM_MOUSEMOVE:
                case WM_MOUSEHOVER:
                case WM_MOUSEWHEEL:
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                    callOriginal = false;
                    break;
                default:
                    break;
            }

        SetCursor(LoadCursor(NULL, IDC_ARROW));
        // ShowCursor(true);
        forceShowCursor();

        result = ImGui_ImplWin32_WndProcHandler( hWnd, uMsg, wParam, lParam );

        // (Doesn't work for some games like 'Sid Meier's Civilization VI')
        // Window may not maximize from taskbar because 'H::bShowDemoWindow' is set to true by default. ('hooks.hpp')
        //
        //return ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam) == 0;
    } else {
        // ShowCursor(false);
        forceHideCursor();
    }

    Mouse::enableApis = false;

    if (callOriginal)
        result = CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);

    return result;
}

namespace Hooks {
    void Init() {
        g_hWindow = U::GetProcessWindow();

        #ifdef DISABLE_LOGGING_CONSOLE
        bool bNoConsole = GetConsoleWindow() == NULL;
        if ( bNoConsole ) {
            AllocConsole();
        }
        #endif

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend();
        switch ( eRenderingBackend ) {
            case DIRECTX9:
                DX9::Hook( g_hWindow );
                break;
            case DIRECTX10:
                DX10::Hook( g_hWindow );
                break;
            case DIRECTX11:
                DX11::Hook( g_hWindow );
                break;
            case DIRECTX12:
                DX12::Hook( g_hWindow );
                break;
            case OPENGL:
                GL::Hook( g_hWindow );
                break;
            case VULKAN:
                VK::Hook( g_hWindow );
                break;
        }

        #ifdef DISABLE_LOGGING_CONSOLE
        if ( bNoConsole ) {
            FreeConsole();
        }
        #endif

        oWndProc = reinterpret_cast<WNDPROC>( SetWindowLongPtr( g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( WndProc ) ) );

        Mouse::Hook( );
        DI8::Hook( );
    }

    void Free() {
        DI8::Unhook( );
        Mouse::Unhook( );

        if ( oWndProc ) {
            SetWindowLongPtr( g_hWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>( oWndProc ) );
        }

        MH_DisableHook( MH_ALL_HOOKS );
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

        RenderingBackend_t eRenderingBackend = U::GetRenderingBackend();
        switch ( eRenderingBackend ) {
            case DIRECTX9:
                DX9::Unhook();
                break;
            case DIRECTX10:
                DX10::Unhook();
                break;
            case DIRECTX11:
                DX11::Unhook();
                break;
            case DIRECTX12:
                DX12::Unhook();
                break;
            case OPENGL:
                GL::Unhook();
                break;
            case VULKAN:
                VK::Unhook();
                break;
        }
    }
} // namespace Hooks
