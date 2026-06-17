#pragma once
#include <list>
#include "render_system.h"
#include "gameobject.h"

// シーン状態を定義する列挙型
enum class Scene {
    TITLE,
    GAMEPLAY,
    CLEAR,
    GAMEOVER
};

// =================================================================
// ゲーム統括管理クラス (Manager)
// =================================================================
class Manager
{
private:
    static std::list<GameObject*> m_GameObjects;     // シーン上の全オブジェクトリスト
    static RenderSystem           m_RenderSystem;     // インスタンス一括描画システム
    static int                    m_HitStopFrames;    // ヒットストップの残りフレーム数
    static int                    m_SlowMotionTimer;  // スローモーションの残りフレーム数
    static int                    m_SlowMotionDuration;// スローモーションの総開始フレーム数
    static bool                   m_IsBossStage;      // ボスステージ中フラグ
    static Scene                  m_CurrentScene;     // 現在のシーン状態

public:
    // 初期化と終了処理
    static void Init();
    static void Uninit();
    
    // 更新と描画
    static void Update();
    static void UpdateGameplay();
    static void Draw();

    // テンプレート関数によるオブジェクトの生成・追加.
    template<typename ObjectType>
    static ObjectType* AddGameObject()
    {
        ObjectType* gameObject = new ObjectType();
        gameObject->Init();
        m_GameObjects.push_back(gameObject);
        return gameObject;
    }

    // テンプレート関数による特定の型のオブジェクトの取得.
    template<typename ObjectType>
    static ObjectType* GetGameObject()
    {
        for (GameObject* gameObject : m_GameObjects)
        {
            ObjectType* find = dynamic_cast<ObjectType*>(gameObject);
            if (find != nullptr)
                return find;
        }
        return nullptr;
    }

    // テンプレート関数による特定の型の全オブジェクトの一括取得.
    template<typename ObjectType>
    static std::list<ObjectType*> GetGameObjects()
    {
        std::list<ObjectType*> list;
        for (GameObject* gameObject : m_GameObjects)
        {
            ObjectType* find = dynamic_cast<ObjectType*>(gameObject);
            if (find != nullptr)
            {
                list.push_back(find);
            }
        }
        return list;
    }

    // オブジェクトリストの取得.
    static const std::list<GameObject*>& GetGameObjectList() { return m_GameObjects; }

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
