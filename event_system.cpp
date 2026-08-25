#include "event_system.h"

// 静的メンバ変数の実体定義
std::unordered_map<std::type_index, std::unique_ptr<EventSystem::ListenerListBase>> EventSystem::m_EventListeners;
