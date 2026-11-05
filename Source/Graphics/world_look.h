#pragma once

// =================================================================
// ワールドルック管理クラス（静的クラス）
// =================================================================
// 1日の時間(0〜1)を持ち、時間とともに朝→昼→夕→夜と進める。
// 時間から「ライト・フォグ・カラーグレード・空の色・星の濃さ」を補間して決める。
//   時間: 0.00=朝焼け(日の出) / 0.25=昼 / 0.50=夕焼け / 0.75=夜 / 1.00=朝焼けへ戻る
class WorldLook {
public:
    // 毎フレーム呼ぶ。時間を進め、ライト/フォグを Renderer へ反映する
    static void Update();

    // 空キー(0=昼, 1=夕, 2=夜)に合わせて時間を移動する。
    // instant=true なら即座に、false なら早送りでその時間まで進める（以降は通常速度）
    static void SetTarget(int skyType, bool instant);
    // 朝焼けへ早送りする（キー用）
    static void GoToDawn();

    static float GetTime();            // 0〜1
    static float GetElapsedSeconds();  // 雲などのアニメーション用(連続)
    static void  GetSunDirection(float out[3]);  // 太陽の方向(正規化済み。y>0 が地平線より上)

    // 空の色
    static const float* GetZenithColor();    // 天頂 RGB
    static const float* GetHorizonColor();   // 地平線 RGB
    static const float* GetSunColor();       // 太陽/朝夕の光 RGB
    static float GetStarAmount();            // 星の濃さ 0〜1
    static float GetCloudBrightness();       // 雲の明るさ係数

    // カラーグレード(補間済み)
    static float GetSaturation();
    static float GetContrast();
    static const float* GetShadowTint();     // 暗部の色味 RGB
    static const float* GetHighlightTint();  // 明部の色味 RGB
};
