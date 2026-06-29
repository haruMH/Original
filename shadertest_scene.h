#pragma once
#include "scene_interface.h"

// =================================================================
// シェーダーテストシーンクラス (ShaderTestScene)
// =================================================================
// 特殊シェーダー（水面、ディゾルブ、屈折など）の描画確認を行う
// デバッグ用のシーンを制御します。
class ShaderTestScene : public IScene
{
public:
    ShaderTestScene();
    ~ShaderTestScene() override;

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

private:
    // 特殊ステージ用の毎フレーム更新処理
    void UpdateShaderTest();
};
