#pragma once
#include <vector>
#include <type_traits>
#include <memory>
#include <unordered_map>
#include <functional>
#include "render_system.h"
#include "gameobject.h"
#include "scene_interface.h"
#include "fade_system.h"

// 前方宣言
class Player;
class Enemy;
class Wall;
class Item;
class EnemyBullet;
class TitleScene;
class TutorialScene;
class GameplayScene;
class ClearScene;
class GameOverScene;
class BossEnemy;

// シーン状態を定義する列挙型（既存のコードとの互換性のために残します）
enum class Scene {
    TITLE,
    TUTORIAL,
    HUB,
    GAMEPLAY,
    CLEAR,
    GAMEOVER
};

// =================================================================
// ゲーム統括管理クラス (Manager)
// =================================================================
// ゲームのメインループ（Update, Draw）と、ポリモーフィズムを用いた
// シーンオブジェクト（IScene）のライフサイクル管理を担当します。
class Manager
{
private:
    static std::vector<std::unique_ptr<GameObject>> m_ManagedObjects; // ライフサイクル管理用オブジェクトリスト
    static std::vector<GameObject*> m_GameObjects;     // 全オブジェクトリスト（描画・一括走査用）
    static std::vector<GameObject*> m_UpdateObjects;   // 更新対象（動的）オブジェクトのリスト（更新処理高速化用）
    static Player*                m_CachedPlayer;     // キャッシュされたプレイヤーへのポインタ (O(1)アクセス用)
    static BossEnemy*             m_CachedBoss;       // キャッシュされたボスへのポインタ (O(1)アクセス用)
    static RenderSystem           m_RenderSystem;     // インスタンスバッチ描画システム
    static int                    m_HitStopFrames;    // ヒットストップの残りフレーム数
    static int                    m_SlowMotionTimer;  // スローモーションの残りフレーム数
    static int                    m_SlowMotionDuration;// スローモーションの総開始フレーム数
    static bool                   m_IsBossStage;      // ボスステージ中フラグ
    static bool                   m_StartFromBoss;    // ボス直行フラグ（クエスト選択用）
    static Scene                  m_CurrentScene;     // 現在のシーン（既存コード互換用）
    static Scene                  m_NextScene;        // 遷移予定 of the next scene
    static bool                   m_SceneTransitionRequested; // シーン遷移リクエストフラグ
    static bool                   m_PendingFadeIn;            // 裏でのシーン構築後、次フレームでフェードインを開始する待機フラグ
    static float                  m_FadeInDuration;           // 遷移後フェードイン時間
    static XMFLOAT4               m_FadeColor;                // フェード色

    static IScene*                m_ActiveScene;      // 現在アクティブなシーンオブジェクト

    // カテゴリ別オブジェクトキャッシュマップ
    static std::unordered_map<ObjectType, std::vector<GameObject*>> m_CategoryMap;

    static void ExecuteChangeScene(Scene nextScene); // 実際のシーン遷移実行

    // 画面フラッシュ/警告パルス制御用
    static DirectX::XMFLOAT4 m_FlashColor;
    static float             m_FlashFadeSpeed;
    static bool              m_IsLowHPWarning;
    static float             m_LowHPPulseTime;
    static float             m_LowHPAlpha;

    // ラジアルブラー/色収差制御用
    static float             m_RadialBlurIntensity;
    static float             m_ChromaticAberration;
    static DirectX::XMFLOAT2 m_RadialBlurCenter;

    // ボス登場カットシーン制御用
    static bool m_IsCutsceneActive;
    static int  m_CutsceneTimer;

    // フレームレート（FPS）制御用
    static int   m_TargetFPS;          // 目標FPS (30, 60, 120, 180, 240, 0=無制限)
    static float m_CurrentFPS;         // 実測FPS
    static int   m_FpsFrameCount;      // FPS計測用フレームカウンタ
    static float m_FpsTimeAccumulator; // FPS計測用時間累積

    // シャドウマップ／アウトラインパス共通のオブジェクト描画ヘルパー
    static void DrawShadowAndOutlineObjects();

public:
    // 初期化と終了処理
    static void Init();
    static void Uninit();
    
    // 更新と描画（アクティブなシーンオブジェクトに処理を委譲します）
    static void Update();
    static void Draw(float alpha = 1.0f);
    static void DestroyObjectsIf(); // オブジェクト破棄処理の一元化（IsDestroy == true）

    // 条件指定によるオブジェクト破棄の一元化（ボスステージ遷移等で使用）
    static void DestroyObjectsIf(std::function<bool(GameObject*)> pred);

    // テンプレート関数によるオブジェクトの生成・追加.
    template<typename ObjT>
    static ObjT* AddGameObject()
    {
        ObjT* gameObject = new ObjT();
        gameObject->Init();
        m_ManagedObjects.push_back(std::unique_ptr<GameObject>(gameObject));
        m_GameObjects.push_back(gameObject);
        // 静的オブジェクト（Wall, Field）は更新不要なため、更新リストから除外してパフォーマンスを向上
        ::ObjectType t = gameObject->GetObjectType();
        if (t != ::ObjectType::Wall && t != ::ObjectType::Field) {
            m_UpdateObjects.push_back(gameObject);
        }

        // プレイヤーが生成された場合はキャッシュポインタに保存
        if (std::is_same<ObjT, Player>::value) {
            m_CachedPlayer = reinterpret_cast<Player*>(gameObject);
        }

        // ボスエネミーが生成された場合はキャッシュポインタに保存
        if (std::is_same<ObjT, BossEnemy>::value) {
            m_CachedBoss = reinterpret_cast<BossEnemy*>(gameObject);
        }

        // カテゴリ別リストへのキャッシュ登録
        RegisterCategory(gameObject);

        return gameObject;
    }

    // テンプレート関数による特定の型のオブジェクトの取得.
    template<typename TargetType>
    static TargetType* GetGameObject()
    {
        // Playerの場合は全探索せずキャッシュを即時返却して O(1) に最適化
        if (std::is_same<TargetType, Player>::value) {
            return reinterpret_cast<TargetType*>(m_CachedPlayer);
        }

        // BossEnemyの場合は全探索せずキャッシュを即時返却して O(1) に最適化
        if (std::is_same<TargetType, BossEnemy>::value) {
            return reinterpret_cast<TargetType*>(m_CachedBoss);
        }

        ObjectType targetType = TargetType::GetStaticType();
        const std::vector<GameObject*>& categoryList = GetCategoryList(targetType);
        for (GameObject* gameObject : categoryList)
        {
            if (gameObject && !gameObject->IsDestroy())
            {
                if (TargetType* casted = dynamic_cast<TargetType*>(gameObject))
                {
                    return casted;
                }
            }
        }
        return nullptr;
    }

    // テンプレート関数による特定の型の全オブジェクトの一括取得.
    template<typename TargetType>
    static std::vector<TargetType*> GetGameObjects()
    {
        std::vector<TargetType*> list;
        ObjectType targetType = TargetType::GetStaticType();
        const std::vector<GameObject*>& categoryList = GetCategoryList(targetType);
        list.reserve(categoryList.size());
        for (GameObject* gameObject : categoryList)
        {
            if (gameObject && !gameObject->IsDestroy())
            {
                if (TargetType* casted = dynamic_cast<TargetType*>(gameObject))
                {
                    list.push_back(casted);
                }
            }
        }
        return list;
    }

    // オブジェクトリストの取得.
    static const std::vector<GameObject*>& GetGameObjectList() { return m_GameObjects; }
    static const std::vector<GameObject*>& GetUpdateObjectList() { return m_UpdateObjects; }

    // カテゴリ別リストへの高速アクセス
    static const std::vector<GameObject*>& GetCategoryList(ObjectType type)
    {
        static const std::vector<GameObject*> emptyList;
        auto it = m_CategoryMap.find(type);
        if (it != m_CategoryMap.end()) {
            return it->second;
        }
        return emptyList;
    }

    // カテゴリ別リストへの登録・解除用ヘルパー
    static void RegisterCategory(GameObject* obj);
    static void UnregisterCategory(GameObject* obj);
    static void ClearCategoryLists();

    // ヒットストップのフレーム設定.
    static void AddHitStop(int frames) { m_HitStopFrames = frames; }
    static int  GetHitStopFrames() { return m_HitStopFrames; }
    static void SetHitStopFrames(int frames) { m_HitStopFrames = frames; }

    // 現在ヒットストップ中かどうか.
    static bool IsHitStopping() { return m_HitStopFrames > 0; }

    // スローモーション（ウィッチタイム）制御用
    static void  StartSlowMotion(int duration) { m_SlowMotionTimer = duration; m_SlowMotionDuration = duration; }
    static int   GetSlowMotionTimer() { return m_SlowMotionTimer; }
    static void  SetSlowMotionTimer(int timer) { m_SlowMotionTimer = timer; }
    static int   GetSlowMotionDuration() { return m_SlowMotionDuration; }
    static void  SetSlowMotionDuration(int duration) { m_SlowMotionDuration = duration; }
    static float GetSlowMotionIntensity();
    static bool  IsSlowMotionActive() { return m_SlowMotionTimer > 0; }
    static bool  IsBossStage() { return m_IsBossStage; }
    static void  SetIsBossStage(bool enable) { m_IsBossStage = enable; }
    static bool  GetStartFromBoss() { return m_StartFromBoss; }
    static void  SetStartFromBoss(bool enable) { m_StartFromBoss = enable; }
    static void  TransitionToBossStage();
    static void  ChangeScene(Scene nextScene, float fadeOutDuration = 0.5f, float fadeInDuration = 0.5f, XMFLOAT4 color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
    static Scene GetCurrentScene() { return m_CurrentScene; }

    // ボス登場カットシーン制御
    static bool IsCutsceneActive() { return m_IsCutsceneActive; }
    static int  GetCutsceneTimer() { return m_CutsceneTimer; }
    static void TriggerBossSpawnCutscene();

    // 画面フラッシュ / 瀕死赤パルス制御
    static void TriggerFlash(DirectX::XMFLOAT4 color, float fadeSpeed = 0.05f);
    static void SetLowHPWarning(bool active) {
        m_IsLowHPWarning = active;
        if (!active) {
            m_LowHPPulseTime = 0.0f;
            m_LowHPAlpha = 0.0f;
        }
    }
    static DirectX::XMFLOAT4 GetFlashColor() {
        if (m_LowHPAlpha <= 0.001f) {
            return m_FlashColor;
        }
        if (m_FlashColor.w <= 0.001f) {
            return DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, m_LowHPAlpha);
        }
        float totalAlpha = (std::min)(1.0f, m_FlashColor.w + m_LowHPAlpha);
        float r = (m_FlashColor.x * m_FlashColor.w + 1.0f * m_LowHPAlpha) / totalAlpha;
        float g = (m_FlashColor.y * m_FlashColor.w) / totalAlpha;
        float b = (m_FlashColor.z * m_FlashColor.w) / totalAlpha;
        return DirectX::XMFLOAT4(r, g, b, totalAlpha);
    }

    // ラジアルブラー / 色収差演出制御
    static void TriggerRadialBlur(float intensity, DirectX::XMFLOAT2 center = DirectX::XMFLOAT2(0.5f, 0.5f)) {
        if (intensity > m_RadialBlurIntensity) {
            m_RadialBlurIntensity = intensity;
            m_RadialBlurCenter = center;
        }
    }
    static void TriggerChromaticAberration(float intensity) {
        if (intensity > m_ChromaticAberration) {
            m_ChromaticAberration = intensity;
        }
    }
    static float GetRadialBlurIntensity() { return m_RadialBlurIntensity; }
    static float GetChromaticAberration() { return m_ChromaticAberration; }
    static DirectX::XMFLOAT2 GetRadialBlurCenter() { return m_RadialBlurCenter; }

    // フレームレート（FPS）制御
    static int   GetTargetFPS() { return m_TargetFPS; }
    static void  SetTargetFPS(int fps) { m_TargetFPS = fps; }
    static float GetCurrentFPS() { return m_CurrentFPS; }
    static void  UpdateFPSCounter(float deltaDrawTime);
};
