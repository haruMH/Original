#pragma once
#include "main.h"
#include "render_component.h"

// =================================================================
// オブジェクトの型識別用列挙型
// =================================================================
enum class ObjectType
{
    Player,
    Enemy,
    Wall,
    Item,
    Field,
    Bullet,
    Boss,
    Unknown
};

class GameObject
{
protected:
    XMFLOAT3 m_Position; // 位置
    XMFLOAT3 m_Rotation; // 回転
    XMFLOAT3 m_Scale;    // 大きさ
    XMFLOAT3 m_Size;     // 当たり判定のサイズ

    bool m_Destroy;      // trueになるとManagerによって削除される
    RenderComponent m_RenderComponent; // 描画コンポーネント

public:
    GameObject()
    {
        m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f); // デフォルトサイズ
        m_Destroy  = false;
    }
    virtual ~GameObject() {}

    virtual void Init() = 0;
    virtual void Uninit() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    RenderComponent& GetRenderComponent() { return m_RenderComponent; }
    const RenderComponent& GetRenderComponent() const { return m_RenderComponent; }

    // ゲッターとセッター
    XMFLOAT3 GetPosition() const { return m_Position; }
    void SetPosition(XMFLOAT3 pos) { m_Position = pos; }

	XMFLOAT3 GetRotation() const { return m_Rotation; }
    void SetRotation(XMFLOAT3 rot) { m_Rotation = rot; }

    XMFLOAT3 GetScale() const { return m_Scale; }
    void SetScale(XMFLOAT3 scale) { m_Scale = scale; }

    XMFLOAT3 GetSize() const { return m_Size; }
    void SetSize(XMFLOAT3 size) { m_Size = size; }
    
    // 便利な関数
    XMVECTOR GetForward() const {
        return XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z));
    }

    bool IsDestroy() const { return m_Destroy; }
    void SetDestroy() { m_Destroy = true; }
    virtual float GetRadius() const { return m_Size.x * m_Scale.x; }
    virtual XMFLOAT3 GetEmissive() const { return XMFLOAT3(0.0f, 0.0f, 0.0f); }
    virtual ObjectType GetObjectType() const { return ObjectType::Unknown; }
};