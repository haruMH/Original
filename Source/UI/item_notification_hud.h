#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

// =================================================================
// アイテム・素材入手ログ通知HUD (ItemNotificationHUD)
// =================================================================
// 画面左端・高さ中央に入手したアイテム名と個数をスタック表示します。
// 違うアイテムを入手した場合は下に追加、同一アイテムは加算します。
// 各エントリは2秒後に少し上にスライドしながらフェードアウトして消滅します。
class ItemNotificationHUD
{
public:
    struct Entry
    {
        std::string         name;          // アイテム表示名（UTF-8）
        int                 count;         // 獲得個数
        float               timer;         // 経過時間（秒）
        float               maxTimer;      // 静止表示時間（デフォルト2.0秒）
        float               fadeDuration;  // フェード消滅時間（デフォルト0.4秒）
        DirectX::XMFLOAT4   color;         // アイテム種別アクセントカラー
        float               currentY;      // スムーズな上下スライド用Y座標
    };

    static void Init();
    static void Uninit();
    static void Clear();
    static void Update(float dt = 0.016f);
    static void Draw();

    // アイテム通知の追加（同種ならカウント加算＆タイマーリフレッシュ、別種なら下に追加）
    static void Add(const std::string& itemName, int count, const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

private:
    static std::vector<Entry> m_Entries;
};
