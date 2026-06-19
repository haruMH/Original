#include "score_popup.h"
#include "renderer.h"
#include "manager.h"
#include <vector>

// =================================================================
// 静的メンバ変数の実体定義
// =================================================================
std::list<ScorePopupEntry>
    ScorePopupSystem::m_Popups;

std::unordered_map<int, ID3D11ShaderResourceView*>
    ScorePopupSystem::m_TextureCache;

ID3D11Buffer*            ScorePopupSystem::m_QuadVB     = nullptr;
ID3D11VertexShader*      ScorePopupSystem::m_VS         = nullptr;
ID3D11PixelShader*       ScorePopupSystem::m_PS         = nullptr;
ID3D11InputLayout*       ScorePopupSystem::m_IL         = nullptr;
ID3D11DepthStencilState* ScorePopupSystem::m_DepthState = nullptr;

// ─────────────────────────────────────────────────────────────────
// GDI でスコアテキストのテクスチャを作成する
// テキスト部分をアルファ（形状マスク）にし、色は Emission で設定する
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScorePopupSystem::CreateScoreTexture(
    ID3D11Device* device,
    const wchar_t* text
)
{
    const int W = 256, H = 64;

    // ─── GDI DIB セクションの作成 ───
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;  // 上から下（トップダウン）で描画する
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;  // BGRA 32bit
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    // 背景を黒で塗りつぶす（後でアルファ=0 に変換する）
    memset(bits, 0, W * H * 4);

    // ─── フォントの作成（Impact: 太くてゲームに映える）───
    HFONT hFont = CreateFontW(
        50, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Impact"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFont);

    // ─── テキスト描画（白色、背景透過）───
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rc = { 0, 0, W, H };
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    GdiFlush();

    // ─── GDI BGRA バッファから RGBA テクスチャデータを生成 ───
    // GDI は 32bit DIB の alpha を設定しないため、
    // テキストピクセルは RGB が白系で alpha は 0 のまま残る。
    // そこで輝度値（平均 RGB）を alpha チャンネルとして使用する。
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];  // GDI は BGRA 順
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3); // 輝度 → アルファ変換
        rgba[i * 4 + 0] = 255;  // R: 白（Emission でカラー指定するため白でよい）
        rgba[i * 4 + 1] = 255;  // G
        rgba[i * 4 + 2] = 255;  // B
        rgba[i * 4 + 3] = a;    // A: テキスト輝度から生成したアルファ
    }

    // ─── D3D11 テクスチャの作成 ───
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

    // ─── GDI リソースの解放 ───
    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFont);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// ─────────────────────────────────────────────────────────────────
// 初期化処理
// ─────────────────────────────────────────────────────────────────
bool ScorePopupSystem::Init(ID3D11Device* device)
{
    if (!device) return false;

    // ─── ビルボードクアッドの頂点バッファを作成 ───
    // ローカル座標 [-0.5, 0.5] の TRIANGLESTRIP 4頂点（TL/TR/BL/BR 順）
    // カメラから正しくCCWに見えるようこの並びにする
    struct VERTEX_3D_LOCAL {
        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT4 Diffuse;
        XMFLOAT2 TexCoord;
        XMFLOAT3 Tangent;
    } v[4];

    // TL: 左上
    v[0].Position = XMFLOAT3(-0.5f,  0.5f, 0.0f);
    v[0].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[0].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    v[0].Tangent  = XMFLOAT3(1, 0, 0);
    // TR: 右上
    v[1].Position = XMFLOAT3( 0.5f,  0.5f, 0.0f);
    v[1].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[1].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
    v[1].Tangent  = XMFLOAT3(1, 0, 0);
    // BL: 左下
    v[2].Position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
    v[2].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[2].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
    v[2].Tangent  = XMFLOAT3(1, 0, 0);
    // BR: 右下
    v[3].Position = XMFLOAT3( 0.5f, -0.5f, 0.0f);
    v[3].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
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

    // ─── シェーダーの読み込み ───
    // リリースビルド時は Assets/shader/ サブフォルダから読み込む
#ifdef NDEBUG
    // 頂点シェーダーは vertexShader.cso を流用する
    Renderer::CreateVertexShader(&m_VS, &m_IL, "Assets/shader/vertexShader.cso");
    // ピクセルシェーダーは UI専用の ui_ps.cso を読み込む
    Renderer::CreatePixelShader(&m_PS, "Assets/shader/ui_ps.cso");
#else
    // 頂点シェーダーは vertexShader.cso を流用する
    Renderer::CreateVertexShader(&m_VS, &m_IL, "vertexShader.cso");
    // ピクセルシェーダーは UI専用の ui_ps.cso を読み込む
    Renderer::CreatePixelShader(&m_PS, "ui_ps.cso");
#endif

    if (!m_VS || !m_PS || !m_IL) {
        OutputDebugStringA("[ScorePopupSystem] シェーダー読み込み失敗\n");
        return false;
    }

    // ─── 深度ステートの作成（テストON / 書き込みOFF）───
    // UIは3D空間内に正しく奥行きで隠れるが、深度バッファを汚さない
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dsd, &m_DepthState);

    OutputDebugStringA("[ScorePopupSystem] 初期化完了\n");
    return true;
}

// ─────────────────────────────────────────────────────────────────
// 終了処理（全リソースを解放する）
// ─────────────────────────────────────────────────────────────────
void ScorePopupSystem::Uninit()
{
    // 生存中のポップアップのテクスチャ参照を解放する
    for (auto& entry : m_Popups) {
        if (entry.Texture) {
            entry.Texture->Release();
            entry.Texture = nullptr;
        }
    }
    m_Popups.clear();

    // テクスチャキャッシュを解放する
    for (auto& pair : m_TextureCache) {
        if (pair.second) pair.second->Release();
    }
    m_TextureCache.clear();

    // GPU リソースを解放する
    if (m_DepthState) { m_DepthState->Release(); m_DepthState = nullptr; }
    if (m_IL)         { m_IL->Release();         m_IL         = nullptr; }
    if (m_PS)         { m_PS->Release();         m_PS         = nullptr; }
    if (m_VS)         { m_VS->Release();         m_VS         = nullptr; }
    if (m_QuadVB)     { m_QuadVB->Release();     m_QuadVB     = nullptr; }
}

// ─────────────────────────────────────────────────────────────────
// 毎フレーム更新処理
// ─────────────────────────────────────────────────────────────────
void ScorePopupSystem::Update()
{
    for (auto it = m_Popups.begin(); it != m_Popups.end(); ) {
        it->Timer--;

        // Y方向に一定速度で浮き上がらせる（イーズアウト: 最初速く後半ゆっくり）
        float progress = 1.0f - (float)it->Timer / (float)it->MaxTimer; // 0.0→1.0
        it->OffsetY = 2.2f * (1.0f - (1.0f - progress) * (1.0f - progress));

        // タイムアップしたポップアップを削除する
        if (it->Timer <= 0) {
            if (it->Texture) {
                it->Texture->Release();
                it->Texture = nullptr;
            }
            it = m_Popups.erase(it);
        } else {
            ++it;
        }
    }
}

// ─────────────────────────────────────────────────────────────────
// 毎フレーム描画処理（Renderer::End() の直前に呼ぶ）
// ─────────────────────────────────────────────────────────────────
void ScorePopupSystem::Draw()
{
    if (m_Popups.empty()) return;
    if (!m_VS || !m_PS || !m_QuadVB || !m_IL || !m_DepthState) return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

    // ─── 描画ステートを設定する ───
    ctx->OMSetDepthStencilState(m_DepthState, 0);
    ctx->VSSetShader(m_VS, nullptr, 0);
    ctx->PSSetShader(m_PS, nullptr, 0);
    ctx->IASetInputLayout(m_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    UINT stride = 60u; // sizeof(VERTEX_3D): Position(12)+Normal(12)+Diffuse(16)+TexCoord(8)+Tangent(12)
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);

    // ─── ビュー行列からカメラ右・上・前ベクトルを取得する ───
    // XMMatrixLookAtLH の行0=right, 行1=up, 行2=forward（ワールド空間）
    XMMATRIX view = Renderer::GetViewMatrix();
    XMVECTOR camRight = XMVectorSet(
        XMVectorGetX(view.r[0]),
        XMVectorGetY(view.r[0]),
        XMVectorGetZ(view.r[0]),
        0.0f
    );
    XMVECTOR camUp = XMVectorSet(
        XMVectorGetX(view.r[1]),
        XMVectorGetY(view.r[1]),
        XMVectorGetZ(view.r[1]),
        0.0f
    );
    XMVECTOR camFwd = XMVectorSet(
        XMVectorGetX(view.r[2]),
        XMVectorGetY(view.r[2]),
        XMVectorGetZ(view.r[2]),
        0.0f
    );

    // ─── 各ポップアップを描画する ───
    for (const auto& entry : m_Popups) {
        if (!entry.Texture) continue;

        // スケールアニメーション（ポップインの演出）
        int age = entry.MaxTimer - entry.Timer;
        float scaleF;
        if (age < 8) {
            // 生成直後: 0.0 → 1.3 に急速拡大する
            scaleF = (float)age / 8.0f * 1.3f;
        } else if (age < 14) {
            // 少し縮んで 1.0 に落ち着かせる（バウンス）
            float t = (float)(age - 8) / 6.0f;
            scaleF = 1.3f + (1.0f - 1.3f) * t;
        } else {
            scaleF = 1.0f;
        }

        // フェードアウトアルファの計算（残り22フレームから透明になる）
        const float fadeStart = 22.0f;
        float alpha = (entry.Timer < (int)fadeStart)
                    ? (float)entry.Timer / fadeStart
                    : 1.0f;

        // ─── ビルボードワールド行列を構築する ───
        // ローカルX = カメラ右方向 × 横幅、ローカルY = カメラ上方向 × 縦幅
        // テクスチャ比率 256:64 = 4:1 を反映させる
        float width  = 2.0f * scaleF;   // ワールド横幅（4:1 比率を設定する）
        float height = 0.5f * scaleF;   // ワールド縦幅
        XMVECTOR pos = XMVectorSet(
            entry.WorldX,
            entry.WorldY + entry.OffsetY,
            entry.WorldZ,
            1.0f
        );

        XMMATRIX billWorld;
        billWorld.r[0] = camRight * width;            // ローカルX: カメラ右方向 × 幅
        billWorld.r[1] = camUp * height;              // ローカルY: カメラ上方向 × 高さ
        billWorld.r[2] = XMVectorNegate(camFwd);      // ローカルZ: カメラへ向かう方向
        billWorld.r[3] = pos;                         // 位置（w=1）

        Renderer::SetWorldMatrix(billWorld);

        // マテリアル設定（Diffuse.a = フェード、Emission = 発光カラー）
        MATERIAL mat = {};
        mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
        mat.Emission = XMFLOAT4(
            entry.EmitR,
            entry.EmitG,
            entry.EmitB,
            0.0f
        );
        mat.TextureEnable = FALSE;  // ui_ps.hlsl はテクスチャをαマスクのみで使用する
        Renderer::SetMaterial(mat);

        // テクスチャをスロット 0 にバインドする
        Renderer::SetTexture(entry.Texture);

        ctx->Draw(4, 0);
    }

    // ─── 後処理: トポロジーをリストに戻す（後続のレンダリングへの影響を防ぐ）───
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ─────────────────────────────────────────────────────────────────
// スコアポップアップを黄金色で発生させる（通常撃破用）
// ─────────────────────────────────────────────────────────────────
void ScorePopupSystem::AddPopup(float x, float y, float z, int score)
{
    // デフォルトは黄金色（スコア+ブルームに映える明るい色）
    AddPopup(x, y, z, score, 1.5f, 1.2f, 0.0f);
}

// ─────────────────────────────────────────────────────────────────
// スコアポップアップをカラー指定で発生させる
// ─────────────────────────────────────────────────────────────────
void ScorePopupSystem::AddPopup(float x, float y, float z, int score,
                                 float emitR, float emitG, float emitB)
{
    ID3D11Device* device = Renderer::GetDevice();
    if (!device) return;

    // テクスチャキャッシュを確認する（同じスコア値は使い回す）
    auto cacheIt = m_TextureCache.find(score);
    ID3D11ShaderResourceView* tex = nullptr;
    if (cacheIt != m_TextureCache.end()) {
        tex = cacheIt->second;
    } else {
        // 新しいテクスチャを生成してキャッシュに登録する
        wchar_t buf[32];
        swprintf_s(buf, L"+%d", score);
        tex = CreateScoreTexture(device, buf);
        if (tex) m_TextureCache[score] = tex;
    }

    if (!tex) return;

    // ポップアップは独立した参照カウントを持つ（AddRef してから登録する）
    tex->AddRef();

    ScorePopupEntry entry = {};
    entry.WorldX   = x;
    entry.WorldY   = y;
    entry.WorldZ   = z;
    entry.Timer    = 90;   // 1.5秒間（60fps 基準）
    entry.MaxTimer = 90;
    entry.OffsetY  = 0.0f;
    entry.EmitR    = emitR;
    entry.EmitG    = emitG;
    entry.EmitB    = emitB;
    entry.Texture  = tex;

    m_Popups.push_back(entry);
}
