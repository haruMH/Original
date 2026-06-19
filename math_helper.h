// math_helper.h
// XMFLOAT3の演算子オーバーロードと数学ユーティリティ関数。
// 注意: 演算子は、"using namespace DirectX" が有効な場合に XMVECTOR の演算子との
//       曖昧さを避けるために、DirectX 名前空間内に定義されています。
#pragma once
#include <directxmath.h>
#include <cmath>

namespace DirectX {
    inline XMFLOAT3 operator+(const XMFLOAT3& a, const XMFLOAT3& b) {
        return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
    }
    inline XMFLOAT3 operator-(const XMFLOAT3& a, const XMFLOAT3& b) {
        return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
    }
    inline XMFLOAT3 operator*(const XMFLOAT3& a, float s) {
        return XMFLOAT3(a.x * s, a.y * s, a.z * s);
    }
    inline XMFLOAT3 operator*(float s, const XMFLOAT3& a) {
        return XMFLOAT3(a.x * s, a.y * s, a.z * s);
    }
    inline XMFLOAT3 operator/(const XMFLOAT3& a, float s) {
        return XMFLOAT3(a.x / s, a.y / s, a.z / s);
    }
    inline XMFLOAT3& operator+=(XMFLOAT3& a, const XMFLOAT3& b) {
        a.x += b.x; a.y += b.y; a.z += b.z; return a;
    }
    inline XMFLOAT3& operator-=(XMFLOAT3& a, const XMFLOAT3& b) {
        a.x -= b.x; a.y -= b.y; a.z -= b.z; return a;
    }
    inline XMFLOAT3& operator*=(XMFLOAT3& a, float s) {
        a.x *= s; a.y *= s; a.z *= s; return a;
    }
    inline XMFLOAT3& operator/=(XMFLOAT3& a, float s) {
        a.x /= s; a.y /= s; a.z /= s; return a;
    }
}

namespace MathHelper {

    // 閾値（epsilon）を下回る場合は値をゼロにクランプする
    inline void ClearIfNearZero(float& value, float epsilon = 0.001f) {
        if (std::abs(value) < epsilon) value = 0.0f;
    }

    // XZ成分のみをスケーリングする
    inline void ScaleXZ(DirectX::XMFLOAT3& v, float s) {
        v.x *= s; v.z *= s;
    }

    // 線形補間 (float)
    inline float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    // 線形補間 (XMFLOAT3)
    inline DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t) {
        return DirectX::XMFLOAT3(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t));
    }

    // ベクトルの長さ（ノルム）
    inline float Length(const DirectX::XMFLOAT3& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    // ベクトルの長さの二乗（平方根計算を避けるため高速）
    inline float LengthSq(const DirectX::XMFLOAT3& v) {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    // ベクトルの正規化。長さがゼロに近い場合はゼロベクトルを返す
    inline DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& v) {
        float len = Length(v);
        if (len > 0.0001f) return v / len;
        return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    // 2点間の距離の二乗（平方根計算を避けるため高速）
    inline float DistanceSq(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // 2点間の距離
    inline float Distance(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        return std::sqrt(DistanceSq(a, b));
    }

} // namespace MathHelper
