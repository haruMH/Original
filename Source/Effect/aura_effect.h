#pragma once
#include "main.h"
#include <wrl/client.h>

// =================================================================
// チャージオーラエフェクト管理クラス（静的クラス）
// =================================================================
// スピン中のプレイヤーを包む発光オーラを描画する。
// チャージ段階(1〜4)が上がるほどオーラが大きく濃くなり、色が変わる。
class AuraEffect {
private:
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_QuadVB;     // XZ平面のクアッド頂点バッファ
    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_VS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_PS;
    static Microsoft::WRL::ComPtr<ID3D11InputLayout>        m_IL;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthState; // テストON / 書き込みOFF
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_Texture;    // ソフトグロー用グラデーション
    static int                                              m_Time;       // 揺らぎ用フレームカウンタ

    static bool CreateGlowTexture(ID3D11Device* device);

public:
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // 毎フレーム描画（プレイヤーのチャージ段階に応じて内部で描画可否を判断）
    static void Draw();

    // チャージ段階ごとの発光色（0=通常スピンのゴールド）
    static XMFLOAT3 StageColor(int stage);
};
