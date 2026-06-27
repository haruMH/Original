#pragma once
#include "gameobject.h"

class Wall : public GameObject
{
public:
    enum class EffectType {
        Normal,
        Water,
        Dissolve,
        Refraction
    };

private:
    ID3D11ShaderResourceView* m_Texture = nullptr;
    bool                      m_IsShaderTest = false; // テストシェーダーを使用するか

    EffectType m_EffectType = EffectType::Normal;

    // 水面用パラメータ
    float    m_WaterTime = 0.0f;
    XMFLOAT3 m_WaveParams = XMFLOAT3(0.08f, 3.0f, 2.0f); // 振幅, 周波数, 速度
    float    m_WaterShininess = 32.0f;
    float    m_WaterFresnelPower = 4.0f;
    XMFLOAT4 m_WaterColorShallow = XMFLOAT4(0.0f, 0.7f, 0.8f, 0.5f);
    XMFLOAT4 m_WaterColorDeep = XMFLOAT4(0.0f, 0.1f, 0.3f, 0.9f);
    XMFLOAT2 m_ScrollSpeed1 = XMFLOAT2(0.03f, 0.01f);
    XMFLOAT2 m_ScrollSpeed2 = XMFLOAT2(-0.02f, 0.04f);

    // ディゾルブ用パラメータ
    float    m_DissolveThreshold = 0.0f;
    float    m_DissolveEdgeWidth = 0.04f;
    XMFLOAT4 m_DissolveEdgeColor = XMFLOAT4(4.0f, 0.8f, 0.0f, 1.0f); // HDR発光のオレンジ
    ID3D11ShaderResourceView* m_NoiseTexture = nullptr;

    // 屈折ガラス用パラメータ
    float    m_RefractionIndex = 0.03f;
    float    m_RefractFresnelPower = 4.0f;
    XMFLOAT4 m_RefractHighlightColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;
    static ObjectType GetStaticType() { return ObjectType::Wall; }
    ObjectType GetObjectType() const override { return GetStaticType(); }

    void SetShaderTest(bool enable) {
        m_IsShaderTest = enable;
        if (enable) {
            // テストシェーダー使用時は、RenderSystemによる一括インスタンス描画から除外する
            m_RenderComponent.visible = false;
        }
    }
    bool IsShaderTest() const { return m_IsShaderTest; }

    // 鏡パラメータゲッター
    EffectType GetEffectType() const { return m_EffectType; }
    float GetRefractionIndex() const { return m_RefractionIndex; }
    float GetRefractFresnelPower() const { return m_RefractFresnelPower; }
    XMFLOAT4 GetRefractHighlightColor() const { return m_RefractHighlightColor; }
    // 水面パラメータゲッター
    float GetWaterTime() const { return m_WaterTime; }
    XMFLOAT3 GetWaveParams() const { return m_WaveParams; }
    float GetWaterShininess() const { return m_WaterShininess; }
    float GetWaterFresnelPower() const { return m_WaterFresnelPower; }
    XMFLOAT4 GetWaterColorShallow() const { return m_WaterColorShallow; }
    XMFLOAT4 GetWaterColorDeep() const { return m_WaterColorDeep; }
    XMFLOAT2 GetScrollSpeed1() const { return m_ScrollSpeed1; }
    XMFLOAT2 GetScrollSpeed2() const { return m_ScrollSpeed2; }
    // Wall ブロック自体の表示/非表示を切り替える
    void SetVisible(bool visible) { m_RenderComponent.visible = visible; }
};
