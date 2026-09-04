#pragma once
#include "scene_interface.h"
#include <d3d11.h>
#include <directxmath.h>
#include <string>

class Player;
class Enemy;

// チュートリアルの進行ステップ
enum class TutorialStep
{
    MOVE_AND_DASH, // ステップ1: 移動・ダッシュ (WASD / Shift)
    GRAB_AND_THROW,// ステップ2: 敵を掴む・投げる (左クリック)
    SPIN_ATTACK,   // ステップ3: 敵を掴んで高速スピン (右クリック)
    PARRY,         // ステップ4: 敵弾のガード・パリィ (右クリックホールド / 直前ガード)
    WARP_SLASH,    // ステップ5: スロー中のワープスラッシュ (スロー中に左クリック)
    COMPLETE       // 全行程完了（本編へ遷移）
};

// =================================================================
// チュートリアルシーンクラス (TutorialScene)
// =================================================================
class TutorialScene : public IScene
{
private:
    TutorialStep m_CurrentStep = TutorialStep::MOVE_AND_DASH;
    int          m_StepTimer = 0;              // ステップ内の経過時間/ディレイタイマー
    int          m_TransitionDelay = 0;        // ステップ完了時の次への遷移ディレイ
    bool         m_StepCleared = false;        // 現在のステップを達成したフラグ
    float        m_InitialPlayerDistance = 0.0f; // 移動判定用の初期距離
    DirectX::XMFLOAT3 m_StartPlayerPos = { 0.0f, 0.0f, 0.0f };

    // ガイドテクスチャ関連 (GDIで作成)
    ID3D11ShaderResourceView* m_GuideTexture = nullptr;
    ID3D11ShaderResourceView* m_SkipTexture  = nullptr;
    ID3D11Buffer*            m_QuadVB       = nullptr;
    ID3D11VertexShader*      m_VS           = nullptr;
    ID3D11PixelShader*       m_PS           = nullptr;
    ID3D11InputLayout*       m_IL           = nullptr;
    ID3D11DepthStencilState* m_DepthState   = nullptr;

    std::wstring m_LastMainText;
    std::wstring m_LastSubText;

    bool         m_HasPendingStep = false;
    TutorialStep m_PendingStep = TutorialStep::MOVE_AND_DASH;

    // ステップごとのセットアップ
    void RequestNextStep(TutorialStep nextStep);
    void SetupStep(TutorialStep step);
    void UpdateStepLogic();
    void CompleteCurrentStep();

    // ガイドUIテクスチャ生成
    void CreateGuideTexture(const std::wstring& mainText, const std::wstring& subText);
    void CreateSkipTexture();
    void DrawUIQuad(ID3D11ShaderResourceView* srv, float posX, float posY, float width, float height, DirectX::XMFLOAT4 emission);

public:
    TutorialScene();
    ~TutorialScene() override;

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
