#include "localization.h"
#include <windows.h>
#include <unordered_map>
#include <vector>

Language Loc::m_CurrentLanguage = Language::Japanese;

namespace {
    // UTF-8変換結果をキャッシュし、毎フレームの動的メモリ確保を排除
    std::unordered_map<std::string, std::string> s_Utf8Cache;
}

void Loc::Init()
{
    // 初期言語設定（デフォルトは日本語）
    m_CurrentLanguage = Language::Japanese;
}

void Loc::SetLanguage(Language lang)
{
    m_CurrentLanguage = lang;
}

const char* Loc::ToUTF8(const char* sjisText)
{
    if (!sjisText || !*sjisText) {
        return "";
    }

    auto it = s_Utf8Cache.find(sjisText);
    if (it != s_Utf8Cache.end()) {
        return it->second.c_str();
    }

    // CP932 (Shift-JIS) -> ワイド文字列 (UTF-16)
    int wlen = MultiByteToWideChar(CP_ACP, 0, sjisText, -1, nullptr, 0);
    if (wlen <= 0) {
        return sjisText;
    }

    std::vector<wchar_t> wbuf(wlen);
    MultiByteToWideChar(CP_ACP, 0, sjisText, -1, wbuf.data(), wlen);

    // ワイド文字列 (UTF-16) -> UTF-8
    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) {
        return sjisText;
    }

    std::vector<char> u8buf(u8len);
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, u8buf.data(), u8len, nullptr, nullptr);

    // キャッシュへ格納
    s_Utf8Cache[sjisText] = std::string(u8buf.data());
    return s_Utf8Cache[sjisText].c_str();
}

const char* Loc::Tr(const char* en, const char* ja)
{
    if (m_CurrentLanguage == Language::Japanese && ja && *ja) {
        return ToUTF8(ja);
    }
    return en ? en : "";
}
