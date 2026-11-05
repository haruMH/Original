#pragma once
#include "main.h"
#include <wrl/client.h>

// =================================================================
// チャージオーラエフェクト管理クラス（静的クラス）
// =================================================================
// スピン中のプレイヤーの足元に、専用シェーダー(aura_ring_ps)で描く光のリングを出す。
// チャージ段階(1〜4)が上がるほど大きく濃くなり、色が変わる。
// 段階到達時はパルスリングが広がり、周回する光の筋はスピン速度で加速する。
class AuraEffect {
private:
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_WallVB;     // 光の壁/光柱用の円筒(両面)
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_QuadVB;     // XZ平面のクアッド頂点バッファ
    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_VS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_PS;         // aura_ring_ps.cso
    static Microsoft::WRL::ComPtr<ID3D11InputLayout>        m_IL;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthState; // テストON / 書き込みOFF
    static UINT                                             m_WallVertexCount;
    static int                                              m_Time;       // 経過フレーム
    static int                                              m_LastStage;
    static int                                              m_PulseTime;  // 段階到達からの経過フレーム
    static float                                            m_Fade;       // 出現/消滅フェード(0〜1)
    static int                                              m_HoldStage;  // 消滅中に見た目を保つ直前の段階

public:
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // 毎フレーム描画。Renderer::End のポスト合成後(ラジアルブラーの後)に呼ばれ、
    // バックバッファへ直接描くので最大回転中でも滲まない。
    static void Draw();

    // チャージ段階ごとの発光色（0=通常スピンのゴールド）
    static XMFLOAT3 StageColor(int stage);
};
