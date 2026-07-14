#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#define NOMINMAX
#include <windows.h>
#include <assert.h>
#include <functional>

#include <d3d11.h>
#pragma comment (lib, "d3d11.lib")


#include <DirectXMath.h>
using namespace DirectX;






#pragma comment (lib, "winmm.lib")


#define SCREEN_WIDTH	(1280)
#define SCREEN_HEIGHT	(720)


#include <stdarg.h>

#ifdef NDEBUG
#define LOG_INFO(fmt, ...) ((void)0)
#else
inline void LogInfoInternal(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    OutputDebugStringA(buf);
}
#define LOG_INFO(fmt, ...) LogInfoInternal(fmt, ##__VA_ARGS__)
#endif

HWND GetWindow();

void Invoke(std::function<void()> Function, int Time);

