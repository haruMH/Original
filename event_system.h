#pragma once
#include <vector>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <memory>

// =================================================================
// イベント駆動システム (EventSystem)
// =================================================================
// ゲーム内の各イベント（被弾、撃破、シーン遷移等）を型安全に購読・発行します。
// システム間の密結合を防ぎ、依存関係を極限まで排除します。
class EventSystem
{
private:
    // 各イベント型ごとのリスナー（コールバック）を保持する抽象基底クラス
    class ListenerListBase {
    public:
        virtual ~ListenerListBase() = default;
    };

    // テンプレートによる型ごとのリスナーリスト実装
    template<typename EventT>
    class ListenerList : public ListenerListBase
    {
    public:
        std::vector<std::function<void(const EventT&)>> listeners;
    };

    // イベントの型識別子(type_index)をキーとしたリスナーリストのマップ
    static std::unordered_map<std::type_index, std::unique_ptr<ListenerListBase>> m_EventListeners;

public:
    // イベントの購読登録 (Subscribe)
    template<typename EventT>
    static void Subscribe(std::function<void(const EventT&)> callback)
    {
        auto typeIdx = std::type_index(typeid(EventT));
        if (m_EventListeners.find(typeIdx) == m_EventListeners.end()) {
            m_EventListeners[typeIdx] = std::make_unique<ListenerList<EventT>>();
        }
        auto* list = static_cast<ListenerList<EventT>*>(m_EventListeners[typeIdx].get());
        list->listeners.push_back(callback);
    }

    // イベントの発行 (Publish)
    template<typename EventT>
    static void Publish(const EventT& eventData)
    {
        auto typeIdx = std::type_index(typeid(EventT));
        auto it = m_EventListeners.find(typeIdx);
        if (it != m_EventListeners.end()) {
            auto* list = static_cast<ListenerList<EventT>*>(it->second.get());
            for (const auto& listener : list->listeners) {
                listener(eventData);
            }
        }
    }

    // 全てのイベントリスナーを解放 (シーン遷移時のクリーンアップ用)
    static void Clear()
    {
        m_EventListeners.clear();
    }
};
