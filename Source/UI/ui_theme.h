#pragma once

// =================================================================
// UI テーマ（ImGui 全体のスタイル）
// =================================================================
// ゲームの絵柄に合わせた配色・丸み・余白を ImGui へ一括で設定する。
//   基調: 夜の深い青 / 強調: 焚き火の橙 / 補助: 発光の水色
// ImGui::CreateContext() の後に Apply() を1回呼ぶ。
namespace UITheme {
    void Apply();
    void Update();   // 毎フレーム: 空の時間に合わせてパレットを切り替える

    // ボタンの種類分け（PushXxx の後に必ず PopButton() を呼ぶ）
    void PushPrimaryButton();   // 主要操作: 橙（セーブ・出撃・解放など）
    void PushDangerButton();    // 危険操作: 赤（ゲーム終了など）
    void PopButton();
}
