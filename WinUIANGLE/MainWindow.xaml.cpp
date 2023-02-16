#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <microsoft.uI.xaml.window.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Windows::Foundation;

namespace winrt::WinUIANGLE::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Must set up the renderer after SwapChainPanel is loaded
        // otherwise creating swap chain will fail
        Panel().Loaded([this](IInspectable const&, RoutedEventArgs const&)
            {
                renderer = WinUIANGLE::ANGLERenderer(false);
                renderer.SetSurface(Panel(), GetScale());
                renderer.Start();
            });
    }

    float MainWindow::GetScale()
    {
        auto windowNative{ try_as<::IWindowNative>() };
        if (windowNative)
        {
            HWND hWnd{ 0 };
            windowNative->get_WindowHandle(&hWnd);
            auto dpi = GetDpiForWindow(hWnd);;
            return dpi / 96.0f;
        }
        return 1.0f;
    }
}
