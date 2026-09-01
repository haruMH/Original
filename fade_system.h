#pragma once
#include <d3d11.h>
#include <directxmath.h>

using namespace DirectX;

// フェードの状態を表す列挙型
enum class FadeState {
    None,       // フェード処理なし (通常状態)
    FadeOut,    // 暗転中 (画面が指定色へ覆われる)
    FadeIn      // 明転中 (暗転状態から画面が現れる)
};

// =================================================================
// フェード制御・描画システムクラス (FadeSystem)
// =================================================================
class FadeSystem {
private:
    static FadeState                m_State;
    static float                    m_CurrentAlpha;
    static float                    m_Timer;
    static float                    m_Duration;
    static XMFLOAT4                 m_FadeColor;

    static ID3D11VertexShader*      m_VS;
    static ID3D11PixelShader*       m_PS;
    static ID3D11Buffer*            m_CBuffer;
    static ID3D11BlendState*        m_BlendState;
    static ID3D11DepthStencilState* m_DepthState;
    static ID3D11RasterizerState*   m_RasterizerState; // カリング無効用ステート

public:
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // フェードアウト開始 (画面全体を指定色へ暗転)
    static void StartFadeOut(float duration = 0.4f, XMFLOAT4 color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));

    // フェードイン開始 (暗転状態から明転)
    static void StartFadeIn(float duration = 0.4f, XMFLOAT4 color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));

    // フレーム更新
    static void Update(float deltaTime = 1.0f / 60.0f);

    // フェードオーバーレイの描画
    static void Draw(ID3D11DeviceContext* context);

    // 状態取得関数
    static FadeState GetState() { return m_State; }
    static bool IsFading() { return m_State != FadeState::None; }
    static bool IsFadeOutComplete() { return m_State == FadeState::FadeOut && m_CurrentAlpha >= 1.0f; }
    static bool IsFadeInComplete() { return m_State == FadeState::FadeIn && m_CurrentAlpha <= 0.0f; }
    static float GetAlpha() { return m_CurrentAlpha; }
};
