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

public:
    void Init();
    void Uninit();
    void Update();
    void Draw();
};
