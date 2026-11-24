#pragma once

// =================================================================
// リザルト見出しの演出（静的クラス）
// =================================================================
// 「GAME OVER」「CLEAR」の見出しを、1文字ずつ動かして ImGui の前景に描く。
//  - 達成(triumphant=true) : 金色の文字が1つずつ上から弾んで登場 → 最後に光が走る
//  - 失敗(triumphant=false): 赤い文字がゆっくり浮かび上がり、少し沈みながら暗くなる
// 文字は GDI で太い書体のマスク画像にして貼る（拡大してもぼやけない）。
class ResultBanner {
public:
    static void Start(const char* text, bool triumphant);   // シーンの Init で呼ぶ
    static void Stop();                                      // シーンの Uninit で呼ぶ
    static void Update();                                    // 毎フレーム(60Hz固定ステップ)
    static void Draw();                                      // シーンの Draw(ImGuiフレーム内)で呼ぶ
};
