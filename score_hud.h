#pragma once
#include "main.h"

// =================================================================
// トータルスコアHUD描画クラス（静的クラス）
// =================================================================
class ScoreHUD {
private:
    static ID3D11Buffer*            m_QuadVB;       // クアッド頂点バッファ
    static ID3D11VertexShader*      m_VS;           // 頂点シェーダー (vertexShader.csoを流用)
    static ID3D11PixelShader*       m_PS;           // ピクセルシェーダー (ui_ps.csoを流用)
    static ID3D11InputLayout*       m_IL;           // 入力レイアウト
    static ID3D11DepthStencilState* m_DepthState;   // 深度ステート（テストOFF / 書き込みOFF）
    static ID3D11ShaderResourceView* m_Texture;     // スコア描画テクスチャ
    static ID3D11ShaderResourceView* m_TitleTexture;    // タイトル描画テクスチャ
    static ID3D11ShaderResourceView* m_ClearTexture;    // クリア画面描画テクスチャ
    static ID3D11ShaderResourceView* m_GameOverTexture; // ゲームオーバー描画テクスチャ

    static int   m_LastScore;                       // 前回のスコア値
    static int   m_LastHP;                          // 前回のプレイヤーHP値
    static float m_ScaleEffect;                     // スコア増加時のスケール演出値

    // GDI でスコアHUDのテクスチャを作成する
    static ID3D11ShaderResourceView* CreateHUDTexture(
        ID3D11Device* device,
        int score,
        int hp
    );

    // GDI でタイトル画面のテクスチャを作成する
    static ID3D11ShaderResourceView* CreateTitleTexture(
        ID3D11Device* device
    );

    // GDI でリザルト画面のテクスチャを作成する
    static ID3D11ShaderResourceView* CreateResultTexture(
        ID3D11Device* device,
        bool isClear,
        int score
    );

public:
    // 初期化・終了処理
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // 毎フレーム更新処理（スケール演出のイージングなど）
    static void Update();

    // 毎フレーム描画処理（Renderer::End() の直前に呼ぶ）
    static void Draw();
};
