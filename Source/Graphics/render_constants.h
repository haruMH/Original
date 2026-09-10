#pragma once
#include <DirectXMath.h>

// =================================================================
// レンダリング・シェーダー関連の定数定義 (RenderConstants)
// =================================================================
namespace RenderConstants {

// シャドウマップ関連
namespace Shadow {
constexpr unsigned int MAP_WIDTH = 2048;
constexpr unsigned int MAP_HEIGHT = 2048;
constexpr float PROJECTION_SIZE = 65.0f;
constexpr float NEAR_Z = 60.0f;
constexpr float FAR_Z = 150.0f;
constexpr float LIGHT_OFFSET_Y = 100.0f;
} // namespace Shadow

// ポストエフェクト・ブルーム関連
namespace PostProcess {
constexpr int DOWNSAMPLE_SCALE = 4;
constexpr float DEFAULT_THRESHOLD = 0.7f;
constexpr float DEFAULT_BLUR_INTENSITY = 1.5f;
} // namespace PostProcess

// アウトライン（輪郭線）描画関連
namespace Outline {
constexpr float DEFAULT_WIDTH = 0.01f;
inline const DirectX::XMFLOAT4 COLOR_BLACK = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
constexpr int DEPTH_BIAS = 1000;
constexpr float SLOPE_SCALED_DEPTH_BIAS = 1.0f;
} // namespace Outline

// デフォルトライティング・フォグ環境設定
namespace DefaultLight {
inline const DirectX::XMFLOAT4 LIGHT_DIRECTION = DirectX::XMFLOAT4(0.45f, -0.85f, 0.35f, 0.0f);
inline const DirectX::XMFLOAT4 LIGHT_DIFFUSE   = DirectX::XMFLOAT4(1.0f, 0.95f, 0.9f, 1.0f);
inline const DirectX::XMFLOAT4 LIGHT_AMBIENT   = DirectX::XMFLOAT4(0.4f, 0.5f, 0.7f, 1.0f);
inline const DirectX::XMFLOAT4 LIGHT_FOG_COLOR = DirectX::XMFLOAT4(0.6f, 0.8f, 1.0f, 1.0f);
constexpr float FOG_START = 30.0f;
constexpr float FOG_END = 60.0f;
} // namespace DefaultLight

// インスタンス描画関連
namespace Instancing {
constexpr int MAX_INSTANCES = 512;
} // namespace Instancing

// 水面・特殊エフェクトシェーダー規定値
namespace ShaderDefault {

namespace Water {
constexpr float WAVE_AMPLITUDE = 0.15f;
constexpr float WAVE_FREQUENCY = 1.2f;
constexpr float WAVE_SPEED = 1.5f;
constexpr float SHININESS = 64.0f;
constexpr float FRESNEL_POWER = 4.0f;
inline const DirectX::XMFLOAT4 SHALLOW_COLOR = DirectX::XMFLOAT4(0.1f, 0.5f, 0.8f, 0.8f);
inline const DirectX::XMFLOAT4 DEEP_COLOR    = DirectX::XMFLOAT4(0.02f, 0.15f, 0.4f, 0.95f);
inline const DirectX::XMFLOAT2 SCROLL_SPEED1 = DirectX::XMFLOAT2(0.03f, 0.02f);
inline const DirectX::XMFLOAT2 SCROLL_SPEED2 = DirectX::XMFLOAT2(-0.02f, 0.03f);
} // namespace Water

namespace Dissolve {
constexpr float DEFAULT_EDGE_WIDTH = 0.05f;
inline const DirectX::XMFLOAT4 DEFAULT_EDGE_COLOR = DirectX::XMFLOAT4(1.0f, 0.4f, 0.1f, 1.0f);
} // namespace Dissolve

namespace Glass {
constexpr float REFRACTION_INDEX = 1.15f;
constexpr float FRESNEL_POWER = 3.0f;
inline const DirectX::XMFLOAT4 HIGHLIGHT_COLOR = DirectX::XMFLOAT4(0.9f, 0.95f, 1.0f, 0.3f);
} // namespace Glass

} // namespace ShaderDefault

} // namespace RenderConstants