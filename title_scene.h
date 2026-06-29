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
};
