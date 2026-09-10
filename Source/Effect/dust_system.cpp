#include "dust_system.h"
#include "renderer.h"
#include "resource_manager.h"
#include "camera.h"
#include <vector>
#include <cmath>
#include <algorithm>

// =================================================================
// 静的メンバの実体定義
// =================================================================
DustParticle DustSystem::m_Particles[DustSystem::MAX_DUST];

ID3D11Buffer*            DustSystem::m_QuadVB      = nullptr;
ID3D11VertexShader*      DustSystem::m_VS          = nullptr;
ID3D11PixelShader*       DustSystem::m_PS          = nullptr;
ID3D11InputLayout*       DustSystem::m_IL          = nullptr;
ID3D11DepthStencilState* DustSystem::m_DepthState  = nullptr;
ID3D11ShaderResourceView* DustSystem::m_DustTexture = nullptr;

// =================================================================
// プロシージャルにソフト円形アルファテクスチャを生成
// =================================================================
ID3D11ShaderResourceView* DustSystem::CreateDustTexture(ID3D11Device* device)
{
    const int W = 128, H = 128;
    const float center = W * 0.5f;
    const float maxRadius = W * 0.46f;
    const float innerRadius = W * 0.28f;

    std::vector<BYTE> rgba(W * H * 4);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float dx = (float)x + 0.5f - center;
            float dy = (float)y + 0.5f - center;
            float dist = sqrtf(dx * dx + dy * dy);

            BYTE alpha = 0;
            if (dist <= innerRadius) {
                alpha = 255;
            }
            else if (dist < maxRadius) {
                float t = (dist - innerRadius) / (maxRadius - innerRadius);
                // 滑らかなコサイン補間フォールオフ
                float smoothT = 0.5f * (1.0f + cosf(t * XM_PI));
                alpha = static_cast<BYTE>(std::clamp(smoothT * 255.0f, 0.0f, 255.0f));
            }

            int idx = (y * W + x) * 4;
            rgba[idx + 0] = 255; // R
            rgba[idx + 1] = 255; // G
            rgba[idx + 2] = 255; // B
            rgba[idx + 3] = alpha; // A
        }
    }

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
    initData.pSysMem          = rgba.data();
    initData.SysMemPitch      = W * 4;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &initData, &tex);
    if (FAILED(hr)) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format                    = td.Format;
    srvd.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels       = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(tex, &srvd, &srv);
    tex->Release();

    return SUCCEEDED(hr) ? srv : nullptr;
}

// =================================================================
// 初期化処理
// =================================================================
void DustSystem::Init()
{
    // パーティクルプールの初期化
    for (size_t i = 0; i < MAX_DUST; ++i) {
        m_Particles[i].Active = false;
    }

    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    // 1. クアッド頂点バッファの作成（XY平面、中心原点）
    VERTEX_3D quadVertices[6] = {
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(0.0f, 1.0f) },

        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3( 0.5f,  0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3( 0.5f, -0.5f, 0.0f), XMFLOAT3(0, 0, -1), XMFLOAT4(1, 1, 1, 1), XMFLOAT2(1.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage          = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth      = sizeof(quadVertices);
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = quadVertices;
    device->CreateBuffer(&bd, &sd, &m_QuadVB);

    // 2. シェーダーの読み込み
    Renderer::CreateVertexShader(&m_VS, &m_IL, Renderer::ResolveShaderPath("vertexShader.cso").c_str());
    Renderer::CreatePixelShader(&m_PS, Renderer::ResolveShaderPath("ui_ps.cso").c_str());

    // 3. 深度ステートの作成（テストON、書き込みOFF）
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 半透明のため書き込みOFF
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &m_DepthState);

    // 4. テクスチャの生成
    m_DustTexture = CreateDustTexture(device);
}

// =================================================================
// 終了処理
// =================================================================
void DustSystem::Uninit()
{
    if (m_DustTexture) { m_DustTexture->Release(); m_DustTexture = nullptr; }
    if (m_DepthState)  { m_DepthState->Release();  m_DepthState = nullptr; }
    if (m_IL)          { m_IL->Release();          m_IL = nullptr; }
    if (m_PS)          { m_PS->Release();          m_PS = nullptr; }
    if (m_VS)          { m_VS->Release();          m_VS = nullptr; }
    if (m_QuadVB)      { m_QuadVB->Release();      m_QuadVB = nullptr; }
}

// =================================================================
// 毎フレーム更新処理
// =================================================================
void DustSystem::Update()
{
    for (size_t i = 0; i < MAX_DUST; ++i) {
        DustParticle& p = m_Particles[i];
        if (!p.Active) continue;

        p.Life--;
        if (p.Life <= 0) {
            p.Active = false;
            continue;
        }

        // 進行度 (0.0: 発生直後 -> 1.0: 消滅)
        float progress = 1.0f - (float)p.Life / (float)p.MaxLife;

        // 位置の更新（速度減速＋地面付近でのふんわり拡散）
        p.Position.x += p.Velocity.x;
        p.Position.y += p.Velocity.y;
        p.Position.z += p.Velocity.z;

        p.Velocity.x *= 0.92f;
        p.Velocity.z *= 0.92f;
        p.Velocity.y = (p.Velocity.y * 0.85f) + 0.001f; // 浮き上がりすぎず地面付近をふんわり漂う

        // 自転
        p.Rotation += p.RotSpeed;

        // スケールのアニメーション（前半でポンッと広がり、後半でゆっくり小さくなる）
        if (progress < 0.25f) {
            float t = progress / 0.25f;
            // イーズアウトで急激に広がる
            p.Scale = p.TargetScale * (1.0f - powf(1.0f - t, 3.0f));
        } else {
            float t = (progress - 0.25f) / 0.75f;
            p.Scale = p.TargetScale * (1.0f - t * 0.4f); // 60%の大きさまで縮む
        }

        // アルファのアニメーション（中盤からスーッと消える）
        if (progress < 0.2f) {
            p.Alpha = (progress / 0.2f) * 0.85f;
        } else {
            float t = (progress - 0.2f) / 0.8f;
            p.Alpha = (1.0f - t) * 0.85f;
        }
    }
}

// =================================================================
// 描画処理
// =================================================================
void DustSystem::Draw()
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    if (!ctx || !m_QuadVB || !m_VS || !m_PS || !m_DustTexture) return;

    // カメラのビュー行列から回転成分（ビルボード用）を抽出
    XMMATRIX viewMatrix = Renderer::GetViewMatrix();
    // ビュー行列の3x3回転成分を転置してカメラ向きに合わせる
    XMFLOAT4X4 viewF;
    XMStoreFloat4x4(&viewF, viewMatrix);
    XMMATRIX billboardRot = XMMatrixSet(
        viewF._11, viewF._21, viewF._31, 0.0f,
        viewF._12, viewF._22, viewF._32, 0.0f,
        viewF._13, viewF._23, viewF._33, 0.0f,
        0.0f,      0.0f,      0.0f,      1.0f
    );

    // パイプライン設定
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);
    ctx->IASetInputLayout(m_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_VS, nullptr, 0);
    ctx->PSSetShader(m_PS, nullptr, 0);

    ctx->OMSetDepthStencilState(m_DepthState, 0);
    ctx->PSSetShaderResources(0, 1, &m_DustTexture);

    for (size_t i = 0; i < MAX_DUST; ++i) {
        const DustParticle& p = m_Particles[i];
        if (!p.Active || p.Alpha <= 0.001f) continue;

        // 煙の底面が地面より下に沈まないよう、スケールに応じた半径分のリフトを加えて地面の上に自然に接地させる
        float drawY = p.Position.y + p.Scale * 0.45f;

        // ワールド行列: スケール * Z軸自転 * ビルボード回転 * 平行移動
        XMMATRIX world = XMMatrixScaling(p.Scale, p.Scale, p.Scale) *
                         XMMatrixRotationZ(p.Rotation) *
                         billboardRot *
                         XMMatrixTranslation(p.Position.x, drawY, p.Position.z);

        Renderer::SetWorldMatrix(world);

        // マテリアル設定（トゥーン調のソフトカラー）
        MATERIAL mat;
        ZeroMemory(&mat, sizeof(mat));
        mat.Diffuse       = XMFLOAT4(p.Color.x, p.Color.y, p.Color.z, p.Alpha * p.Color.w);
        mat.Ambient       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        mat.TextureEnable = TRUE;
        Renderer::SetMaterial(mat);

        ctx->Draw(6, 0);
    }

    // 深度ステートを通常に戻す
    ctx->OMSetDepthStencilState(Renderer::GetDepthStateEnable(), 0);
}

// =================================================================
// =================================================================
// 走行時の砂埃（足元から後方へポンッと出る）
// =================================================================
void DustSystem::AddRunDust(const XMFLOAT3& footPos, const XMFLOAT3& moveDir)
{
    const float GROUND_Y = -1.0f; // 地面のワールドY座標

    // 空きスロットを探す
    for (size_t i = 0; i < MAX_DUST; ++i) {
        DustParticle& p = m_Particles[i];
        if (!p.Active) {
            p.Active = true;

            // 少しランダムに散らす
            float offsetX = ((float)rand() / RAND_MAX - 0.5f) * 0.20f;
            float offsetZ = ((float)rand() / RAND_MAX - 0.5f) * 0.20f;

            // 進行方向の反対（足元後方）へオフセット
            float backOffsetX = -moveDir.x * 0.35f;
            float backOffsetZ = -moveDir.z * 0.35f;

            // 地面（Y = -1.0f）の上に確実に接地して生成
            p.Position = XMFLOAT3(footPos.x + backOffsetX + offsetX, GROUND_Y, footPos.z + backOffsetZ + offsetZ);

            // 移動方向の逆向きへふんわり飛ぶ
            float speed = 0.020f + ((float)rand() / RAND_MAX) * 0.015f;
            p.Velocity = XMFLOAT3(
                -moveDir.x * speed + ((float)rand() / RAND_MAX - 0.5f) * 0.012f,
                0.004f + ((float)rand() / RAND_MAX) * 0.006f,
                -moveDir.z * speed + ((float)rand() / RAND_MAX - 0.5f) * 0.012f
            );

            p.TargetScale = 0.38f + ((float)rand() / RAND_MAX) * 0.22f;
            p.Scale       = 0.05f;
            p.Alpha       = 0.0f;
            p.Rotation    = ((float)rand() / RAND_MAX) * XM_2PI;
            p.RotSpeed    = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.04f;
            p.MaxLife     = 24 + rand() % 8; // 約 0.4〜0.5 秒
            p.Life        = p.MaxLife;

            // やわらかい白〜わずかにクリームがかった砂埃カラー
            p.Color = XMFLOAT4(0.95f, 0.93f, 0.88f, 1.0f);
            break;
        }
    }
}

// =================================================================
// 着地時の砂埃（周囲へ円形に広がる）
// =================================================================
void DustSystem::AddLandDust(const XMFLOAT3& footPos, int count)
{
    const float GROUND_Y = -1.0f;

    for (int k = 0; k < count; ++k) {
        for (size_t i = 0; i < MAX_DUST; ++i) {
            DustParticle& p = m_Particles[i];
            if (!p.Active) {
                p.Active = true;

                float angle = (XM_2PI / count) * k + (((float)rand() / RAND_MAX) * 0.3f - 0.15f);
                float dist = 0.2f + ((float)rand() / RAND_MAX) * 0.15f;

                p.Position = XMFLOAT3(
                    footPos.x + cosf(angle) * dist,
                    GROUND_Y,
                    footPos.z + sinf(angle) * dist
                );

                float spd = 0.040f + ((float)rand() / RAND_MAX) * 0.025f;
                p.Velocity = XMFLOAT3(
                    cosf(angle) * spd,
                    0.005f + ((float)rand() / RAND_MAX) * 0.006f,
                    sinf(angle) * spd
                );

                p.TargetScale = 0.42f + ((float)rand() / RAND_MAX) * 0.22f;
                p.Scale       = 0.05f;
                p.Alpha       = 0.0f;
                p.Rotation    = ((float)rand() / RAND_MAX) * XM_2PI;
                p.RotSpeed    = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.05f;
                p.MaxLife     = 28 + rand() % 8;
                p.Life        = p.MaxLife;
                p.Color       = XMFLOAT4(0.96f, 0.94f, 0.90f, 1.0f);
                break;
            }
        }
    }
}

// =================================================================
// ダッシュ時の砂埃（後方へ激しく噴出）
// =================================================================
void DustSystem::AddDashDust(const XMFLOAT3& footPos, const XMFLOAT3& dashDir)
{
    const float GROUND_Y = -1.0f;

    for (int k = 0; k < 4; ++k) {
        for (size_t i = 0; i < MAX_DUST; ++i) {
            DustParticle& p = m_Particles[i];
            if (!p.Active) {
                p.Active = true;

                float offsetX = ((float)rand() / RAND_MAX - 0.5f) * 0.30f;
                float offsetZ = ((float)rand() / RAND_MAX - 0.5f) * 0.30f;
                float backOffsetX = -dashDir.x * 0.45f;
                float backOffsetZ = -dashDir.z * 0.45f;

                p.Position = XMFLOAT3(footPos.x + backOffsetX + offsetX, GROUND_Y, footPos.z + backOffsetZ + offsetZ);

                float spd = 0.06f + ((float)rand() / RAND_MAX) * 0.04f;
                p.Velocity = XMFLOAT3(
                    -dashDir.x * spd + ((float)rand() / RAND_MAX - 0.5f) * 0.025f,
                    0.006f + ((float)rand() / RAND_MAX) * 0.010f,
                    -dashDir.z * spd + ((float)rand() / RAND_MAX - 0.5f) * 0.025f
                );

                p.TargetScale = 0.55f + ((float)rand() / RAND_MAX) * 0.30f;
                p.Scale       = 0.1f;
                p.Alpha       = 0.0f;
                p.Rotation    = ((float)rand() / RAND_MAX) * XM_2PI;
                p.RotSpeed    = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * 0.08f;
                p.MaxLife     = 30 + rand() % 10;
                p.Life        = p.MaxLife;
                p.Color       = XMFLOAT4(1.0f, 0.98f, 0.92f, 1.0f);
                break;
            }
        }
    }
}
