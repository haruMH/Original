#pragma once
#include "scene_interface.h"

// =================================================================
// ゲームプレイシーンクラス (GameplayScene)
// =================================================================
// ゲームのメインアクションパート（戦闘や移動）を制御します。
// プレイヤー、敵、壁などのオブジェクト更新、衝突判定、およびクリア条件の監視を担当します。
class GameplayScene : public IScene
{
public:
    GameplayScene();
    ~GameplayScene() override;

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

private:
    // ゲームプレイ本編の毎フレーム更新処理（旧 Manager::UpdateGameplay）
    void UpdateGameplay();

    int m_ClearDelayTimer = 0; // ボス撃破後のクリア移行遅延タイマー（フレーム数）
};
