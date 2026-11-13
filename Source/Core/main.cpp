

#include "main.h"
#include "manager.h"
#include "renderer.h"
#include <thread>
#include <imm.h>

#pragma comment(lib, "imm32.lib")

const char* CLASS_NAME = "AppClass";
const char* WINDOW_NAME = "TOON SLASHER";


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


HWND g_Window;

HWND GetWindow()
{
	return g_Window;
}


int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	// ゲーム中にIME（日本語入力候補窓）が起動しないように完全に無効化する
	ImmDisableIME(0);

#ifdef NDEBUG
	// リリースビルド時は、カレントディレクトリを実行ファイルが存在するフォルダに移動します。
	// これにより、Visual Studioからのデバッグ実行や直接実行に関わらず、Assetsフォルダから正しく読み込めるようになります。
	char szPath[MAX_PATH];
	GetModuleFileNameA(NULL, szPath, MAX_PATH);
	char* pLastSlash = strrchr(szPath, '\\');
	if (pLastSlash) {
		*pLastSlash = '\0';
		SetCurrentDirectoryA(szPath);
	}
#endif

	WNDCLASSEX wcex;
	{
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = 0;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = CLASS_NAME;
		wcex.hIconSm = nullptr;

		RegisterClassEx(&wcex);


		RECT rc = { 0, 0, (LONG)SCREEN_WIDTH, (LONG)SCREEN_HEIGHT };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		g_Window = CreateWindowEx(0, CLASS_NAME, WINDOW_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	}

	HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
	if (FAILED(hr))
	{
		return 0;
	}

	Manager::Init();



	ShowWindow(g_Window, nCmdShow);
	UpdateWindow(g_Window);




	timeBeginPeriod(1);

	// 高精度タイマーの初期化
	LARGE_INTEGER qpcFreq;
	QueryPerformanceFrequency(&qpcFreq);
	const double invFreq = 1.0 / static_cast<double>(qpcFreq.QuadPart);

	LARGE_INTEGER lastCounter;
	QueryPerformanceCounter(&lastCounter);
	LARGE_INTEGER lastDrawCounter = lastCounter;

	// ゲームロジックは常に 60Hz（約16.666ms）固定で更新
	const double FIXED_DELTA_TIME = 1.0 / 60.0;
	double accumulator = 0.0;

	MSG msg;
	while(1)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
			{
				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			LARGE_INTEGER currentCounter;
			QueryPerformanceCounter(&currentCounter);
			double frameDelta = static_cast<double>(currentCounter.QuadPart - lastCounter.QuadPart) * invFreq;
			lastCounter = currentCounter;

			// ウィンドウ移動や一時停止などによる極端な時間の跳躍（スパイラル・オブ・デス）を防止（最大0.1秒）
			if (frameDelta > 0.1) {
				frameDelta = 0.1;
			}

			accumulator += frameDelta;

			// ゲームロジックの固定ステップ更新（60Hz）
			LARGE_INTEGER dbgU0, dbgU1;
			QueryPerformanceCounter(&dbgU0);
			while (accumulator >= FIXED_DELTA_TIME)
			{
				Manager::Update();
				accumulator -= FIXED_DELTA_TIME;
			}
			QueryPerformanceCounter(&dbgU1);
			Manager::s_DbgCpuUpdateMs = (float)((double)(dbgU1.QuadPart - dbgU0.QuadPart) * invFreq * 1000.0);

			// 描画フレームレート制御
			int targetFPS = Manager::GetTargetFPS();
			bool canDraw = false;

			if (targetFPS <= 0)
			{
				// 無制限（即座に描画）
				canDraw = true;
			}
			else
			{
				double targetInterval = 1.0 / static_cast<double>(targetFPS);
				double elapsedSinceLastDraw = static_cast<double>(currentCounter.QuadPart - lastDrawCounter.QuadPart) * invFreq;
				if (elapsedSinceLastDraw >= targetInterval)
				{
					canDraw = true;
				}
			}

			if (canDraw)
			{
				double drawDelta = static_cast<double>(currentCounter.QuadPart - lastDrawCounter.QuadPart) * invFreq;
				lastDrawCounter = currentCounter;

				// 60Hz固定ステップ間の余剰時間から描画補間係数（0.0〜1.0）を算出
				float alpha = static_cast<float>(accumulator / FIXED_DELTA_TIME);
				if (alpha > 1.0f) alpha = 1.0f;
				if (alpha < 0.0f) alpha = 0.0f;

				Manager::Draw(alpha);
				Manager::UpdateFPSCounter(static_cast<float>(drawDelta));
			}
			else
			{
				// 次の描画タイミングまで余裕がある場合は、CPU使用率100%張り付きを防ぐため休止
				double targetInterval = 1.0 / static_cast<double>(targetFPS);
				double elapsedSinceLastDraw = static_cast<double>(currentCounter.QuadPart - lastDrawCounter.QuadPart) * invFreq;
				double remaining = targetInterval - elapsedSinceLastDraw;
				if (remaining > 0.002) // 2ms以上あるならSleep(1)
				{
					Sleep(1);
				}
				else
				{
					YieldProcessor(); // スピン待機
				}
			}
		}
	}

	timeEndPeriod(1);

	UnregisterClass(CLASS_NAME, wcex.hInstance);

	Manager::Uninit();

	CoUninitialize();

	return (int)msg.wParam;
}




// ImGuiのメッセージハンドラの前方宣言
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // F11 または Alt+Enter は、ImGui のフォーカス状態に関わらず最優先で処理
    if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)
    {
        if (wParam == VK_F11 || (wParam == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000)))
        {
            ToggleFullscreen();
            return 0;
        }
    }

    // ImGuiがメッセージを処理した場合は早期リターン
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

	switch(uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

    case WM_SIZE:
        if (Renderer::GetDeviceContext() && wParam != SIZE_MINIMIZED) {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            Renderer::OnResize(width, height);
        }
        break;

	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_ESCAPE:
			// Escキーによる強制終了を無効化（ゲーム内ポーズ/マウスロック解除用に使用）
			break;
		}
		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// =================================================================
// ボーダレスフルスクリーンの切り替え処理（ネイティブ解像度対応）
// =================================================================
static bool  s_IsFullscreen = false;
static RECT  s_SavedWindowRect = { 0, 0, 0, 0 };
static DWORD s_SavedWindowStyle = 0;

bool IsFullscreen()
{
    return s_IsFullscreen;
}

void SetFullscreen(bool fullscreen)
{
    if (s_IsFullscreen == fullscreen) return;

    HWND hWnd = g_Window;
    if (!hWnd) return;

    if (fullscreen) {
        // 現在のウィンドウ位置とスタイルを退避
        GetWindowRect(hWnd, &s_SavedWindowRect);
        s_SavedWindowStyle = GetWindowLong(hWnd, GWL_STYLE);

        // 現在のウィンドウが所属するモニターの解像度を取得
        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(hMon, &mi);

        int width = mi.rcMonitor.right - mi.rcMonitor.left;
        int height = mi.rcMonitor.bottom - mi.rcMonitor.top;

        // スタイルを枠なしポップアップに変更して画面全体に配置
        SetWindowLong(hWnd, GWL_STYLE, (s_SavedWindowStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        SetWindowPos(hWnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            width, height,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        s_IsFullscreen = true;
        Renderer::OnResize((UINT)width, (UINT)height);
    } else {
        // 元のスタイルとウィンドウ位置に戻す
        SetWindowLong(hWnd, GWL_STYLE, s_SavedWindowStyle);

        int winW = s_SavedWindowRect.right - s_SavedWindowRect.left;
        int winH = s_SavedWindowRect.bottom - s_SavedWindowRect.top;
        if (winW <= 0 || winH <= 0) {
            RECT rc = { 0, 0, (LONG)SCREEN_WIDTH, (LONG)SCREEN_HEIGHT };
            AdjustWindowRect(&rc, s_SavedWindowStyle, FALSE);
            winW = rc.right - rc.left;
            winH = rc.bottom - rc.top;
            s_SavedWindowRect.left = (GetSystemMetrics(SM_CXSCREEN) - winW) / 2;
            s_SavedWindowRect.top = (GetSystemMetrics(SM_CYSCREEN) - winH) / 2;
        }

        SetWindowPos(hWnd, nullptr,
            s_SavedWindowRect.left, s_SavedWindowRect.top,
            winW, winH,
            SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        s_IsFullscreen = false;
        Renderer::OnResize((UINT)SCREEN_WIDTH, (UINT)SCREEN_HEIGHT);
    }
}

void ToggleFullscreen()
{
    SetFullscreen(!s_IsFullscreen);
}
