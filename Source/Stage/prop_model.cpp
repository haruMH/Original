#include "prop_model.h"

using namespace DirectX;

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
    m_Rotation = XMFLOAT3(0.0f, 0.5f, 0.0f);
    m_Scale    = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_Size     = XMFLOAT3(1.0f, 1.0f, 1.0f);
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
    m_Model = std::make_shared<ObjModel>();
    return m_Model->Load(objPath, mtlPath);
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
