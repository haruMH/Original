#include "sound_manager.h"
#include "main.h"
#include "event_system.h"
#include "event_types.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <fstream>
#include <algorithm>

// 静的メンバ変数の実体
IXAudio2*               SoundManager::m_XAudio2 = nullptr;
IXAudio2MasteringVoice* SoundManager::m_MasteringVoice = nullptr;
std::unordered_map<SoundID, std::unique_ptr<AudioData>> SoundManager::m_AudioCache;
IXAudio2SourceVoice*    SoundManager::m_BgmVoice = nullptr;
SoundID                 SoundManager::m_CurrentBgmId = static_cast<SoundID>(-1);
float                   SoundManager::m_BgmVolume = 0.8f;
float                   SoundManager::m_SeVolume = 1.0f;
std::vector<SoundManager::PooledVoice> SoundManager::m_SeVoicePool;

// ─────────────────────────────────────────────
// 初期化
// ─────────────────────────────────────────────
bool SoundManager::Init()
{
    // 1. Media Foundation の初期化（MP3 などのデコード用）
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        LOG_INFO("[SoundManager] MFStartup 失敗: hr=0x%08X\n", hr);
        return false;
    }

    // 2. XAudio2 本体の作成
    hr = XAudio2Create(&m_XAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        LOG_INFO("[SoundManager] XAudio2Create 失敗: hr=0x%08X\n", hr);
        MFShutdown();
        return false;
    }

    // 3. マスターボイス（出力先）の作成
    hr = m_XAudio2->CreateMasteringVoice(&m_MasteringVoice);
    if (FAILED(hr)) {
        LOG_INFO("[SoundManager] CreateMasteringVoice 失敗: hr=0x%08X\n", hr);
        m_XAudio2->Release();
        m_XAudio2 = nullptr;
        MFShutdown();
        return false;
    }

    LOG_INFO("[SoundManager] 初期化成功 (XAudio2 & Media Foundation)\n");

    // イベント購読の紐付け
    BindEvents();

    // ─────────────────────────────────────────────
    // デフォルトサウンドの事前登録
    // ─────────────────────────────────────────────
    RegisterSound(SoundID::BGM_TITLE, "Blackout_Divinity.mp3");
    RegisterSound(SoundID::SE_DECIDE, "決定ボタンを押す44.wav");

    return true;
}

// ─────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────
void SoundManager::Uninit()
{
    StopBGM();

    // SE ボイスプールの破棄
    for (auto& pv : m_SeVoicePool) {
        if (pv.voice) {
            pv.voice->Stop();
            pv.voice->DestroyVoice();
            pv.voice = nullptr;
        }
    }
    m_SeVoicePool.clear();

    // キャッシュデータのクリア
    m_AudioCache.clear();

    // マスターボイスの破棄
    if (m_MasteringVoice) {
        m_MasteringVoice->DestroyVoice();
        m_MasteringVoice = nullptr;
    }

    // XAudio2 本体の解放
    if (m_XAudio2) {
        m_XAudio2->Release();
        m_XAudio2 = nullptr;
    }

    // Media Foundation のシャットダウン
    MFShutdown();
    LOG_INFO("[SoundManager] 終了処理完了\n");
}

// ─────────────────────────────────────────────
// 毎フレーム更新（再生終了した SE ボイスの回収）
// ─────────────────────────────────────────────
void SoundManager::Update()
{
    for (auto& pv : m_SeVoicePool) {
        if (pv.inUse && pv.voice) {
            XAUDIO2_VOICE_STATE state;
            pv.voice->GetState(&state);
            if (state.BuffersQueued == 0) {
                // 再生完了したためプールへ返却
                pv.voice->Stop();
                pv.inUse = false;
            }
        }
    }
}

// ─────────────────────────────────────────────
// Media Foundation を使用したオーディオファイルの PCM デコード読み込み
// （WAV / MP3 両対応）
// ─────────────────────────────────────────────
std::unique_ptr<AudioData> SoundManager::LoadAudioFile(const std::string& filePath)
{
    // ワイド文字列への変換
    int wlen = MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return nullptr;
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, filePath.c_str(), -1, &wpath[0], wlen);

    IMFSourceReader* pReader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &pReader);
    if (FAILED(hr)) {
        LOG_INFO("[SoundManager] ファイルオープン失敗: %s (hr=0x%08X)\n", filePath.c_str(), hr);
        return nullptr;
    }

    // PCM 形式を指定してストリーム出力をリクエスト
    IMFMediaType* pPartialType = nullptr;
    MFCreateMediaType(&pPartialType);
    pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pPartialType);
    pPartialType->Release();

    if (FAILED(hr)) {
        LOG_INFO("[SoundManager] PCM 形式の設定失敗: %s\n", filePath.c_str());
        pReader->Release();
        return nullptr;
    }

    // デコードされたオーディオフォーマット情報の取得
    IMFMediaType* pUncompressedType = nullptr;
    pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pUncompressedType);

    WAVEFORMATEX* pWaveFormat = nullptr;
    UINT32 formatSize = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(pUncompressedType, &pWaveFormat, &formatSize);
    pUncompressedType->Release();

    if (FAILED(hr) || !pWaveFormat) {
        LOG_INFO("[SoundManager] WAVEFORMATEX の取得失敗: %s\n", filePath.c_str());
        pReader->Release();
        return nullptr;
    }

    auto audio = std::make_unique<AudioData>();
    memcpy(&audio->format, pWaveFormat, sizeof(WAVEFORMATEX));
    CoTaskMemFree(pWaveFormat);

    // サンプルデータの連続読み出し
    while (true) {
        IMFSample* pSample = nullptr;
        DWORD flags = 0;
        hr = pReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr, &pSample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (pSample) pSample->Release();
            break;
        }

        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            if (SUCCEEDED(hr)) {
                BYTE* pAudioBytes = nullptr;
                DWORD currentLength = 0;
                hr = pBuffer->Lock(&pAudioBytes, nullptr, &currentLength);
                if (SUCCEEDED(hr)) {
                    size_t curSize = audio->buffer.size();
                    audio->buffer.resize(curSize + currentLength);
                    memcpy(audio->buffer.data() + curSize, pAudioBytes, currentLength);
                    pBuffer->Unlock();
                }
                pBuffer->Release();
            }
            pSample->Release();
        }
    }

    pReader->Release();
    LOG_INFO("[SoundManager] ロード成功: %s (%zu bytes, %u Hz, %u ch)\n",
        filePath.c_str(), audio->buffer.size(), audio->format.nSamplesPerSec, audio->format.nChannels);

    return audio;
}

// ─────────────────────────────────────────────
// サウンドの事前登録
// ─────────────────────────────────────────────
bool SoundManager::RegisterSound(SoundID id, const std::string& relativePath)
{
    // 複数の候補パスを順に探索する（ソース直下、Assets/sound/、Assets/）
    // これにより開発中（ソース直下配置）でもリリース後でも確実に読み込めます
    std::vector<std::string> searchPaths;
    searchPaths.push_back(relativePath); // 指定されたパスそのまま（またはソース直下）
    
    std::string baseName = relativePath;
    size_t lastSlash = baseName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseName = baseName.substr(lastSlash + 1);
    }
    searchPaths.push_back(baseName); // フォルダを除いたファイル名単体（ソース直下）
    searchPaths.push_back("Assets/sound/" + baseName);
    searchPaths.push_back("Assets/" + baseName);

    std::unique_ptr<AudioData> audio = nullptr;
    for (const auto& path : searchPaths) {
        audio = LoadAudioFile(path);
        if (audio) {
            break;
        }
    }

    if (!audio) {
        LOG_INFO("[SoundManager] サウンドファイルの読み込みに失敗しました: %s\n", relativePath.c_str());
        return false;
    }

    m_AudioCache[id] = std::move(audio);
    return true;
}

// ─────────────────────────────────────────────
// BGM 再生
// ─────────────────────────────────────────────
void SoundManager::PlayBGM(SoundID id, bool loop, float volume)
{
    if (!m_XAudio2) return;
    if (m_CurrentBgmId == id && m_BgmVoice) return; // 既に同じ曲を再生中

    StopBGM();

    auto it = m_AudioCache.find(id);
    if (it == m_AudioCache.end() || !it->second) {
        LOG_INFO("[SoundManager] 指定された BGM は未登録です: %d\n", static_cast<int>(id));
        return;
    }

    AudioData* audio = it->second.get();
    HRESULT hr = m_XAudio2->CreateSourceVoice(&m_BgmVoice, &audio->format);
    if (FAILED(hr)) {
        LOG_INFO("[SoundManager] BGM ソースボイスの作成失敗: hr=0x%08X\n", hr);
        return;
    }

    m_CurrentBgmId = id;
    m_BgmVolume = volume;
    m_BgmVoice->SetVolume(m_BgmVolume);

    XAUDIO2_BUFFER xBuffer = { 0 };
    xBuffer.AudioBytes = static_cast<UINT32>(audio->buffer.size());
    xBuffer.pAudioData = audio->buffer.data();
    xBuffer.Flags = XAUDIO2_END_OF_STREAM;
    if (loop) {
        xBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    hr = m_BgmVoice->SubmitSourceBuffer(&xBuffer);
    if (SUCCEEDED(hr)) {
        m_BgmVoice->Start(0);
        LOG_INFO("[SoundManager] BGM 再生開始: %d\n", static_cast<int>(id));
    }
}

// ─────────────────────────────────────────────
// BGM 停止
// ─────────────────────────────────────────────
void SoundManager::StopBGM()
{
    if (m_BgmVoice) {
        m_BgmVoice->Stop();
        m_BgmVoice->DestroyVoice();
        m_BgmVoice = nullptr;
        m_CurrentBgmId = static_cast<SoundID>(-1);
        LOG_INFO("[SoundManager] BGM 停止\n");
    }
}

// ─────────────────────────────────────────────
// BGM 音量設定
// ─────────────────────────────────────────────
void SoundManager::SetBGMVolume(float volume)
{
    m_BgmVolume = (std::max)(0.0f, (std::min)(1.0f, volume));
    if (m_BgmVoice) {
        m_BgmVoice->SetVolume(m_BgmVolume);
    }
}

// ─────────────────────────────────────────────
// SE 再生（ボイスプールによるワンショット多重再生）
// ─────────────────────────────────────────────
void SoundManager::PlaySE(SoundID id, float volumeMultiplier)
{
    if (!m_XAudio2) return;

    auto it = m_AudioCache.find(id);
    if (it == m_AudioCache.end() || !it->second) {
        return; // 未登録ならスキップ
    }

    AudioData* audio = it->second.get();

    // 1. ボイスプールから再利用可能な空きボイスを探す
    IXAudio2SourceVoice* targetVoice = nullptr;
    for (auto& pv : m_SeVoicePool) {
        if (!pv.inUse && pv.voice) {
            // フォーマットが一致しているか確認
            XAUDIO2_VOICE_DETAILS details;
            pv.voice->GetVoiceDetails(&details);
            if (details.InputSampleRate == audio->format.nSamplesPerSec &&
                details.InputChannels == audio->format.nChannels) {
                targetVoice = pv.voice;
                pv.inUse = true;
                break;
            }
        }
    }

    // 2. 空きボイスがなければ新しく生成してプールに追加
    if (!targetVoice) {
        HRESULT hr = m_XAudio2->CreateSourceVoice(&targetVoice, &audio->format);
        if (FAILED(hr)) {
            return;
        }
        PooledVoice newPv;
        newPv.voice = targetVoice;
        newPv.inUse = true;
        m_SeVoicePool.push_back(newPv);
    }

    // 音量設定
    float vol = (std::max)(0.0f, (std::min)(1.0f, m_SeVolume * volumeMultiplier));
    targetVoice->SetVolume(vol);

    // バッファを流し込んで再生
    XAUDIO2_BUFFER xBuffer = { 0 };
    xBuffer.AudioBytes = static_cast<UINT32>(audio->buffer.size());
    xBuffer.pAudioData = audio->buffer.data();
    xBuffer.Flags = XAUDIO2_END_OF_STREAM;

    targetVoice->FlushSourceBuffers();
    targetVoice->SubmitSourceBuffer(&xBuffer);
    targetVoice->Start(0);
}

// ─────────────────────────────────────────────
// SE 音量設定
// ─────────────────────────────────────────────
void SoundManager::SetSEVolume(float volume)
{
    m_SeVolume = (std::max)(0.0f, (std::min)(1.0f, volume));
}

// ─────────────────────────────────────────────
// EventSystem との自動連携イベント登録
// ─────────────────────────────────────────────
void SoundManager::BindEvents()
{
    // プレイヤー被弾
    EventSystem::Subscribe<PlayerHitEvent>([](const PlayerHitEvent&) {
        PlaySE(SoundID::SE_PLAYER_HIT);
    });

    // パリィ成功
    EventSystem::Subscribe<PlayerParriedEvent>([](const PlayerParriedEvent&) {
        PlaySE(SoundID::SE_PARRY, 1.2f);
    });

    // ボス登場咆哮
    EventSystem::Subscribe<BossSpawnEvent>([](const BossSpawnEvent&) {
        PlaySE(SoundID::SE_BOSS_ROAR, 1.5f);
        PlayBGM(SoundID::BGM_BOSS_STAGE, true);
    });
}
