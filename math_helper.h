#pragma once
#include <directxmath.h>
#include <cmath>

// XMFLOAT3 の演算子オーバーロード
inline DirectX::XMFLOAT3 operator+(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
    return DirectX::XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline DirectX::XMFLOAT3 operator-(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
    return DirectX::XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline DirectX::XMFLOAT3 operator*(const DirectX::XMFLOAT3& a, float scalar) {
    return DirectX::XMFLOAT3(a.x * scalar, a.y * scalar, a.z * scalar);
}

inline DirectX::XMFLOAT3 operator*(float scalar, const DirectX::XMFLOAT3& a) {
    return DirectX::XMFLOAT3(a.x * scalar, a.y * scalar, a.z * scalar);
}

inline DirectX::XMFLOAT3 operator/(const DirectX::XMFLOAT3& a, float scalar) {
    return DirectX::XMFLOAT3(a.x / scalar, a.y / scalar, a.z / scalar);
}

inline DirectX::XMFLOAT3& operator+=(DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
    a.x += b.x; a.y += b.y; a.z += b.z;
    return a;
}

inline DirectX::XMFLOAT3& operator-=(DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
    a.x -= b.x; a.y -= b.y; a.z -= b.z;
    return a;
}

inline DirectX::XMFLOAT3& operator*=(DirectX::XMFLOAT3& a, float scalar) {
    a.x *= scalar; a.y *= scalar; a.z *= scalar;
    return a;
}

inline DirectX::XMFLOAT3& operator/=(DirectX::XMFLOAT3& a, float scalar) {
    a.x /= scalar; a.y /= scalar; a.z /= scalar;
    return a;
}

// 数学演算ヘルパー関数
namespace MathHelper {
    // 絶対値がしきい値(epsilon)未満なら、強制的にゼロにする
    inline void ClearIfNearZero(float& value, float epsilon = 0.001f) {
        if (std::abs(value) < epsilon) {
            value = 0.0f;
        }
    }
    // XZ軸（水平面）の成分だけにスカラを掛ける
    inline void ScaleXZ(DirectX::XMFLOAT3& v, float scalar) {
        v.x *= scalar;
        v.z *= scalar;
    }

    // 浮動小数点の線形補間（tは 0.0f ～ 1.0f の割合）
    inline float Lerp(float current, float target, float t) {
        return current + (target - current) * t;
    }

    // XMFLOAT3 の線形補間（位置や回転をじわっと近づける用）
    inline DirectX::XMFLOAT3 Lerp(const DirectX::XMFLOAT3& current, const DirectX::XMFLOAT3& target, float t) {
        return DirectX::XMFLOAT3(
            Lerp(current.x, target.x, t),
            Lerp(current.y, target.y, t),
            Lerp(current.z, target.z, t)
        );
    }


    // ベクトルの長さ（大きさ）を取得
    inline float Length(const DirectX::XMFLOAT3& v) {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    // ベクトルの長さの2乗を取得（高速な比較用）
    inline float LengthSq(const DirectX::XMFLOAT3& v) {
        return v.x * v.x + v.y * v.y + v.z * v.z;
    }

    // ベクトルの正規化
    inline DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& v) {
        float len = Length(v);
        if (len > 0.0001f) {
            return v / len;
        }
        return DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    // 2点間の距離の2乗を取得
    inline float DistanceSq(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // 2点間の距離を取得
    inline float Distance(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
        return std::sqrt(DistanceSq(a, b));
    }
}
