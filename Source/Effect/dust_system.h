#pragma once
#include "main.h"
#include <vector>

// =================================================================
// 砂埃パーティクル 1件のデータ構造体
// =================================================================
struct DustParticle {
    XMFLOAT3 Position;     // 現在位置
    XMFLOAT3 Velocity;     // 速度
    float    Scale;        // 現在スケール
    float    TargetScale;  // 到達目標スケール
    float    Alpha;        // 不透明度
    float    Rotation;     // ビルボード自転角 (Z軸回転)
    float    RotSpeed;     // 自転速度
    int      Life;         // 残り生存フレーム
    int      MaxLife;      // 最大生存フレーム
    XMFLOAT4 Color;        // パーティクル色
    bool     Active;       // 使用中フラグ
};

// =================================================================
// トゥーン砂埃パーティクルシステム（静的クラス）
// =================================================================
class DustSystem {
private:
    static const size_t MAX_DUST = 64;
    static DustParticle m_Particles[MAX_DUST];

    // GPU リソース
    static ID3D11Buffer*            m_QuadVB;       // クアッド頂点バッファ (ビルボード用)
    static ID3D11VertexShader*      m_VS;           // 頂点シェーダー
    static ID3D11PixelShader*       m_PS;           // ピクセルシェーダー
    static ID3D11InputLayout*       m_IL;           // 入力レイアウト
    static ID3D11DepthStencilState* m_DepthState;   // 深度ステート（テストON / 書き込みOFF）
    static ID3D11ShaderResourceView* m_DustTexture; // トゥーン雲型ソフト円テクスチャ

    // プロシージャルにソフト円形アルファテクスチャを生成
    static ID3D11ShaderResourceView* CreateDustTexture(ID3D11Device* device);

public:
    static void Init();
    static void Uninit();
    static void Clear();
    static void Update();
    static void Draw();

    // 走行時の砂埃（足元から進行方向の逆へふわっと出る）
    static void AddRunDust(const XMFLOAT3& footPos, const XMFLOAT3& moveDir);

    // 着地時の砂埃（足元から周囲へ円形に広がる）
    static void AddLandDust(const XMFLOAT3& footPos, int count = 6);

    // ダッシュ時の砂埃（勢いよく後方へ吹き出す）
    static void AddDashDust(const XMFLOAT3& footPos, const XMFLOAT3& dashDir);
};
