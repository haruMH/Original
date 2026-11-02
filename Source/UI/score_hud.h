#pragma once
#include "main.h"
#include <functional>
#include <wrl/client.h>

// =================================================================
// トータルスコアHUD描画クラス（静的クラス）
// =================================================================
class ScoreHUD {
private:
    static Microsoft::WRL::ComPtr<ID3D11Buffer>             m_QuadVB;           // クアッド頂点バッファ
    static Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_VS;               // 頂点シェーダー (vertexShader.csoを流用)
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_PS;               // ピクセルシェーダー (ui_ps.csoを流用)
    static Microsoft::WRL::ComPtr<ID3D11InputLayout>        m_IL;               // 入力レイアウト
    static Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_DepthState;       // 深度ステート（テストOFF / 書き込みOFF）
    static Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_HUDTex2D;         // タイムHUD用動的テクスチャ（毎フレーム再生成防止）
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_Texture;          // スコア描画テクスチャ
    static HDC                                              m_HudDC;            // タイムHUD用キャッシュDC
    static HBITMAP                                          m_HudBmp;           // タイムHUD用キャッシュDIB
    static HGDIOBJ                                          m_HudOldBmp;        // タイムHUD用旧ビットマップ
    static BYTE*                                            m_HudBits;          // タイムHUD用DIBピクセルバッファ
    static HFONT                                            m_HudFontBadge;     // タイムHUD用バッジフォント
    static HFONT                                            m_HudFontScore;     // タイムHUD用スコアフォント
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_TitleTexture;     // タイトル描画テクスチャ
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_ClearTexture;     // クリア画面描画テクスチャ
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_GameOverTexture;  // ゲームオーバー描画テクスチャ
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_BossHPTexture;    // ボスHPバーテクスチャ
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_PlayerHPTexture;  // プレイヤーHPバーテクスチャ
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_WhiteTexture;     // 単色ゲージ描画用の1x1白テクスチャ

    static int   m_LastTimeFrames;                  // 前回の表示フレーム数
    static int   m_LastHP;                          // 前回のプレイヤーHP値
    static int   m_LastMaxHP;                       // 前回のプレイヤー最大HP値
    static int   m_LastBossHP;                      // 前回のボスHP値
    static int   m_LastBossMaxHP;                   // 前回のボス最大HP値
    static float m_ScaleEffect;                     // タイム短縮等のスケール演出値

    // GDI DIB描画からD3D11 SRVを生成する共通ヘルパー
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateGdiTexture(
        ID3D11Device* device,
        int width,
        int height,
        int lumThreshold,
        int baseAlpha,
        const std::function<void(HDC)>& drawCallback
    );

    // 2D スプライトクアッドの共通描画ヘルパー
    static void DrawQuad2D(
        ID3D11ShaderResourceView* srv,
        float posX,
        float posY,
        float width,
        float height,
        const DirectX::XMMATRIX& view2D,
        const DirectX::XMMATRIX& proj2D,
        const DirectX::XMFLOAT4& tint = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    // 投擲チャージ段階ゲージと非回転投げクールダウンゲージの描画
    static void DrawChargeGauge(
        float screenW,
        float screenH,
        const DirectX::XMMATRIX& view2D,
        const DirectX::XMMATRIX& proj2D
    );

    // GDI でタイムHUDのテクスチャを作成・動的更新する
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateHUDTexture(
        ID3D11Device* device,
        int frames
    );
    static void UpdateHUDTexture(int frames);

    // GDI でタイトル画面のテクスチャを作成する
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateTitleTexture(
        ID3D11Device* device
    );

    // GDI でリザルト画面のテクスチャを作成する
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateResultTexture(
        ID3D11Device* device,
        bool isClear,
        int frames
    );

    // GDI でボスHPバーテクスチャを作成する
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateBossHPTexture(
        ID3D11Device* device,
        int hp,
        int maxHp
    );

    // GDI でプレイヤーHPバーテクスチャを作成する
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreatePlayerHPTexture(
        ID3D11Device* device,
        int hp,
        int maxHp
    );

    // UIシェイクパラメータ
    static float m_ShakeIntensity;
    static int   m_ShakeTimer;
    static DirectX::XMFLOAT2 m_ShakeOffset;

public:
    // 初期化・終了処理
    static bool Init(ID3D11Device* device);
    static void Uninit();

    // 毎フレーム更新処理（スケール演出のイージングなど）
    static void Update();

    // 毎フレーム描画処理（Renderer::End() の直前に呼ぶ）
    static void Draw();

    // UIシェイクの発生（強い打撃時やボス被弾時にUI全体を振動させる）
    static void Shake(float intensity = 10.0f, int frames = 8);
};
