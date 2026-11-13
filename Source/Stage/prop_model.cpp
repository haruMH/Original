#include "prop_model.h"

#include <unordered_map>

using namespace DirectX;

namespace {
    // 同じ OBJ は 1 回だけ読み込んで共有する（キーは obj + mtl のパス）
    std::unordered_map<std::string, std::shared_ptr<ObjModel>> g_ModelCache;
}

void PropModel::ClearModelCache()
{
    g_ModelCache.clear();
    ObjModel::ReleaseSharedResources();
}

// =================================================================
// コンストラクタ / デストラクタ
// =================================================================
PropModel::PropModel()
    : m_Model(nullptr)
{
}

PropModel::~PropModel()
{
    Uninit();
}

// =================================================================
// 初期化
// =================================================================
void PropModel::Init()
{
    m_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(0.0f, 0.0f, 0.0f); // デフォルトは衝突なし。必要なプロップのみSetSizeで設定
}

// =================================================================
// 終了処理
// =================================================================
void PropModel::Uninit()
{
    m_Model.reset();
}

// =================================================================
// 更新処理
// =================================================================
void PropModel::Update()
{
}

// =================================================================
// モデル読み込み
// =================================================================
bool PropModel::LoadModel(const std::string& objPath, const std::string& mtlPath)
{
    const std::string key = objPath + "|" + mtlPath;
    auto it = g_ModelCache.find(key);
    if (it != g_ModelCache.end()) {
        m_Model = it->second;
        return true;
    }

    auto model = std::make_shared<ObjModel>();
    if (!model->Load(objPath, mtlPath)) {
        m_Model = model;   // 読み込み失敗でも描画側は null チェック済み。キャッシュはしない
        return false;
    }
    g_ModelCache[key] = model;
    m_Model = model;
    return true;
}

// =================================================================
// 描画処理
// =================================================================
void PropModel::Draw()
{
    if (!m_Model) return;

    XMMATRIX scaleM = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    XMMATRIX rotM   = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    XMMATRIX transM = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    XMMATRIX world  = scaleM * rotM * transM;

    m_Model->Draw(world);
}
