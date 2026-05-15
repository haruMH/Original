#pragma once
#include "gameobject.h"

class Enemy;

enum class PlayerState {
    IDLE,
    ATTACK
};

class Player : public GameObject
{
private:
    ID3D11Buffer*       m_VertexBuffer = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader*  m_PixelShader = nullptr;
    ID3D11InputLayout*  m_VertexLayout = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;

    int m_VertexCount = 0;

    PlayerState m_State = PlayerState::IDLE;
    Enemy*      m_GrabbedEnemy = nullptr;
    float       m_AngularVelocity = 0.0f; // プレイヤーの旋回速度（遠心力用）

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    Enemy* GetGrabbedEnemy() const { return m_GrabbedEnemy; }
    void   SetGrabbedEnemy(Enemy* enemy) { m_GrabbedEnemy = enemy; m_State = PlayerState::ATTACK; }
    float  GetAngularVelocity() const { return m_AngularVelocity; }
    void   Throw();
};
