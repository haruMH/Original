#pragma once
#include "main.h"
#include <list>

// =================================================================
// 衝撃波 1件分のデータ構造体
// =================================================================
struct ShockwaveEntry {
    XMFLOAT3 Position;     // 衝撃波の中心座標
    int      Timer;        // 生存フレーム数カウンタ
    int      MaxTimer;     // 最大生存フレーム（フェード計算用）
    float    MaxRadius;    // 最大半径（スケールサイズ）
    float    ColorR;       // 発光色 R
    float    ColorG;       // 発光色 G
    float    ColorB;       // 発光色 B
    int      Delay;        // 開始ディレイ（待機フレーム数）
    bool     PhysicsApplied; // 物理衝撃波が既に適用されたか
    float    Force;        // 物理衝撃波の威力
    bool     Shrink;       // 収縮するエフェクトか
    bool     Used;         // 使用中フラグ (メモリプール用)
};

// =================================================================
// 衝撃波エフェクト＆物理処理管理クラス（静的クラス）
// =================================================================
class ShockwaveSystem {
private:
    static const size_t POOL_SIZE = 32;
    static ShockwaveEntry m_Shockwaves[POOL_SIZE];

    // GPU リソース
    static ID3D11Buffer*            m_QuadVB;       // 地面に水平なクアッド頂点バッファ
    static ID3D11VertexShader*      m_VS;           // 頂点シェーダー (vertexShader.cso)
    static ID3D11PixelShader*       m_PS;           // ピクセルシェーダー (ui_ps.cso)
    static ID3D11InputLayout*       m_IL;           // 入力レイアウト
    static ID3D11DepthStencilState* m_DepthState;   // 深度ステート（テストON / 書き込みOFF）
    static ID3D11ShaderResourceView* m_Texture;     // ドーナツ型リングテクスチャ

    // GDI を用いてドーナツ型リングのアルファテクスチャを作成する
    static ID3D11ShaderResourceView* CreateRingTexture(ID3D11Device* device);

    // 周囲の敵をなぎ倒す物理吹き飛ばし処理を適用する
    static void ApplyShockwavePhysics(const XMFLOAT3& center, float radius, float forceVal);

public:
    // 初期化・終了処理
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // 毎フレーム更新処理
    static void Update();

    // 毎フレーム描画処理（Renderer::End() の直前に呼ぶ）
    static void Draw();

    // 衝撃波を発生させる（同時に周囲への物理吹き飛ばしもトリガーする） - 従来版
    static void AddShockwave(
        const XMFLOAT3& pos, 
        float maxRadius, 
        float colorR, 
        float colorG, 
        float colorB,
        int duration = 24,
        float force = 1.0f,
        int delay = 0
    );

    // 衝撃波を発生させる - 収縮オプション付き
    static void AddShockwave(
        const XMFLOAT3& pos, 
        float maxRadius, 
        float colorR, 
        float colorG, 
        float colorB,
        int duration,
        float force,
        int delay,
        bool shrink
    );
};
