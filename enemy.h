#pragma once
#include "gameobject.h"

enum class EnemyState {
    NORMAL,    // 通常状態
    GRABBED,   // プレイヤーにつかまれている状態
    FLYING,    // 投げられて飛んでいる状態
    CHASING,   // プレイヤーを追尾している状態
};

class Enemy : public GameObject
{
private:
    ID3D11Buffer*             m_VertexBuffer = nullptr;
    ID3D11VertexShader*       m_VertexShader = nullptr;
    ID3D11PixelShader*        m_PixelShader  = nullptr;
    ID3D11InputLayout*        m_VertexLayout = nullptr;
    ID3D11ShaderResourceView* m_Texture      = nullptr;
    int m_VertexCount = 0;

    EnemyState m_EnemyState = EnemyState::NORMAL;
    XMFLOAT3   m_Velocity   = XMFLOAT3(0, 0, 0);
    float      m_VelocityY  = 0.0f;

    int        m_ScoreValue = 100;   // 撃破時のスコア加算値

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;

    EnemyState GetEnemyState() const         { return m_EnemyState; }
    void       SetEnemyState(EnemyState s)   { m_EnemyState = s; }
    void       SetVelocity(XMFLOAT3 v)       { m_Velocity = v; m_VelocityY = v.y; }

    // スコア値の取得・設定
    int        GetScoreValue() const         { return m_ScoreValue; }
    void       SetScoreValue(int v)          { m_ScoreValue = v; }
};