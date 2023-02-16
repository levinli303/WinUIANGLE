#pragma once

#ifndef EGL_EGL_PROTOTYPES
#define EGL_EGL_PROTOTYPES 1
#endif

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <thread>
#include <mutex>
#include "ANGLERenderer.g.h"

namespace winrt::WinUIANGLE::implementation
{
    struct ANGLERenderer : ANGLERendererT<ANGLERenderer>
    {
        ANGLERenderer(bool enableMultisample);

        bool Initialize();
        void Destroy();
        void ResizeIfNeeded();
        bool Prepare();
        void Draw();
        void Start();
        void Stop();
        void Lock();
        void Unlock();
        void Pause();
        void Resume();
        void Wait();
        void SetSurface(Microsoft::UI::Xaml::Controls::SwapChainPanel const& surface, float scale);
        void SetSize(int32_t width, int32_t height);

        enum RenderThreadMessage {
            MSG_NONE = 0,
            MSG_WINDOW_SET,
        };

        enum RenderThreadMessage msg = MSG_NONE;

        bool enableMultisample = false;
        bool engineStartedCalled = false;

        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
        EGLConfig config{};
        EGLint format{};

    private:
        bool suspendedFlag = false;
        CRITICAL_SECTION msgCritSection;
        CONDITION_VARIABLE resumeCond;

        int currentWindowWidth = 0;
        int currentWindowHeight = 0;
        float windowScale = 1.0f;

        Microsoft::UI::Xaml::Controls::SwapChainPanel window{ nullptr };

        Windows::Foundation::IAsyncAction mRenderLoopWorker{ nullptr };

        bool enableMSAA;

        GLuint program{ 0 };
    };
}

namespace winrt::WinUIANGLE::factory_implementation
{
    struct ANGLERenderer : ANGLERendererT<ANGLERenderer, implementation::ANGLERenderer>
    {
    };
}
