#pragma once

#include "MainWindow.g.h"

namespace winrt::WinUIANGLE::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

    private:
        WinUIANGLE::ANGLERenderer renderer{ nullptr };

        float GetScale();
    };
}

namespace winrt::WinUIANGLE::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
