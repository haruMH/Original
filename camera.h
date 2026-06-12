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

    float m_ShakeIntensity = 0.0f; // 振動の強さ
    int   m_ShakeTimer = 0;        // 振動の残り時間

public:
    void Init();
    void Uninit();
    void Update();
    void Draw();

    void Shake(float intensity, int duration) {
        m_ShakeIntensity = intensity;
        m_ShakeTimer = duration;
    }

    float GetAngleY() const { return m_AngleY; }
    // ゲッター（必要に応じて）
    XMFLOAT3 GetPosition() { return m_Position; }
    XMFLOAT3 GetForward() const;
};

// ゲームグローバルカメラ（manager.cpp で実体化）
extern Camera* g_Camera;
