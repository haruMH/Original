#pragma once
#include <string>

// =================================================================
// 多言語対応・ローカライゼーションシステム (Localization)
// =================================================================
enum class Language {
    English = 0,
    Japanese = 1
};

class Loc
{
private:
    static Language m_CurrentLanguage;

public:
    static void Init();

    // 言語の取得と設定
    static Language GetLanguage() { return m_CurrentLanguage; }
    static void     SetLanguage(Language lang);

    // CP932 (Shift-JIS) から UTF-8 への文字列変換（ImGui描画用）
    static const char* ToUTF8(const char* sjisText);

    // 英語と日本語を受け取り、現在の言語に応じた文字列を返却
    // （日本語の場合は自動で UTF-8 に変換して返却し、文字化けを防止します）
    static const char* Tr(const char* en, const char* ja);
};
