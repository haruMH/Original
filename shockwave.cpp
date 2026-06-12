#include "shockwave.h"
#include "renderer.h"
#include "manager.h"
#include "enemy.h"
#include "game_rule.h"
#include "score_popup.h"
#include <vector>

// =================================================================
// 静的メンバ変数の実体定義
// =================================================================
std::list<ShockwaveEntry> ShockwaveSystem::m_Shockwaves;

ID3D11Buffer*            ShockwaveSystem::m_QuadVB     = nullptr;
ID3D11VertexShader*      ShockwaveSystem::m_VS         = nullptr;
ID3D11PixelShader*       ShockwaveSystem::m_PS         = nullptr;
ID3D11InputLayout*       ShockwaveSystem::m_IL         = nullptr;
ID3D11DepthStencilState* ShockwaveSystem::m_DepthState = nullptr;
ID3D11ShaderResourceView* ShockwaveSystem::m_Texture    = nullptr;

// =================================================================
// GDI を用いてドーナツ型リングのアルファテクスチャを作成する
// =================================================================
ID3D11ShaderResourceView* ShockwaveSystem::CreateRingTexture(ID3D11Device* device)
{
    const int W = 256, H = 256;

    // --- GDI DIB セクションの作成 ---
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;  // トップダウン
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    // 背景を黒（アルファ0）でクリア
    memset(bits, 0, W * H * 4);

    // 円を描画するための太線のペンと透明ブラシを作成
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH)); // 塗りつぶしなし

    // 三重の同心円をそれぞれ太さを変えて描く（視認性向上のため線を太く配置を調整）
    HPEN hPen1 = CreatePen(PS_SOLID, 12, RGB(255, 255, 255));
    HGDIOBJ oldPen = SelectObject(hdc, hPen1);
    Ellipse(hdc, 16, 16, W - 16, H - 16);

    HPEN hPen2 = CreatePen(PS_SOLID, 9, RGB(255, 255, 255));
    SelectObject(hdc, hPen2);
    Ellipse(hdc, 52, 52, W - 52, H - 52);

    HPEN hPen3 = CreatePen(PS_SOLID, 6, RGB(255, 255, 255));
    SelectObject(hdc, hPen3);
    Ellipse(hdc, 88, 88, W - 88, H - 88);

    GdiFlush();

    // --- アルファ値（輝度）への変換処理 ---
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3); // 輝度 -> アルファ
        
        rgba[i * 4 + 0] = 255; // R
        rgba[i * 4 + 1] = 255; // G
        rgba[i * 4 + 2] = 255; // B
        rgba[i * 4 + 3] = a;   // A: ドーナツ型マスク
    }

    // --- D3D11 テクスチャの生成 ---
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = W;
    td.Height           = H;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem     = rgba.data();
    initData.SysMemPitch = W * 4;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &initData, &tex);

    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(hr) && tex) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }

    // リソース解放
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBmp);
    DeleteObject(hPen1);
    DeleteObject(hPen2);
    DeleteObject(hPen3);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// =================================================================
// 初期化処理
// =================================================================
bool ShockwaveSystem::Init(ID3D11Device* device)
{
    if (!device) return false;

    // --- 地面に水平なクアッド頂点バッファの作成 ---
    // Y=0.0f の平面上に作成
    struct VERTEX_3D_LOCAL {
        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT4 Diffuse;
        XMFLOAT2 TexCoord;
        XMFLOAT3 Tangent;
    } v[4];

    // TL: 左上
    v[0].Position = XMFLOAT3(-0.5f, 0.0f,  0.5f);
    v[0].Normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
    v[0].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    v[0].Tangent  = XMFLOAT3(1, 0, 0);

    // TR: 右上
    v[1].Position = XMFLOAT3( 0.5f, 0.0f,  0.5f);
    v[1].Normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
    v[1].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
    v[1].Tangent  = XMFLOAT3(1, 0, 0);

    // BL: 左下
    v[2].Position = XMFLOAT3(-0.5f, 0.0f, -0.5f);
    v[2].Normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
    v[2].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
    v[2].Tangent  = XMFLOAT3(1, 0, 0);

    // BR: 右下
    v[3].Position = XMFLOAT3( 0.5f, 0.0f, -0.5f);
    v[3].Normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
    v[3].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[3].TexCoord = XMFLOAT2(1.0f, 1.0f);
    v[3].Tangent  = XMFLOAT3(1, 0, 0);

    D3D11_BUFFER_DESC bd = {};
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth = sizeof(v);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = v;
    if (FAILED(device->CreateBuffer(&bd, &sd, &m_QuadVB))) return false;

    // --- シェーダー読み込み ---
    Renderer::CreateVertexShader(&m_VS, &m_IL, "vertexShader.cso");
    Renderer::CreatePixelShader(&m_PS, "ui_ps.cso");

    if (!m_VS || !m_PS || !m_IL) {
        OutputDebugStringA("[ShockwaveSystem] VS/PS/IL Load Failed\n");
        return false;
    }

    // --- 深度ステートの作成（テストON / 書き込みOFF）---
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(device->CreateDepthStencilState(&dsd, &m_DepthState))) return false;

    // --- リングテクスチャ生成 ---
    m_Texture = CreateRingTexture(device);
    if (!m_Texture) return false;

    OutputDebugStringA("[ShockwaveSystem] Init Succeeded\n");
    return true;
}

// =================================================================
// 終了処理
// =================================================================
void ShockwaveSystem::Uninit()
{
    m_Shockwaves.clear();

    if (m_Texture)    { m_Texture->Release();    m_Texture    = nullptr; }
    if (m_DepthState) { m_DepthState->Release(); m_DepthState = nullptr; }
    if (m_IL)         { m_IL->Release();         m_IL         = nullptr; }
    if (m_PS)         { m_PS->Release();         m_PS         = nullptr; }
    if (m_VS)         { m_VS->Release();         m_VS         = nullptr; }
    if (m_QuadVB)     { m_QuadVB->Release();     m_QuadVB     = nullptr; }
}

// =================================================================
// 毎フレーム更新処理
// =================================================================
void ShockwaveSystem::Update()
{
    for (auto it = m_Shockwaves.begin(); it != m_Shockwaves.end(); ) {
        if (it->Delay > 0) {
            it->Delay--;
            ++it;
            continue;
        }

        // ディレイが終了した瞬間（Delay == 0）、かつ物理未適用のフレームに物理効果を適用
        if (it->Delay == 0 && !it->PhysicsApplied) {
            ApplyShockwavePhysics(it->Position, it->MaxRadius, it->Force);
            it->PhysicsApplied = true;
        }

        it->Timer--;
        if (it->Timer <= 0) {
            it = m_Shockwaves.erase(it);
        } else {
            ++it;
        }
    }
}

// =================================================================
// 毎フレーム描画処理（Renderer::End() の直前に呼ぶ）
// =================================================================
void ShockwaveSystem::Draw()
{
    if (m_Shockwaves.empty()) return;
    if (!m_VS || !m_PS || !m_QuadVB || !m_IL || !m_DepthState || !m_Texture) return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

    // --- 描画ステートを設定する ---
    ctx->OMSetDepthStencilState(m_DepthState, 0);
    ctx->VSSetShader(m_VS, nullptr, 0);
    ctx->PSSetShader(m_PS, nullptr, 0);
    ctx->IASetInputLayout(m_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    UINT stride = 60u;
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);

    // テクスチャバインド
    Renderer::SetTexture(m_Texture);

    // --- 各衝撃波を描画する ---
    for (const auto& entry : m_Shockwaves) {
        if (entry.Delay > 0) continue; // ディレイ中の波紋は描画しない

        float progress = 1.0f - (float)entry.Timer / (float)entry.MaxTimer; // 0.0 -> 1.0
        
        // イージング（最初は速く、後半はゆっくり広がる）
        float t = progress;
        float easeProgress = 1.0f - (1.0f - t) * (1.0f - t); 
        
        float currentRadius = entry.MaxRadius * easeProgress;
        
        // アルファは時間経過で線形フェードアウト
        float alpha = 1.0f - progress;

        // ワールド行列の作成 (Yは地面-1.0fと重なってチラつくのを防ぐため0.05f浮かせる)
        XMMATRIX world = XMMatrixScaling(currentRadius * 2.0f, 1.0f, currentRadius * 2.0f) 
                       * XMMatrixTranslation(entry.Position.x, entry.Position.y + 0.05f, entry.Position.z);
        Renderer::SetWorldMatrix(world);

        // マテリアルの設定 (Emission に発光色、Diffuse.a にフェードアウト)
        MATERIAL mat = {};
        mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
        mat.Emission = XMFLOAT4(entry.ColorR, entry.ColorG, entry.ColorB, 0.0f);
        mat.TextureEnable = FALSE;
        Renderer::SetMaterial(mat);

        ctx->Draw(4, 0);
    }

    // --- 後処理 ---
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// =================================================================
// 衝撃波を追加し、周囲の敵への物理吹き飛ばしも適用する
// =================================================================
void ShockwaveSystem::AddShockwave(
    const XMFLOAT3& pos, 
    float maxRadius, 
    float colorR, 
    float colorG, 
    float colorB,
    int duration,
    float force,
    int delay
)
{
    // 描画エフェクトの登録
    ShockwaveEntry entry = {};
    entry.Position       = pos;
    entry.Timer          = duration;
    entry.MaxTimer       = duration;
    entry.MaxRadius      = maxRadius;
    entry.ColorR         = colorR;
    entry.ColorG         = colorG;
    entry.ColorB         = colorB;
    entry.Delay          = delay;
    entry.PhysicsApplied = false;
    entry.Force          = force;
    m_Shockwaves.push_back(entry);

    // ディレイが0の場合のみ、即時に物理効果を適用する
    if (delay == 0) {
        ApplyShockwavePhysics(pos, maxRadius, force);
        m_Shockwaves.back().PhysicsApplied = true;
    }
}

// =================================================================
// 物理吹き飛ばし処理の適用
// =================================================================
void ShockwaveSystem::ApplyShockwavePhysics(const XMFLOAT3& center, float radius, float forceVal)
{
    if (forceVal <= 0.0f) return;

    for (GameObject* obj : Manager::GetGameObjectList()) {
        if (!obj || obj->IsDestroy()) continue;

        if (obj->GetObjectType() != ObjectType::Enemy) continue;
        Enemy* enemy = static_cast<Enemy*>(obj);

        EnemyState state = enemy->GetEnemyState();
        // すでに倒されている、あるいは持ち上げられている敵は物理対象外
        if (state == EnemyState::DEFEATED || state == EnemyState::GRABBED || state == EnemyState::VACUUMED) continue;

        XMFLOAT3 ePos = enemy->GetPosition();
        float dx = ePos.x - center.x;
        float dy = ePos.y - center.y;
        float dz = ePos.z - center.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        // 衝撃波の半径内にあるか
        if (distSq < radius * radius) {
            // 自分自身（激突した巨大エネミー本人など、現在FLYING状態）は除外する
            if (state == EnemyState::FLYING && enemy->GetScale().x > 2.0f) continue;

            float dist = sqrtf(distSq);
            if (dist < 0.01f) dist = 0.01f;

            // 距離による力の減衰
            float attenuation = (radius - dist) / radius;
            if (attenuation < 0.0f) attenuation = 0.0f;

            // 外側へ向かう方向ベクトル
            XMFLOAT3 dir = XMFLOAT3(dx / dist, 0.0f, dz / dist);

            // 吹き飛ばしベクトルの構築
            float finalForce = forceVal * attenuation;
            XMFLOAT3 vel;
            vel.x = dir.x * finalForce;
            vel.y = 0.35f * attenuation + 0.15f; // 上空へ軽く打ち上げる
            vel.z = dir.z * finalForce;

            // 撃破処理（衝撃波・黄金オレンジポップアップ）
            enemy->Defeat(2.5f, 0.8f, 0.0f);

            // 吹き飛ばし状態の設定
            enemy->SetVelocity(vel);
            enemy->SetEnemyState(EnemyState::BLOWN_AWAY);
        }
    }
}
