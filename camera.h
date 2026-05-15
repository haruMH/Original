#pragma once
#include "main.h"

class Camera
{
private:
    XMFLOAT3 m_Position; // カメラの位置
    XMFLOAT3 m_Target;   // 注視点
    XMFLOAT3 m_Up;       // 上方向ベクトル

    float m_AngleX;      // ピッチ角（上下）
    float m_AngleY;      // ヨー角（左右）
    float m_Distance;    // プレイヤーとの距離

public:
    void Init();
    void Uninit();
    void Update();
    void Draw();


    float GetAngleY() const { return m_AngleY; }
    // ゲッター（必要に応じて）
    XMFLOAT3 GetPosition() { return m_Position; }
};

// ゲームグローバルカメラ（manager.cpp で実体化）
extern Camera* g_Camera;
