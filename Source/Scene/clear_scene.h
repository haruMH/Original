#pragma once
#include "scene_interface.h"

// =================================================================
// クリアシーンクラス (ClearScene)
// =================================================================
// ゲームクリア画面を制御します。
// エンターキー入力を監視して、タイトル画面へと戻る処理を行います。
class ClearScene : public IScene
{
public:
    ClearScene();
    ~ClearScene() override;

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
