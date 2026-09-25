#pragma once
#include "scene_interface.h"

// =================================================================
// ゲームオーバーシーンクラス (GameOverScene)
// =================================================================
// ゲームオーバー画面を制御します。
// エンターキー入力を監視して、タイトル画面へと戻る処理を行います。
class GameOverScene : public IScene
{
public:
    GameOverScene();
    ~GameOverScene() override;

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

private:
    void DrawRewardUI();

    float m_OrbitAngle;    // カメラ水平旋回角
    float m_CameraHeight;  // カメラ高度
    float m_Timer;         // 演出タイマー
};
