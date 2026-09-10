#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <memory>
#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// サウンド識別用 ID（必要に応じて追加可能）
enum class SoundID
{
    // BGM
    BGM_NORMAL_STAGE, // 通常ステージBGM
    BGM_BOSS_STAGE,   // ボス戦BGM
    BGM_TITLE,        // タイトルBGM
    BGM_CLEAR,        // クリアBGM

    // SE (戦闘・アクション)
    SE_GRAB,          // 敵を掴む
    SE_THROW,         // 投擲
    SE_HIT_ENEMY,     // 敵同士の激突・打撃音
    SE_HIT_WALL,      // 壁への激突破砕音
    SE_PARRY,         // パリィ成功音
    SE_DODGE,         // ジャスト回避音
    SE_ITEM_PICKUP,   // アイテム取得音
    SE_LIGHTNING,     // 連鎖雷電音
    SE_EXPLOSION,     // 爆発音
    SE_PLAYER_HIT,    // プレイヤー被弾
    SE_BOSS_ROAR,     // ボス咆哮・登場
    SE_DECIDE,        // 決定ボタン音（Enter）

    COUNT
};

// メモリ上にキャッシュされる音声データ構造体
struct AudioData
{
    WAVEFORMATEX        format;       // 音声フォーマット情報
    std::vector<BYTE>   buffer;       // PCM 波形データ
};

// =================================================================
// サウンド管理システム (SoundManager)
// =================================================================
// XAudio2 と Media Foundation を用いて、WAV および MP3 形式のオーディオを
// メモリ上にデコード・キャッシュし、BGM および SE（多重再生対応）を管理します。
class SoundManager
{
private:
    static IXAudio2*               m_XAudio2;
    static IXAudio2MasteringVoice* m_MasteringVoice;

    // キャッシュされたオーディオデータ
    static std::unordered_map<SoundID, std::unique_ptr<AudioData>> m_AudioCache;

    // 現在再生中の BGM ソースボイス
    static IXAudio2SourceVoice* m_BgmVoice;
    static SoundID              m_CurrentBgmId;
    static float                m_BgmVolume;
    static float                m_SeVolume;

    // SE 多重再生用のボイスプール
    struct PooledVoice
    {
        IXAudio2SourceVoice* voice = nullptr;
        bool                 inUse = false;
    };
    static std::vector<PooledVoice> m_SeVoicePool;

    // ファイルパスから波形データをデコード・ロードする内部関数（WAV / MP3 自動判定）
    static std::unique_ptr<AudioData> LoadAudioFile(const std::string& filePath);

public:
    static bool Init();
    static void Uninit();
    static void Update(); // 不要になった SE ボイスの自動回収

    // オーディオファイルの事前登録
    static bool RegisterSound(SoundID id, const std::string& relativePath);

    // BGM 制御（volume < 0 の場合は現在の m_BgmVolume を維持）
    static void  PlayBGM(SoundID id, bool loop = true, float volume = -1.0f);
    static void  StopBGM();
    static void  SetBGMVolume(float volume);
    static float GetBGMVolume() { return m_BgmVolume; }

    // SE 制御 (ワンショット多重再生)
    static void  PlaySE(SoundID id, float volumeMultiplier = 1.0f);
    static void  SetSEVolume(float volume);
    static float GetSEVolume() { return m_SeVolume; }

    // EventSystem との自動連携イベント登録
    static void BindEvents();
};
