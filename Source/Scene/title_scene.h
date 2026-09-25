#pragma once
#include "scene_interface.h"

// =================================================================
// タイトルシーンクラス (TitleScene)
// =================================================================
// ゲーム起動時のタイトル画面を制御します。
// スペースキーまたは'T'キーの入力を監視してシーン遷移をトリガーします。
class TitleScene : public IScene
{
public:
    TitleScene();
    ~TitleScene() override;

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

private:
    float m_OrbitAngle = 0.0f;       // カメラ旋回用アングル
    int   m_SelectedIndex = 0;      // 0: つづきから, 1: はじめから
    bool  m_HasSaveData = false;    // セーブデータの有無
    bool  m_ShowNewGameConfirm = false; // 「はじめから」上書き確認
};
