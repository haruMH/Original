#pragma once
#include "gameobject.h"

// =================================================================
// Item クラス
// =================================================================
// プレイヤーが取得することでスピン吸い込み効果を発動できるパワーアップアイテム
class Item : public GameObject
{
private:
    float m_RotationSpeed = 2.0f; // 回転アニメーション速度

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
