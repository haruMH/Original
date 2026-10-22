#pragma once
#include "main.h"
#include "game_constants.h"

class Camera
{
private:
    XMFLOAT3 m_Position; // カメラの位置
    XMFLOAT3 m_Target;   // 注視点
    XMFLOAT3 m_Up;       // 上方向ベクトル

    float m_AngleX;      // ピッチ角（上下）
    float m_AngleY;      // ヨー角（左右）
    float m_Distance;    // プレイヤーとの距離

    // カメラ感度・操作設定
    static float m_Sensitivity; // マウス感度（デフォルト 0.002f）
    static bool  m_InvertY;     // 上下反転（デフォルト false）

    float m_ShakeIntensity = 0.0f; // 振動の強さ
    int   m_ShakeTimer = 0;        // 振動の残り時間

    // カットシーン補間用
    bool     m_IsCutsceneMode = false;
    XMFLOAT3 m_CutsceneEye = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT3 m_CutsceneTarget = XMFLOAT3(0.0f, 0.0f, 0.0f);
    float    m_InterpolationFactor = 0.0f; // 0.0=通常, 1.0=カットシーン

    // 描画補間（高FPS時のスタッター解消）用
    XMFLOAT3 m_PrevPosition;
    XMFLOAT3 m_PrevTarget;

public:
    void Init();
    void Uninit();
    void Update();
    void Draw();
    void ApplyViewMatrix(float alpha = 1.0f);

    void Shake(float intensity, int duration) {
        m_ShakeIntensity = intensity;
        m_ShakeTimer = duration;
    }

    float GetAngleY() const { return m_AngleY; }
    float GetAngleX() const { return m_AngleX; }
    void  SetAngles(float angleX, float angleY) {
        m_AngleX = angleX;
        m_AngleY = angleY;
    }
    void  SetDistance(float distance) { m_Distance = distance; }

    // カメラ感度・上下反転設定
    static float GetSensitivity() { return m_Sensitivity; }
    static void  SetSensitivity(float sens) { m_Sensitivity = sens; }
    static bool  GetInvertY() { return m_InvertY; }
    static void  SetInvertY(bool invert) { m_InvertY = invert; }

    // ゲッター（必要に応じて）
    XMFLOAT3 GetPosition() { return m_Position; }
    XMFLOAT3 GetForward() const;

    // カットシーン設定用
    void SetCutsceneMode(bool enable, XMFLOAT3 eye = XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3 target = XMFLOAT3(0.0f, 0.0f, 0.0f))
    {
        if (enable && !m_IsCutsceneMode) {
            // カットシーン開始
        }
        else if (!enable && m_IsCutsceneMode) {
            // カットシーン終了時: ボス正面（Z+方向）を捉える標準アングルにリセット
            m_AngleX = Constants::Camera::CUTSCENE_RESET_ANGLE_X;
            m_AngleY = Constants::Camera::CUTSCENE_RESET_ANGLE_Y;
            m_InterpolationFactor = 0.0f; // 演出用補間係数を即時リセット
        }

        m_IsCutsceneMode = enable;
        if (enable) {
            m_CutsceneEye = eye;
            m_CutsceneTarget = target;
        }
    }
private:
    float m_SavedAngleX = 0.2f;
    float m_SavedAngleY = 0.0f;
};

// ゲームグローバルカメラ（manager.cpp で実体化）
extern Camera* g_Camera;
