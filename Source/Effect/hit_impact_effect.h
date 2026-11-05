#pragma once
#include "main.h"
#include <wrl/client.h>

// =================================================================
// 命中エフェクト管理クラス（静的クラス）
// =================================================================
// 投げた敵が命中した瞬間に、パワー(0〜1)に応じた「フラッシュ・衝撃リング・火花」を出す。
// パワーが高いほど大きく、本数・数が増え、段階色(AuraEffect::StageColor)になる。
// Renderer::End のポスト合成後に描かれる（ブラー・ブルームの影響を受けない）。
class HitImpactEffect {
private:
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_QuadVB;
    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_VS;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_PS;        // hit_impact_ps.cso
    static Microsoft::WRL::ComPtr<ID3D11InputLayout>        m_IL;
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthOff;  // 深度テストOFF（命中点は物体に埋まるため）

public:
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // 命中を登録する。dir=飛んできた方向、power=スピン威力比(0〜1)
    static void Spawn(const XMFLOAT3& pos, const XMFLOAT3& dir, float power);

    // 威力比から段階(0〜4)を求める（SPIN_STAGE_GUARANTEE 基準）
    static int TierFromPower(float power);

    // 毎フレーム描画（内部で経過フレームも進める）
    static void Draw();
};
