#pragma once
#include "gameobject.h"

// アイテムの種類
enum class ItemType
{
    VACUUM,      // 吸引（ブラックホール）
    GIGANT,      // 巨大化
    LIGHTNING    // 雷電
};

// =================================================================
// Item クラス
// =================================================================
// プレイヤーが取得することで各種効果を発動できるパワーアップアイテム
class Item : public GameObject
{
private:
    float    m_RotationSpeed = 2.0f; // 回転アニメーション速度
    ItemType m_Type = ItemType::VACUUM; // アイテムの種類

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    ItemType GetItemType() const { return m_Type; }
    void     SetItemType(ItemType type) { m_Type = type; }
};

