#pragma once
#include <vector>
#include <type_traits>
#include "render_system.h"
#include "gameobject.h"
#include "scene_interface.h"

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
    // 各シーンクラスから静的メンバ（オブジェクトリスト等）への直接アクセスを許可します
    friend class TitleScene;
    friend class GameplayScene;
    friend class ClearScene;
    friend class GameOverScene;

private:
    static std::vector<GameObject*> m_GameObjects;     // 全オブジェクトリスト（描画・一括走査用）
    static std::vector<GameObject*> m_UpdateObjects;   // 更新対象（動的）オブジェクトのリスト（更新処理高速化用）
    static Player*                m_CachedPlayer;     // キャッシュされたプレイヤーへのポインタ (O(1)アクセス用)
    static RenderSystem           m_RenderSystem;     // インスタンスバッチ描画システム
    static int                    m_HitStopFrames;    // ヒットストップの残りフレーム数
    static int                    m_SlowMotionTimer;  // スローモーションの残りフレーム数
    static int                    m_SlowMotionDuration;// スローモーションの総開始フレーム数
    static bool                   m_IsBossStage;      // ボスステージ中フラグ
    static Scene                  m_CurrentScene;     // 現在のシーン（既存コード互換用）
    static Scene                  m_NextScene;        // 遷移予定の次のシーン
    static bool                   m_SceneTransitionRequested; // シーン遷移リクエストフラグ

    static IScene*                m_ActiveScene;      // 現在アクティブなシーンオブジェクト

    // カテゴリ別オブジェクトキャッシュリスト
    static std::vector<Enemy*>       m_Enemies;
    static std::vector<Wall*>        m_Walls;
    static std::vector<Item*>        m_Items;
    static std::vector<EnemyBullet*> m_Bullets;

    static void ExecuteChangeScene(Scene nextScene); // 実際のシーン遷移実行

public:
    // 初期化と終了処理
    static void Init();
    static void Uninit();
    
    // 更新と描画（アクティブなシーンオブジェクトに処理を委譲します）
    static void Update();
    static void Draw();

    // テンプレート関数によるオブジェクトの生成・追加.
    template<typename ObjT>
    static ObjT* AddGameObject()
    {
        ObjT* gameObject = new ObjT();
        gameObject->Init();
        m_GameObjects.push_back(gameObject);
        // 静的オブジェクト（Wall, Field）は更新不要なため、更新リストから除外してパフォーマンスを向上
        ::ObjectType t = gameObject->GetObjectType();
        if (t != ::ObjectType::Wall && t != ::ObjectType::Field) {
            m_UpdateObjects.push_back(gameObject);
        }

        // プレイヤーが生成された場合はキャッシュポインタに保存
        if constexpr (std::is_same_v<ObjT, Player>) {
            m_CachedPlayer = reinterpret_cast<Player*>(gameObject);
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
        if constexpr (std::is_same_v<TargetType, Player>) {
            return reinterpret_cast<TargetType*>(m_CachedPlayer);
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
    static const std::vector<Enemy*>&       GetEnemyList()  { return m_Enemies; }
    static const std::vector<Wall*>&        GetWallList()   { return m_Walls; }
    static const std::vector<Item*>&        GetItemList()   { return m_Items; }
    static const std::vector<EnemyBullet*>& GetBulletList() { return m_Bullets; }

    // カテゴリ別リストへの登録・解除用ヘルパー
    static void RegisterCategory(GameObject* obj);
    static void UnregisterCategory(GameObject* obj);
    static void ClearCategoryLists();

    // ヒットストップのフレーム設定.
    static void AddHitStop(int frames) { m_HitStopFrames = frames; }

    // 現在ヒットストップ中かどうか.
    static bool IsHitStopping() { return m_HitStopFrames > 0; }

    // スローモーション（ウィッチタイム）制御用
    static void  StartSlowMotion(int duration) { m_SlowMotionTimer = duration; m_SlowMotionDuration = duration; }
    static float GetSlowMotionIntensity();
    static bool  IsSlowMotionActive() { return m_SlowMotionTimer > 0; }
    static bool  IsBossStage() { return m_IsBossStage; }
    static void  TransitionToBossStage();
    static void  ChangeScene(Scene nextScene);
    static Scene GetCurrentScene() { return m_CurrentScene; }
};
