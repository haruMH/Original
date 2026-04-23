#pragma once
#include "main.h"

class GameObject
{
protected:
    XMFLOAT3 m_Position; // 位置
    XMFLOAT3 m_Rotation; // 回転
    XMFLOAT3 m_Scale;    // 大きさ
    XMFLOAT3 m_Size;     // 当たり判定のサイズ

    bool m_Destroy;      // trueになるとManagerによって削除される

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

    // ... 省略 ...
    virtual void Init() = 0;
    virtual void Uninit() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;

    // ゲッターとセッター
    XMFLOAT3 GetPosition() const { return m_Position; }
    void SetPosition(XMFLOAT3 pos) { m_Position = pos; }

    XMFLOAT3 GetSize() const { return m_Size; }
    void SetSize(XMFLOAT3 size) { m_Size = size; }
    
    bool IsDestroy() const { return m_Destroy; }
    void SetDestroy() { m_Destroy = true; }
};
