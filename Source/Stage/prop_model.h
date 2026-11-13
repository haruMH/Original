#pragma once
#include "gameobject.h"
#include "obj_model.h"
#include <memory>

// =================================================================
// 3Dモデル配置用ゲームオブジェクト
// =================================================================
class PropModel : public GameObject
{
public:
    PropModel();
    virtual ~PropModel();

    virtual void Init() override;
    virtual void Uninit() override;
    virtual void Update() override;
    virtual void Draw() override;

    // モデルファイルの読み込み
    bool LoadModel(const std::string& objPath, const std::string& mtlPath = "");

    // 配置モデルオブジェクト
    virtual ObjectType GetObjectType() const override { return ObjectType::Prop; }

    // 共有キャッシュの全解放（アプリ終了時に呼ぶ）
    static void ClearModelCache();

private:
    std::shared_ptr<ObjModel> m_Model;
};
