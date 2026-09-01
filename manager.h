#pragma once
#include <vector>
#include <type_traits>
#include <memory>
#include <unordered_map>
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
class GameplayScene;
class ClearScene;
class GameOverScene;
class BossEnemy;

// シーン状態を定義する列挙型（既存のコードとの互換性のために残します）
enum class Scene {
    TITLE,
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
    static Scene                  m_CurrentScene;     // 現在のシーン（既存コード互換用）
    static Scene                  m_NextScene;        // 遷移予定 of the next scene
    static bool                   m_SceneTransitionRequested; // シーン遷移リクエストフラグ
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

    // ボス登場カットシーン制御用
    static bool m_IsCutsceneActive;
    static int  m_CutsceneTimer;

public:
    // 初期化と終了処理
    static void Init();
    static void Uninit();
    
    // 更新と描画（アクティブなシーンオブジェクトに処理を委譲します）
    static void Update();
    static void Draw();
    static void DestroyObjectsIf(); // オブジェクト破棄処理の一元化

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
        for (GameObject* gameObject : m_GameObjects)
        {
            if (gameObject && gameObject->GetObjectType() == targetType)
                return static_cast<TargetType*>(gameObject);
        }
        return nullptr;
    }

    // テンプレート関数による特定の型の全オブジェクトの一括取得.
    template<typename TargetType>
    static std::vector<TargetType*> GetGameObjects()
    {
        std::vector<TargetType*> list;
        ObjectType targetType = TargetType::GetStaticType();
        for (GameObject* gameObject : m_GameObjects)
        {
            if (gameObject && gameObject->GetObjectType() == targetType)
            {
                list.push_back(static_cast<TargetType*>(gameObject));
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
    static void  TransitionToBossStage();
    static void  ChangeScene(Scene nextScene, float fadeOutDuration = 0.5f, float fadeInDuration = 0.5f, XMFLOAT4 color = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f));
    static Scene GetCurrentScene() { return m_CurrentScene; }

    // ボス登場カットシーン制御
    static bool IsCutsceneActive() { return m_IsCutsceneActive; }
    static int  GetCutsceneTimer() { return m_CutsceneTimer; }
    static void TriggerBossSpawnCutscene();

    // 画面フラッシュ / 瀕死赤パルス制御
    static void TriggerFlash(DirectX::XMFLOAT4 color, float fadeSpeed = 0.05f);
    static void SetLowHPWarning(bool active) { m_IsLowHPWarning = active; }
    static DirectX::XMFLOAT4 GetFlashColor() { return m_FlashColor; }
};
