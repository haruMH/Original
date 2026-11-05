#include "main.h"
#include "world_look.h"
#include "renderer.h"
#include "render_constants.h"
#include <algorithm>
#include <cmath>

namespace {
    // 時間キー: 0=朝 / 1=昼 / 2=夕 / 3=夜
    struct Preset {
        XMFLOAT4 Direction;
        XMFLOAT4 Diffuse;
        XMFLOAT4 Ambient;
        XMFLOAT4 FogColor;
        float    FogStart;
        float    FogEnd;
        float    Saturation;
        float    Contrast;
        float    ShadowTint[3];
        float    HighlightTint[3];
        float    Zenith[3];
        float    Horizon[3];
        float    SunColor[3];
        float    StarAmount;
        float    CloudBright;
    };

    const int   kKeys = 4;
    // 1日の長さ(フレーム)。60fps で 5分
    const float kDayFrames = 18000.0f;
    // 空キー(昼/夕/夜)切替時の早送り速度（1フレームで進める時間の量）
    const float kFastForwardStep = 0.006f;

    Preset g_Presets[kKeys];
    Preset g_Current;
    float  g_Time = 0.25f;            // 現在の時間(0〜1)。最初は昼
    float  g_ForwardTarget = -1.0f;   // 早送り先(負なら通常進行)
    float  g_Elapsed = 0.0f;
    bool   g_Initialized = false;

    void BuildPresets()
    {
        // 朝焼け: 低い位置の暖色の光、淡い空
        Preset& dawn = g_Presets[0];
        dawn.Direction  = XMFLOAT4(-0.75f, -0.45f, 0.35f, 0.0f);
        dawn.Diffuse    = XMFLOAT4(1.00f, 0.72f, 0.50f, 1.0f);
        dawn.Ambient    = XMFLOAT4(0.50f, 0.48f, 0.62f, 1.0f);
        dawn.FogColor   = XMFLOAT4(0.95f, 0.70f, 0.62f, 1.0f);
        dawn.FogStart   = 25.0f;  dawn.FogEnd = 65.0f;
        dawn.Saturation = 1.10f;  dawn.Contrast = 1.02f;
        dawn.ShadowTint[0] = 0.94f; dawn.ShadowTint[1] = 0.94f; dawn.ShadowTint[2] = 1.08f;
        dawn.HighlightTint[0] = 1.08f; dawn.HighlightTint[1] = 1.02f; dawn.HighlightTint[2] = 0.94f;
        dawn.Zenith[0] = 0.30f; dawn.Zenith[1] = 0.42f; dawn.Zenith[2] = 0.78f;
        dawn.Horizon[0] = 1.00f; dawn.Horizon[1] = 0.68f; dawn.Horizon[2] = 0.52f;
        dawn.SunColor[0] = 1.00f; dawn.SunColor[1] = 0.62f; dawn.SunColor[2] = 0.36f;
        dawn.StarAmount = 0.0f; dawn.CloudBright = 1.0f;

        // 昼: 既存のデフォルト値をそのまま使う
        Preset& day = g_Presets[1];
        day.Direction  = RenderConstants::DefaultLight::LIGHT_DIRECTION;
        day.Diffuse    = RenderConstants::DefaultLight::LIGHT_DIFFUSE;
        day.Ambient    = RenderConstants::DefaultLight::LIGHT_AMBIENT;
        day.FogColor   = RenderConstants::DefaultLight::LIGHT_FOG_COLOR;
        day.FogStart   = RenderConstants::DefaultLight::FOG_START;
        day.FogEnd     = RenderConstants::DefaultLight::FOG_END;
        day.Saturation = 1.05f;  day.Contrast = 1.0f;
        day.ShadowTint[0] = 0.96f; day.ShadowTint[1] = 0.98f; day.ShadowTint[2] = 1.06f;
        day.HighlightTint[0] = 1.04f; day.HighlightTint[1] = 1.02f; day.HighlightTint[2] = 0.98f;
        day.Zenith[0] = 0.16f; day.Zenith[1] = 0.40f; day.Zenith[2] = 0.85f;
        day.Horizon[0] = 0.66f; day.Horizon[1] = 0.82f; day.Horizon[2] = 0.96f;
        day.SunColor[0] = 1.00f; day.SunColor[1] = 0.94f; day.SunColor[2] = 0.80f;
        day.StarAmount = 0.0f; day.CloudBright = 1.15f;

        // 夕焼け: 低い橙の太陽、温かい環境光
        Preset& sun = g_Presets[2];
        sun.Direction  = XMFLOAT4(0.75f, -0.45f, 0.35f, 0.0f);
        sun.Diffuse    = XMFLOAT4(1.00f, 0.66f, 0.38f, 1.0f);
        sun.Ambient    = XMFLOAT4(0.55f, 0.42f, 0.52f, 1.0f);
        sun.FogColor   = XMFLOAT4(0.95f, 0.62f, 0.45f, 1.0f);
        sun.FogStart   = 25.0f;  sun.FogEnd = 70.0f;
        sun.Saturation = 1.15f;  sun.Contrast = 1.05f;
        sun.ShadowTint[0] = 0.92f; sun.ShadowTint[1] = 0.90f; sun.ShadowTint[2] = 1.10f;
        sun.HighlightTint[0] = 1.10f; sun.HighlightTint[1] = 1.00f; sun.HighlightTint[2] = 0.88f;
        sun.Zenith[0] = 0.28f; sun.Zenith[1] = 0.24f; sun.Zenith[2] = 0.56f;
        sun.Horizon[0] = 1.00f; sun.Horizon[1] = 0.48f; sun.Horizon[2] = 0.28f;
        sun.SunColor[0] = 1.00f; sun.SunColor[1] = 0.50f; sun.SunColor[2] = 0.22f;
        sun.StarAmount = 0.0f; sun.CloudBright = 1.0f;

        // 夜: 青白い月光、暗い青の環境光
        Preset& night = g_Presets[3];
        night.Direction  = XMFLOAT4(-0.35f, -0.80f, -0.45f, 0.0f);
        night.Diffuse    = XMFLOAT4(0.45f, 0.55f, 0.85f, 1.0f);
        night.Ambient    = XMFLOAT4(0.18f, 0.22f, 0.40f, 1.0f);
        night.FogColor   = XMFLOAT4(0.06f, 0.09f, 0.20f, 1.0f);
        night.FogStart   = 18.0f;  night.FogEnd = 55.0f;
        night.Saturation = 0.95f;  night.Contrast = 1.10f;
        night.ShadowTint[0] = 0.85f; night.ShadowTint[1] = 0.92f; night.ShadowTint[2] = 1.18f;
        night.HighlightTint[0] = 0.95f; night.HighlightTint[1] = 1.00f; night.HighlightTint[2] = 1.08f;
        night.Zenith[0] = 0.010f; night.Zenith[1] = 0.020f; night.Zenith[2] = 0.075f;
        night.Horizon[0] = 0.050f; night.Horizon[1] = 0.085f; night.Horizon[2] = 0.200f;
        night.SunColor[0] = 0.55f; night.SunColor[1] = 0.65f; night.SunColor[2] = 1.00f;
        night.StarAmount = 1.0f; night.CloudBright = 0.35f;
    }

    float Lerp(float a, float b, float t) { return a + (b - a) * t; }

    XMFLOAT4 Lerp4(const XMFLOAT4& a, const XMFLOAT4& b, float t)
    {
        return XMFLOAT4(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t), Lerp(a.w, b.w, t));
    }

    void Lerp3(float* out, const float* a, const float* b, float t)
    {
        for (int i = 0; i < 3; i++) out[i] = Lerp(a[i], b[i], t);
    }

    // 2つのプリセットを t で混ぜる
    void Blend(Preset& o, const Preset& a, const Preset& b, float t)
    {
        o.Direction  = Lerp4(a.Direction, b.Direction, t);
        o.Diffuse    = Lerp4(a.Diffuse,   b.Diffuse,   t);
        o.Ambient    = Lerp4(a.Ambient,   b.Ambient,   t);
        o.FogColor   = Lerp4(a.FogColor,  b.FogColor,  t);
        o.FogStart   = Lerp(a.FogStart, b.FogStart, t);
        o.FogEnd     = Lerp(a.FogEnd,   b.FogEnd,   t);
        o.Saturation = Lerp(a.Saturation, b.Saturation, t);
        o.Contrast   = Lerp(a.Contrast,   b.Contrast,   t);
        Lerp3(o.ShadowTint,    a.ShadowTint,    b.ShadowTint,    t);
        Lerp3(o.HighlightTint, a.HighlightTint, b.HighlightTint, t);
        Lerp3(o.Zenith,   a.Zenith,   b.Zenith,   t);
        Lerp3(o.Horizon,  a.Horizon,  b.Horizon,  t);
        Lerp3(o.SunColor, a.SunColor, b.SunColor, t);
        o.StarAmount  = Lerp(a.StarAmount,  b.StarAmount,  t);
        o.CloudBright = Lerp(a.CloudBright, b.CloudBright, t);
    }

    // 現在の時間から g_Current を計算し、ライトへ反映する
    void Evaluate()
    {
        float x = g_Time * kKeys;
        int seg = (int)floorf(x) % kKeys;
        if (seg < 0) seg += kKeys;
        float f = x - floorf(x);
        // 各キーの付近では見た目を保ち、中間で切り替わるようにする
        float e = (std::min)((std::max)((f - 0.30f) / 0.40f, 0.0f), 1.0f);
        e = e * e * (3.0f - 2.0f * e);
        Blend(g_Current, g_Presets[seg], g_Presets[(seg + 1) % kKeys], e);

        LIGHT light = Renderer::GetLight();
        light.Direction = g_Current.Direction;
        light.Diffuse   = g_Current.Diffuse;
        light.Ambient   = g_Current.Ambient;
        light.FogColor  = g_Current.FogColor;
        light.FogStart  = g_Current.FogStart;
        light.FogEnd    = g_Current.FogEnd;
        Renderer::SetLight(light);
    }

    void EnsureInit()
    {
        if (g_Initialized) return;
        BuildPresets();
        g_Initialized = true;
        Evaluate();
    }

    float KeyTime(int skyType)
    {
        switch (skyType) {
        case 0:  return 0.25f;   // 昼
        case 1:  return 0.50f;   // 夕
        default: return 0.75f;   // 夜
        }
    }

    // 時間 a から b へ前向きに進めたときの距離(0〜1)
    float ForwardDistance(float a, float b)
    {
        float d = b - a;
        while (d < 0.0f) d += 1.0f;
        return d;
    }
}

void WorldLook::SetTarget(int skyType, bool instant)
{
    EnsureInit();
    float t = KeyTime(skyType);
    if (instant) {
        g_Time = t;
        g_ForwardTarget = -1.0f;
        Evaluate();
    } else {
        g_ForwardTarget = t;
    }
}

void WorldLook::GoToDawn()
{
    EnsureInit();
    g_ForwardTarget = 0.0f;
}

void WorldLook::Update()
{
    EnsureInit();
    g_Elapsed += 1.0f / 60.0f;

    if (g_ForwardTarget >= 0.0f) {
        // 早送り中: 目標に届いたら通常進行へ戻る
        float dist = ForwardDistance(g_Time, g_ForwardTarget);
        if (dist <= kFastForwardStep || dist > 0.999f) {
            g_Time = g_ForwardTarget;
            g_ForwardTarget = -1.0f;
        } else {
            g_Time += kFastForwardStep;
        }
    } else {
        g_Time += 1.0f / kDayFrames;
    }
    if (g_Time >= 1.0f) g_Time -= 1.0f;
    if (g_Time < 0.0f)  g_Time += 1.0f;

    Evaluate();
}

float WorldLook::GetTime() { EnsureInit(); return g_Time; }
float WorldLook::GetElapsedSeconds() { return g_Elapsed; }

void WorldLook::GetSunDirection(float out[3])
{
    EnsureInit();
    // 時間0で東の地平線、0.25で天頂、0.5で西の地平線、0.75で地面の下
    float a = g_Time * 6.2831853f;
    float x = cosf(a), y = sinf(a), z = -0.35f;
    float len = sqrtf(x * x + y * y + z * z);
    out[0] = x / len; out[1] = y / len; out[2] = z / len;
}

const float* WorldLook::GetZenithColor()  { EnsureInit(); return g_Current.Zenith; }
const float* WorldLook::GetHorizonColor() { EnsureInit(); return g_Current.Horizon; }
const float* WorldLook::GetSunColor()     { EnsureInit(); return g_Current.SunColor; }
float WorldLook::GetStarAmount()          { EnsureInit(); return g_Current.StarAmount; }
float WorldLook::GetCloudBrightness()     { EnsureInit(); return g_Current.CloudBright; }
float WorldLook::GetSaturation() { EnsureInit(); return g_Current.Saturation; }
float WorldLook::GetContrast()   { EnsureInit(); return g_Current.Contrast; }
const float* WorldLook::GetShadowTint()    { EnsureInit(); return g_Current.ShadowTint; }
const float* WorldLook::GetHighlightTint() { EnsureInit(); return g_Current.HighlightTint; }
