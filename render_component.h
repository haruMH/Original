#pragma once
#include <string>

// 描画するメッシュの形状タイプ
enum class MeshType {
    Cube,   // 立方体
    // 必要に応じて Sphere, Cylinder 等を拡張可能
};

// =================================================================
// RenderComponent 構造体
// =================================================================
// 各GameObjectに持たせる描画用の軽量なコンポーネントです。
// 自身はDrawなどの描画ロジック（関数）を持たず、描画に必要なパラメータのみを保持します。
struct RenderComponent {
    std::string textureKey;  // テクスチャの識別キー (例: "enemy.png")
    MeshType    meshType;    // メッシュの形状タイプ (デフォルトは Cube)
    bool        visible;     // 表示フラグ (true で描画対象となる)

    // デフォルトコンストラクタ
    RenderComponent()
        : textureKey(""), meshType(MeshType::Cube), visible(true) {}

    // 初期化用コンストラクタ
    RenderComponent(const std::string& texKey, MeshType type = MeshType::Cube, bool isVisible = true)
        : textureKey(texKey), meshType(type), visible(isVisible) {}
};
