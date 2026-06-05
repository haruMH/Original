#pragma once
#include "main.h"
#include <list>
#include <unordered_map>

// =================================================================
// スコアポップアップ 1件分のデータ構造体
// =================================================================
struct ScorePopupEntry {
    float WorldX, WorldY, WorldZ;        // 3Dワールド発生座標
    int   Timer;                         // 残りフレーム数（0になると消滅）
    int   MaxTimer;                      // 総フレーム数（フェード計算用）
    float OffsetY;                       // Y方向浮き上がり累積量
    float EmitR, EmitG, EmitB;           // 発光カラー（Emission に設定する値）
    ID3D11ShaderResourceView* Texture;   // GDI生成テクスチャ（参照カウントを所有）
};

// =================================================================
// スコアポップアップ管理システム（静的クラス）
// =================================================================
class ScorePopupSystem {
private:
    static std::list<ScorePopupEntry> m_Popups;

    // GPU リソース
    static ID3D11Buffer*            m_QuadVB;      // 4頂点 TRIANGLESTRIP クアッド
    static ID3D11VertexShader*      m_VS;          // vertexShader.cso を流用
    static ID3D11PixelShader*       m_PS;          // ui_ps.cso（UI専用シェーダー）
    static ID3D11InputLayout*       m_IL;          // VERTEX_3D 入力レイアウト
    static ID3D11DepthStencilState* m_DepthState;  // 深度テストON / 書き込みOFF

    // テクスチャキャッシュ（スコア値 → SRV、複数ポップアップで共有する）
    static std::unordered_map<int, ID3D11ShaderResourceView*> m_TextureCache;

    // GDI でスコアテキストのアルファマスクテクスチャを生成する
    static ID3D11ShaderResourceView* CreateScoreTexture(
        ID3D11Device* device,
        const wchar_t* text
    );

public:
    // 初期化・終了処理
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // 毎フレーム更新処理
    static void Update();

    // 毎フレーム描画処理（Renderer::End() の直前に呼ぶこと）
    static void Draw();

    // スコアポップアップを発生させる（通常: 黄金色）
    static void AddPopup(float x, float y, float z, int score);

    // スコアポップアップをカラー指定で発生させる
    static void AddPopup(float x, float y, float z, int score,
                         float emitR, float emitG, float emitB);
};
