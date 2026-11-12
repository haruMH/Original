#pragma once

// =================================================================
// タイトルロゴの登場アニメーション（静的クラス）
// =================================================================
// 文字を距離場テクスチャにして、専用シェーダー(title_logo_ps)で
// 「輪郭が描かれる → 黒く塗られ白縁が付く → 下線が伸びる → 光が走る」を描く。
// 描画は Renderer::End のポスト合成の後（ACES などの影響を受けない）に行う。
class TitleLogo {
public:
    static void Start(const char* text);   // 距離場を作って演出を開始（タイトル画面の Init で呼ぶ）
    static void Stop();                    // 終了・リソース解放（タイトル画面の Uninit で呼ぶ）
    static void Update();                  // 毎フレーム
    static void Draw();                    // Renderer::End から呼ぶ。非稼働時は何もしない
    static void Skip();                    // 演出を飛ばして完成状態にする

    static bool  IsMenuReady();            // メニューを操作可能にしてよいか
    static float GetMenuAlpha();           // メニューのフェードイン度合い 0〜1
};
