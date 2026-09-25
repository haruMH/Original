#include "item_notification_hud.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

std::vector<ItemNotificationHUD::Entry> ItemNotificationHUD::m_Entries;

void ItemNotificationHUD::Init()
{
    m_Entries.clear();
}

void ItemNotificationHUD::Uninit()
{
    m_Entries.clear();
}

void ItemNotificationHUD::Clear()
{
    m_Entries.clear();
}

void ItemNotificationHUD::Add(const std::string& itemName, int count, const DirectX::XMFLOAT4& color)
{
    // 同一アイテムが既に通知リストに存在する場合は、個数を合算してタイマーをリフレッシュ
    for (auto& entry : m_Entries) {
        if (entry.name == itemName) {
            entry.count += count;
            entry.timer = 0.0f; // 表示時間を再スタート
            entry.color = color;
            return;
        }
    }

    // 違うアイテムを入手した場合は、直前に入手したアイテムの下（リスト末尾）に追加
    Entry newEntry;
    newEntry.name         = itemName;
    newEntry.count        = count;
    newEntry.timer        = 0.0f;
    newEntry.maxTimer     = 2.0f; // 2秒間静止表示
    newEntry.fadeDuration = 0.4f; // 0.4秒かけてフェード上昇
    newEntry.color        = color;
    newEntry.currentY     = -1.0f; // 初回描画時に初期化

    m_Entries.push_back(newEntry);
}

void ItemNotificationHUD::Update(float dt)
{
    for (auto it = m_Entries.begin(); it != m_Entries.end(); ) {
        it->timer += dt;

        // 2秒 + フェード時間経過で完全に消滅
        if (it->timer >= it->maxTimer + it->fadeDuration) {
            it = m_Entries.erase(it);
        } else {
            ++it;
        }
    }
}

void ItemNotificationHUD::Draw()
{
    if (m_Entries.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) return;

    const float startX  = 28.0f;                           // 画面左端からのマージン
    const float baseY   = io.DisplaySize.y * 0.5f - 20.0f; // 画面高さ中央
    const float itemH   = 34.0f;                           // 1項目の高さ
    const float itemGap = 6.0f;                            // 項目間の隙間
    const float itemW   = 240.0f;                          // プレートの横幅

    for (size_t i = 0; i < m_Entries.size(); ++i) {
        auto& entry = m_Entries[i];

        // 理想的な配置Y座標（前のアイテムの下に並ぶ）
        float targetY = baseY + (float)i * (itemH + itemGap);

        // 初回初期化またはスムーズ追従
        if (entry.currentY < 0.0f) {
            entry.currentY = targetY;
        } else {
            entry.currentY += (targetY - entry.currentY) * 0.25f;
        }

        // 2秒後からのフェードアウト＆上昇アニメーション計算
        float alpha = 1.0f;
        float floatUpOffset = 0.0f;
        if (entry.timer > entry.maxTimer) {
            float fadeProgress = (entry.timer - entry.maxTimer) / entry.fadeDuration;
            fadeProgress = (std::min)((std::max)(fadeProgress, 0.0f), 1.0f);
            
            // 少し上にフワッと消える（イーズアウト上昇 -22px）
            floatUpOffset = -22.0f * (1.0f - (1.0f - fadeProgress) * (1.0f - fadeProgress));
            alpha = 1.0f - fadeProgress;
        }

        if (alpha <= 0.001f) continue;

        float posX = startX;
        float posY = entry.currentY + floatUpOffset;

        // プレート背景（ダーク半透明グラデーション風）
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.06f, 0.08f, 0.11f, 0.85f * alpha));
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(entry.color.x * 0.7f, entry.color.y * 0.7f, entry.color.z * 0.7f, 0.6f * alpha));
        drawList->AddRectFilled(ImVec2(posX, posY), ImVec2(posX + itemW, posY + itemH), bgColor, 4.0f);
        drawList->AddRect(ImVec2(posX, posY), ImVec2(posX + itemW, posY + itemH), borderColor, 4.0f, 0, 1.2f);

        // 左端のアクセントカラー縦バー
        ImU32 accentColor = ImGui::ColorConvertFloat4ToU32(ImVec4(entry.color.x, entry.color.y, entry.color.z, alpha));
        drawList->AddRectFilled(ImVec2(posX, posY), ImVec2(posX + 4.5f, posY + itemH), accentColor, 4.0f, ImDrawFlags_RoundCornersLeft);

        // テキスト表示: "アイテム名"
        ImVec2 textPos(posX + 14.0f, posY + 8.0f);
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.96f, 0.98f, alpha));
        drawList->AddText(textPos, textColor, entry.name.c_str());

        // 個数表記: "× 個数"（右寄せ）
        char countBuf[32];
        snprintf(countBuf, sizeof(countBuf), "x %d", entry.count);
        ImVec2 countSize = ImGui::CalcTextSize(countBuf);
        ImVec2 countPos(posX + itemW - countSize.x - 14.0f, posY + 8.0f);
        ImU32 countColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.88f, 0.35f, alpha));
        drawList->AddText(countPos, countColor, countBuf);
    }
}
