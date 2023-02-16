#include "pch.h"
#include "ANGLERenderer.h"
#if __has_include("ANGLERenderer.g.cpp")
#include "ANGLERenderer.g.cpp"
#endif

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif

#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <GLES2/gl2.h>
#include <angle_windowsstore.h>

#define EGL_PLATFORM_ANGLE_ANGLE 0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE 0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE 0x3205
#define EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE 0x3206
#define EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE 0x3451
#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE 0x320A
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE 0x320B
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_REFERENCE_ANGLE 0x320C
#define EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE 0x320F

using namespace std;

namespace winrt::WinUIANGLE::implementation
{
    ANGLERenderer::ANGLERenderer(bool enableMultisample) : enableMultisample(enableMultisample)
    {
        InitializeCriticalSection(&msgCritSection);
        InitializeConditionVariable(&resumeCond);
    }

    bool ANGLERenderer::Initialize()
    {
        if (context == EGL_NO_CONTEXT)
        {
            printf("Initializing context");

            const EGLint multisampleAttribs[] =
            {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
                EGL_DEPTH_SIZE, 16,
                EGL_SAMPLES, 4,
                EGL_SAMPLE_BUFFERS, 1,
                EGL_NONE
            };
            const EGLint attribs[] =
            {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_BLUE_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_RED_SIZE, 8,
                EGL_DEPTH_SIZE, 16,
                EGL_NONE
            };

            const EGLint defaultDisplayAttributes[] =
            {
                // These are the default display attributes, used to request ANGLE's D3D11 renderer.
                // eglInitialize will only succeed with these attributes if the hardware supports D3D11 Feature Level 10_0+.
                EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

                // EGL.PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE is an option that enables ANGLE to automatically call 
                // the IDXGIDevice3::Trim method on behalf of the application when it gets suspended. 
                // Calling IDXGIDevice3::Trim when an application is suspended is a Windows Store application certification requirement.
                EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
                EGL_NONE,
            };

            const EGLint fl9_3DisplayAttributes[] =
            {
                // These can be used to request ANGLE's D3D11 renderer, with D3D11 Feature Level 9_3.
                // These attributes are used if the call to eglInitialize fails with the default display attributes.
                EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
                EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
                EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
                EGL_NONE,
            };

            const EGLint warpDisplayAttributes[] =
            {
                // These attributes can be used to request D3D11 WARP.
                // They are used if eglInitialize fails with both the default display attributes and the 9_3 display attributes.
                EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
                EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
                EGL_NONE,
            };

            EGLint numConfigs;

            display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
            if (display == EGL_NO_DISPLAY)
            {
                printf("eglGetDisplay() returned error %d", eglGetError());
                return false;
            }

            if (eglInitialize(display, nullptr, nullptr) == EGL_FALSE)
            {
                // This tries to initialize EGL to D3D11 Feature Level 9_3, if 10_0+ is unavailable (e.g. on some mobile devices).
                Destroy();
                display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY,
                    fl9_3DisplayAttributes);
                if (display == EGL_NO_DISPLAY)
                {
                    printf("eglGetDisplay() returned error %d", eglGetError());
                    return false;
                }

                if (eglInitialize(display, nullptr, nullptr) == EGL_FALSE)
                {
                    Destroy();

                    // This initializes EGL to D3D11 Feature Level 11_0 on WARP, if 9_3+ is unavailable on the default GPU.
                    display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY,
                        warpDisplayAttributes);
                    if (display == EGL_NO_DISPLAY)
                    {
                        printf("eglGetDisplay() returned error %d", eglGetError());
                        return false;
                    }

                    if (eglInitialize(display, nullptr, nullptr) == EGL_FALSE)
                    {
                        // If all of the calls to eglInitialize returned EGL.FALSE then an error has occurred.
                        Destroy();
                        printf("eglInitialize() returned error %d", eglGetError());
                        return false;
                    }
                }
            }

            if (enableMultisample) {
                // Try to enable multisample but fallback if not available
                if (!eglChooseConfig(display, multisampleAttribs, &config, 1, &numConfigs) && !eglChooseConfig(display, attribs, &config, 1, &numConfigs))
                {
                    printf("eglChooseConfig() returned error %d", eglGetError());
                    Destroy();
                    return false;
                }
            }
            else {
                if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs))
                {
                    printf("eglChooseConfig() returned error %d", eglGetError());
                    Destroy();
                    return false;
                }
            }

            if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format))
            {
                printf("eglGetConfigAttrib() returned error %d", eglGetError());
                Destroy();
                return false;
            }

            const EGLint contextAttributes[] =
            {
                    EGL_CONTEXT_CLIENT_VERSION, 2,
                    EGL_NONE
            };

            context = eglCreateContext(display, config, nullptr, contextAttributes);
            if (context == nullptr)
            {
                printf("eglCreateContext() returned error %d", eglGetError());
                Destroy();
                return false;
            }
        }

        if (surface != EGL_NO_SURFACE)
        {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(display, surface);
            surface = EGL_NO_SURFACE;
        }

        if (window)
        {
            const EGLint surfaceAttributes[] =
            {
                EGL_NONE
            };

            Windows::Foundation::Collections::PropertySet propertySet;
            propertySet.Insert(EGLNativeWindowTypeProperty, window);
            propertySet.Insert(EGLRenderResolutionScaleProperty, Windows::Foundation::PropertyValue::CreateSingle(windowScale));
            EGLNativeWindowType win = reinterpret_cast<EGLNativeWindowType>(get_abi(propertySet));

            surface = eglCreateWindowSurface(display, config, win, surfaceAttributes);
            if (surface == nullptr)
            {
                printf("eglCreateWindowSurface() returned error %d", eglGetError());
                Destroy();
                return false;
            }

            if (!eglMakeCurrent(display, surface, surface, context))
            {
                printf("eglMakeCurrent() returned error %d", eglGetError());
                Destroy();
                return false;
            }
        }
        return true;
    }

    void ANGLERenderer::Destroy()
    {
        if (context != EGL_NO_CONTEXT)
        {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(display, context);
            if (surface != EGL_NO_SURFACE)
                eglDestroySurface(display, surface);
            eglTerminate(display);
        }
        display = EGL_NO_DISPLAY;
        surface = EGL_NO_SURFACE;
        context = EGL_NO_CONTEXT;
        currentWindowWidth = 0;
        currentWindowHeight = 0;
    }

    void ANGLERenderer::ResizeIfNeeded()
    {
        EGLint windowWidth;
        EGLint windowHeight;
        if (!eglQuerySurface(display, surface, EGL_WIDTH, &windowWidth) || !eglQuerySurface(display, surface, EGL_HEIGHT, &windowHeight))
            return;

        if (currentWindowHeight != windowHeight || currentWindowWidth != windowWidth)
        {
            currentWindowWidth = windowWidth;
            currentWindowHeight = windowHeight;
        }
    }

    bool ANGLERenderer::Prepare()
    {
        const char* vShaderSource =
            "attribute vec4 vPosition;\n"
            "void main()\n"
            "{\n"
            "    gl_Position = vPosition;\n"
            "}";
        const char* fShaderSource =
            "precision mediump float;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
            "}\n";

        // Assume we are always on the happy path, so no error checking...
        auto vShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vShader, 1, &vShaderSource, nullptr);
        glCompileShader(vShader);

        GLint compiled;
        glGetShaderiv(vShader, GL_COMPILE_STATUS, &compiled);

        auto fShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fShader, 1, &fShaderSource, nullptr);
        glCompileShader(fShader);

        glGetShaderiv(fShader, GL_COMPILE_STATUS, &compiled);

        auto program = glCreateProgram();
        glAttachShader(program, vShader);
        glAttachShader(program, fShader);
        glBindAttribLocation(program, 0, "vPosition");
        glLinkProgram(program);

        GLint linked;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);

        glDeleteShader(vShader);
        glDeleteShader(fShader);

        this->program = program;

        return true;
    }

    void ANGLERenderer::Draw()
    {
        glViewport(0, 0, (GLsizei)currentWindowWidth, (GLsizei)currentWindowHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        const float vertices[] = { 0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.2f, 0.5f, -0.5f, 0.0f };
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void ANGLERenderer::Start()
    {
        // If the render loop is already running then do not start another thread.
        if (mRenderLoopWorker != nullptr && mRenderLoopWorker.Status() == Windows::Foundation::AsyncStatus::Started)
        {
            return;
        }

        // Create a task for rendering that will be run on a background thread.
        auto workItemHandler = Windows::System::Threading::WorkItemHandler([this](Windows::Foundation::IAsyncAction action)
        {
            auto renderer = this;
            while (action.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
            {
                if (renderer->surface != EGL_NO_SURFACE && !renderer->engineStartedCalled)
                {
                    if (!Prepare())
                        break;
                    renderer->engineStartedCalled = true;
                }

                renderer->Lock();
                renderer->Wait();

                switch (renderer->msg)
                {
                case ANGLERenderer::MSG_WINDOW_SET:
                    renderer->Initialize();
                    break;
                default:
                    break;
                }
                renderer->msg = ANGLERenderer::MSG_NONE;

                bool needsDrawn = false;
                if (renderer->engineStartedCalled && renderer->surface != EGL_NO_SURFACE)
                {
                    renderer->ResizeIfNeeded();
                    needsDrawn = true;
                }
                renderer->Unlock();

                if (needsDrawn)
                {
                    renderer->Draw();
                    if (!eglSwapBuffers(renderer->display, renderer->surface))
                        printf("eglSwapBuffers() returned error %d", eglGetError());
                }
            }
            renderer->Destroy();
        });

        // Run task on a dedicated high priority background thread.
        mRenderLoopWorker = Windows::System::Threading::ThreadPool::RunAsync(workItemHandler, Windows::System::Threading::WorkItemPriority::High, Windows::System::Threading::WorkItemOptions::TimeSliced);
    }

    void ANGLERenderer::Stop()
    {
        if (mRenderLoopWorker)
        {
            mRenderLoopWorker.Cancel();
            mRenderLoopWorker = nullptr;
        }
    }

    void ANGLERenderer::Lock()
    {
        EnterCriticalSection(&msgCritSection);
    }

    void ANGLERenderer::Unlock()
    {
        LeaveCriticalSection(&msgCritSection);
    }

    void ANGLERenderer::Pause()
    {
        Lock();
        suspendedFlag = true;
        Unlock();
    }

    void ANGLERenderer::Resume()
    {
        Lock();
        suspendedFlag = false;
        WakeAllConditionVariable(&resumeCond);
        Unlock();
    }

    void ANGLERenderer::Wait()
    {
        while (suspendedFlag)
        {
            SleepConditionVariableCS(&resumeCond, &msgCritSection, INFINITE);
        }
    }

    void ANGLERenderer::SetSurface(Microsoft::UI::Xaml::Controls::SwapChainPanel const& newSurface, float scale)
    {
        Lock();
        msg = ANGLERenderer::MSG_WINDOW_SET;
        window = newSurface;
        windowScale = scale;
        Unlock();
    }
}
