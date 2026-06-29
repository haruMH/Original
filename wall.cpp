#include "wall.h"
#include "renderer.h"
#include "resource_manager.h"
#include "imgui/imgui.h"

void Wall::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_IsShaderTest = false;

    m_EffectType = EffectType::Normal;
    m_WaterTime = 0.0f;

    m_Texture = ResourceManager::GetTexture("grid.png");

    // ディゾルブ用のノイズとしてplayer.pngを流用
#ifdef NDEBUG
    m_NoiseTexture = ResourceManager::GetTexture("Assets/texture/player.png");
#else
    m_NoiseTexture = ResourceManager::GetTexture("player.png");
#endif

    // コンポーネント指向での描画パラメータ初期化
    m_RenderComponent = RenderComponent("grid.png", MeshType::Cube, true);
}

void Wall::Uninit()
{
}

void Wall::Update()
{
    if (m_IsShaderTest)
    {
        // 時間経過を進める
        m_WaterTime += 1.0f / 60.0f;

        // ImGui UI の構築 (Always を指定して強制表示)
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(450.0f, 550.0f), ImGuiCond_Always);
        ImGui::Begin("Shader Test Controller");

        // エフェクト選択
        const char* effectNames[] = { 
            "Normal (Test Shader)", 
            "Water Effect", 
            "Dissolve Effect", 
            "Refraction / Reflection Glass" 
        };
        int currentEffect = (int)m_EffectType;
        if (ImGui::Combo("Effect Type", &currentEffect, effectNames, 4))
        {
            m_EffectType = (EffectType)currentEffect;
        }

        ImGui::Separator();

        if (m_EffectType == EffectType::Water)
        {
            ImGui::Text("--- Water Parameters ---");
            ImGui::SliderFloat("Wave Amplitude (Height)", &m_WaveParams.x, 0.0f, 0.5f);
            ImGui::SliderFloat("Wave Frequency (Density)", &m_WaveParams.y, 0.1f, 10.0f);
            ImGui::SliderFloat("Wave Speed", &m_WaveParams.z, 0.0f, 10.0f);
            ImGui::SliderFloat("Water Shininess", &m_WaterShininess, 1.0f, 256.0f);
            ImGui::SliderFloat("Fresnel Power (Reflect)", &m_WaterFresnelPower, 1.0f, 10.0f);
            ImGui::ColorEdit4("Color Shallow", (float*)&m_WaterColorShallow);
            ImGui::ColorEdit4("Color Deep", (float*)&m_WaterColorDeep);
            ImGui::SliderFloat2("Scroll Speed 1", (float*)&m_ScrollSpeed1, -0.2f, 0.2f);
            ImGui::SliderFloat2("Scroll Speed 2", (float*)&m_ScrollSpeed2, -0.2f, 0.2f);
        }
        else if (m_EffectType == EffectType::Dissolve)
        {
            ImGui::Text("--- Dissolve Parameters ---");
            ImGui::SliderFloat("Threshold", &m_DissolveThreshold, 0.0f, 1.0f);
            ImGui::SliderFloat("Edge Width", &m_DissolveEdgeWidth, 0.0f, 0.2f);
            ImGui::ColorEdit4("Edge Glow Color", (float*)&m_DissolveEdgeColor);
        }
        else if (m_EffectType == EffectType::Refraction)
        {
            ImGui::Text("--- Glass (Mirror) Parameters ---");
            ImGui::SliderFloat("Reflection/Refraction Index", &m_RefractionIndex, -0.1f, 0.1f);
            ImGui::SliderFloat("Fresnel Power (Edge)", &m_RefractFresnelPower, 1.0f, 10.0f);
            ImGui::ColorEdit4("Highlight Color", (float*)&m_RefractHighlightColor);
        }

        ImGui::End();
    }
}

void Wall::Draw()
{
    // 通常・シャドウ・アウトラインの描画は RenderSystem 側で一括描画するため、
    // テストモードではない時はここでの個別描画（36頂点）は行いません。
    if (m_IsShaderTest)
    {
        // ワールド行列を計算
        XMMATRIX scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
        XMMATRIX translation = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
        XMMATRIX world = scale * rotation * translation;

        switch (m_EffectType)
        {
        case EffectType::Normal:
            Renderer::DrawCubeWithTestShader(world, m_Texture);
            break;

        case EffectType::Water:
            // テスト用に法線マップ1, 2として m_Texture(grid.png) を流用
            Renderer::DrawCubeWithWaterShader(world, m_Texture, m_Texture, m_WaterTime, m_WaveParams, m_WaterShininess, m_WaterFresnelPower, m_WaterColorShallow, m_WaterColorDeep, m_ScrollSpeed1, m_ScrollSpeed2);
            break;

        case EffectType::Dissolve:
            Renderer::DrawCubeWithDissolveShader(world, m_Texture, m_NoiseTexture, m_DissolveThreshold, m_DissolveEdgeWidth, m_DissolveEdgeColor);
            break;

        case EffectType::Refraction:
            // 描画直前のシーンターゲット（背景）を退避コピー
            Renderer::CopySceneTexture();
            // テスト用に法線マップとして m_Texture を流用
            Renderer::DrawCubeWithRefractShader(world, m_Texture, m_RefractionIndex, m_RefractFresnelPower, m_RefractHighlightColor);
            break;
        }
    }
}
