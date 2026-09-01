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

    // カットシーン補間用
    bool     m_IsCutsceneMode = false;
    XMFLOAT3 m_CutsceneEye = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 m_CutsceneTarget = XMFLOAT3(0.0f, 0.0f, 0.0f);
    float    m_InterpolationFactor = 0.0f; // 0.0=通常, 1.0=カットシーン

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

    // カットシーン設定用
    void SetCutsceneMode(bool enable, XMFLOAT3 eye = XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3 target = XMFLOAT3(0.0f, 0.0f, 0.0f))
    {
        m_IsCutsceneMode = enable;
        if (enable) {
            m_CutsceneEye = eye;
            m_CutsceneTarget = target;
        }
    }
};

// ゲームグローバルカメラ（manager.cpp で実体化）
extern Camera* g_Camera;
