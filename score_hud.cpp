#include "score_hud.h"
#include "renderer.h"
#include "game_rule.h"
#include "player.h"
#include "manager.h"
#include <vector>

// =================================================================
// 静的メンバ変数の実体定義
// =================================================================
ID3D11Buffer*            ScoreHUD::m_QuadVB     = nullptr;
ID3D11VertexShader*      ScoreHUD::m_VS         = nullptr;
ID3D11PixelShader*       ScoreHUD::m_PS         = nullptr;
ID3D11InputLayout*       ScoreHUD::m_IL         = nullptr;
ID3D11DepthStencilState* ScoreHUD::m_DepthState = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_Texture    = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_TitleTexture    = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_ClearTexture    = nullptr;
ID3D11ShaderResourceView* ScoreHUD::m_GameOverTexture = nullptr;

int   ScoreHUD::m_LastScore   = 0;
int   ScoreHUD::m_LastHP      = 5;
float ScoreHUD::m_ScaleEffect = 1.0f;

// ─────────────────────────────────────────────────────────────────
// GDI を使用してトータルスコアHUD用のテクスチャを生成する
// テキスト部分がアルファ（輝度）として抽出され、ui_ps.hlsl で発光描画される
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateHUDTexture(
    ID3D11Device* device,
    int score,
    int hp
)
{
    const int W = 512, H = 64;

    // ─── GDI DIB セクションの作成 ───
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;  // トップダウン形式（上から下）
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;  // BGRA 32bit
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    // 背景を黒（アルファ=0用）でクリア
    memset(bits, 0, W * H * 4);

    // ─── フォントの作成 ───
    HFONT hFont = CreateFontW(
        46, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Impact"
    );
    HGDIOBJ oldFont = SelectObject(hdc, hFont);

    // ─── テキストの描画 ───
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255)); // 白色で描画

    wchar_t buf[64];
    swprintf_s(buf, L"SCORE: %06d  LIFE: %d", score, hp);

    RECT rc = { 0, 0, W, H };
    DrawTextW(hdc, buf, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    GdiFlush();

    // ─── アルファ値への変換処理 ───
    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3); // 輝度をアルファにする
        rgba[i * 4 + 0] = 255; // R: 白（Emission で色を変えるため白）
        rgba[i * 4 + 1] = 255; // G
        rgba[i * 4 + 2] = 255; // B
        rgba[i * 4 + 3] = a;   // A: テキストの形
    }

    // ─── D3D11 テクスチャの生成 ───
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

    // GDI リソースの解放
    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFont);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してタイトル画面用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateTitleTexture(ID3D11Device* device)
{
    const int W = 512, H = 256;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    memset(bits, 0, W * H * 4);

    HFONT hFontTitle = CreateFontW(
        64, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Impact"
    );
    HFONT hFontDesc = CreateFontW(
        22, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
    );

    SetBkMode(hdc, TRANSPARENT);

    // 1. タイトルを描画
    HGDIOBJ oldFont = SelectObject(hdc, hFontTitle);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rcTitle = { 0, 20, W, 100 };
    DrawTextW(hdc, L"TOON SLASHER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

    // 2. 説明を描画
    SelectObject(hdc, hFontDesc);
    SetTextColor(hdc, RGB(200, 200, 200));
    RECT rcDesc = { 0, 120, W, 250 };
    DrawTextW(hdc, L"PRESS SPACE TO START\n\n[Controls]\nMove: WASD / Jump: Space\nGrab/Throw: Left Click / Spin: Right Click", -1, &rcDesc, DT_CENTER);

    GdiFlush();

    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3);
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = a;
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFontTitle);
    DeleteObject(hFontDesc);
    DeleteObject(hBmp);
    DeleteDC(hdc);

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
    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&td, &initData, &tex))) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }
    return srv;
}

// ─────────────────────────────────────────────────────────────────
// GDI を使用してリザルト画面用のテクスチャを生成する
// ─────────────────────────────────────────────────────────────────
ID3D11ShaderResourceView* ScoreHUD::CreateResultTexture(ID3D11Device* device, bool isClear, int score)
{
    const int W = 512, H = 256;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = W;
    bmi.bmiHeader.biHeight      = -H;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    BYTE* bits = nullptr;
    HDC hdc     = CreateCompatibleDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

    memset(bits, 0, W * H * 4);

    HFONT hFontResult = CreateFontW(
        64, 0, 0, 0, FW_HEAVY,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Impact"
    );
    HFONT hFontDesc = CreateFontW(
        24, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial"
    );

    SetBkMode(hdc, TRANSPARENT);

    // 1. タイトルを描画
    HGDIOBJ oldFont = SelectObject(hdc, hFontResult);
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rcTitle = { 0, 20, W, 100 };
    if (isClear) {
        DrawTextW(hdc, L"GAME CLEAR", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);
    } else {
        DrawTextW(hdc, L"GAME OVER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);
    }

    // 2. スコアとタイトル戻る案内を描画
    SelectObject(hdc, hFontDesc);
    SetTextColor(hdc, RGB(200, 200, 200));
    wchar_t buf[128];
    if (isClear) {
        swprintf_s(buf, L"FINAL SCORE: %06d\n\nPRESS ENTER TO RETURN TITLE", score);
    } else {
        swprintf_s(buf, L"PRESS ENTER TO RETURN TITLE");
    }
    RECT rcDesc = { 0, 130, W, 250 };
    DrawTextW(hdc, buf, -1, &rcDesc, DT_CENTER);

    GdiFlush();

    std::vector<BYTE> rgba(W * H * 4);
    for (int i = 0; i < W * H; i++) {
        BYTE b = bits[i * 4 + 0];
        BYTE g = bits[i * 4 + 1];
        BYTE r = bits[i * 4 + 2];
        BYTE a = (BYTE)(((int)r + (int)g + (int)b) / 3);
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = a;
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldBmp);
    DeleteObject(hFontResult);
    DeleteObject(hFontDesc);
    DeleteObject(hBmp);
    DeleteDC(hdc);

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
    ID3D11ShaderResourceView* srv = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&td, &initData, &tex))) {
        device->CreateShaderResourceView(tex, nullptr, &srv);
        tex->Release();
    }
    return srv;
}

// ─────────────────────────────────────────────────────────────────
// 初期化処理
// ─────────────────────────────────────────────────────────────────
bool ScoreHUD::Init(ID3D11Device* device)
{
    if (!device) return false;

    // ─── 描画用クアッド頂点バッファの作成 ───
    // 中心基準でスケールできるように、ローカル座標範囲 [-0.5, 0.5] で作成
    struct VERTEX_3D_LOCAL {
        XMFLOAT3 Position;
        XMFLOAT3 Normal;
        XMFLOAT4 Diffuse;
        XMFLOAT2 TexCoord;
        XMFLOAT3 Tangent;
    } v[4];

    // TL: 左上
    v[0].Position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
    v[0].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[0].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    v[0].Tangent  = XMFLOAT3(1, 0, 0);

    // TR: 右上
    v[1].Position = XMFLOAT3( 0.5f, -0.5f, 0.0f);
    v[1].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[1].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
    v[1].Tangent  = XMFLOAT3(1, 0, 0);

    // BL: 左下
    v[2].Position = XMFLOAT3(-0.5f,  0.5f, 0.0f);
    v[2].Normal   = XMFLOAT3(0.0f, 0.0f, -1.0f);
    v[2].Diffuse  = XMFLOAT4(1, 1, 1, 1);
    v[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
    v[2].Tangent  = XMFLOAT3(1, 0, 0);

    // BR: 右下
    v[3].Position = XMFLOAT3( 0.5f,  0.5f, 0.0f);
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

    // ─── シェーダーファイルの読み込み ───
    // リリースビルド時は Assets/shader/ サブフォルダから読み込む
#ifdef NDEBUG
    Renderer::CreateVertexShader(&m_VS, &m_IL, "Assets/shader/vertexShader.cso");
    Renderer::CreatePixelShader(&m_PS, "Assets/shader/ui_ps.cso");
#else
    Renderer::CreateVertexShader(&m_VS, &m_IL, "vertexShader.cso");
    Renderer::CreatePixelShader(&m_PS, "ui_ps.cso");
#endif

    if (!m_VS || !m_PS || !m_IL) {
        OutputDebugStringA("[ScoreHUD] シェーダーの読み込みに失敗しました\n");
        return false;
    }

    // ─── 深度ステートの作成（テストOFF / 書き込みOFF）───
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = FALSE; // 常に最前面に描画
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device->CreateDepthStencilState(&dsd, &m_DepthState))) return false;

    // ─── 初期テクスチャ（スコア0、HP5）の生成 ───
    m_LastScore = GameRule::GetScore();
    m_LastHP = 5;
    Player* player = Manager::GetGameObject<Player>();
    if (player) {
        m_LastHP = player->GetHP();
    }
    m_Texture = CreateHUDTexture(device, m_LastScore, m_LastHP);
    m_ScaleEffect = 1.0f;

    OutputDebugStringA("[ScoreHUD] 初期化が正常に完了しました\n");
    return true;
}

// ─────────────────────────────────────────────────────────────────
// 終了処理
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Uninit()
{
    if (m_Texture)    { m_Texture->Release();    m_Texture    = nullptr; }
    if (m_TitleTexture) { m_TitleTexture->Release(); m_TitleTexture = nullptr; }
    if (m_ClearTexture) { m_ClearTexture->Release(); m_ClearTexture = nullptr; }
    if (m_GameOverTexture) { m_GameOverTexture->Release(); m_GameOverTexture = nullptr; }
    if (m_DepthState) { m_DepthState->Release(); m_DepthState = nullptr; }
    if (m_IL)         { m_IL->Release();         m_IL         = nullptr; }
    if (m_PS)         { m_PS->Release();         m_PS         = nullptr; }
    if (m_VS)         { m_VS->Release();         m_VS         = nullptr; }
    if (m_QuadVB)     { m_QuadVB->Release();     m_QuadVB     = nullptr; }
}

// ─────────────────────────────────────────────────────────────────
// 更新処理
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Update()
{
    Scene currentScene = Manager::GetCurrentScene();

    if (currentScene == Scene::TITLE) {
        if (!m_TitleTexture) {
            m_TitleTexture = CreateTitleTexture(Renderer::GetDevice());
        }
    } else if (currentScene == Scene::GAMEPLAY) {
        // スコアとHP変化の監視
        int currentScore = GameRule::GetScore();
        int currentHP = m_LastHP;
        Player* player = Manager::GetGameObject<Player>();
        if (player) {
            currentHP = player->GetHP();
        }

        if (currentScore != m_LastScore || currentHP != m_LastHP || !m_Texture) {
            // スコアまたはHPが変わったらテクスチャを再構築
            if (m_Texture) {
                m_Texture->Release();
                m_Texture = nullptr;
            }
            m_Texture = CreateHUDTexture(Renderer::GetDevice(), currentScore, currentHP);
            m_LastScore = currentScore;
            m_LastHP = currentHP;

            // スケールポップ効果を発動（少し大きくなる）
            m_ScaleEffect = 1.25f;
        }

        // スケールを徐々に 1.0 に戻す（イージング）
        if (m_ScaleEffect > 1.0f) {
            m_ScaleEffect += (1.0f - m_ScaleEffect) * 0.15f;
            if (m_ScaleEffect - 1.0f < 0.005f) {
                m_ScaleEffect = 1.0f;
            }
        }
    } else if (currentScene == Scene::CLEAR) {
        if (!m_ClearTexture) {
            m_ClearTexture = CreateResultTexture(Renderer::GetDevice(), true, GameRule::GetScore());
        }
    } else if (currentScene == Scene::GAMEOVER) {
        if (!m_GameOverTexture) {
            m_GameOverTexture = CreateResultTexture(Renderer::GetDevice(), false, GameRule::GetScore());
        }
    }

    // 不要なシーンテクスチャを解放
    if (currentScene != Scene::TITLE && m_TitleTexture) {
        m_TitleTexture->Release();
        m_TitleTexture = nullptr;
    }
    if (currentScene != Scene::CLEAR && m_ClearTexture) {
        m_ClearTexture->Release();
        m_ClearTexture = nullptr;
    }
    if (currentScene != Scene::GAMEOVER && m_GameOverTexture) {
        m_GameOverTexture->Release();
        m_GameOverTexture = nullptr;
    }
    if (currentScene != Scene::GAMEPLAY && m_Texture) {
        m_Texture->Release();
        m_Texture = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────
// 描画処理（Renderer::End() の直前に呼び出す）
// ─────────────────────────────────────────────────────────────────
void ScoreHUD::Draw()
{
    ID3D11ShaderResourceView* texToDraw = nullptr;
    Scene currentScene = Manager::GetCurrentScene();

    float w = 512.0f;
    float h = 64.0f;
    float posX = (float)SCREEN_WIDTH * 0.5f; // 画面中央
    float posY = 45.0f;                      // 画面上部から少し下げた位置
    XMFLOAT4 emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // スコアHUDは黄金色の発光

    switch (currentScene) {
    case Scene::TITLE:
        texToDraw = m_TitleTexture;
        w = 512.0f;
        h = 256.0f;
        posY = (float)SCREEN_HEIGHT * 0.5f; // 画面中央
        emissionColor = XMFLOAT4(0.0f, 1.2f, 1.8f, 0.0f); // タイトルは青白発光
        break;
    case Scene::GAMEPLAY:
        texToDraw = m_Texture;
        w = 512.0f * m_ScaleEffect;
        h = 64.0f * m_ScaleEffect;
        posY = 45.0f;
        break;
    case Scene::CLEAR:
        texToDraw = m_ClearTexture;
        w = 512.0f;
        h = 256.0f;
        posY = (float)SCREEN_HEIGHT * 0.5f;
        emissionColor = XMFLOAT4(1.5f, 1.2f, 0.0f, 0.0f); // クリアは黄金発光
        break;
    case Scene::GAMEOVER:
        texToDraw = m_GameOverTexture;
        w = 512.0f;
        h = 256.0f;
        posY = (float)SCREEN_HEIGHT * 0.5f;
        emissionColor = XMFLOAT4(1.8f, 0.0f, 0.0f, 0.0f); // ゲームオーバーは赤発光
        break;
    }

    if (!texToDraw || !m_QuadVB || !m_VS || !m_PS || !m_IL || !m_DepthState) return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

    // ─── レンダーステートの設定 ───
    ctx->OMSetDepthStencilState(m_DepthState, 0);
    ctx->VSSetShader(m_VS, nullptr, 0);
    ctx->PSSetShader(m_PS, nullptr, 0);
    ctx->IASetInputLayout(m_IL);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    UINT stride = 60u; // VERTEX_3D のストライドサイズ
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);

    // ─── 2D正射影行列の設定 ───
    // カメラのビュー・プロジェクション行列を一時保存
    XMMATRIX oldView = Renderer::GetViewMatrix();
    XMMATRIX oldProj = Renderer::GetProjectionMatrix();

    // 2D正射影用のビュー/プロジェクション行列を設定
    XMMATRIX view2D = XMMatrixIdentity();
    XMMATRIX proj2D = XMMatrixOrthographicOffCenterLH(0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f);
    Renderer::SetViewMatrix(view2D);
    Renderer::SetProjectionMatrix(proj2D);

    // ─── ワールド行列の算出 ───
    XMMATRIX world = XMMatrixScaling(w, h, 1.0f) * XMMatrixTranslation(posX, posY, 0.0f);
    Renderer::SetWorldMatrix(world);

    // ─── マテリアル・テクスチャの設定 ───
    MATERIAL mat = {};
    mat.Diffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    mat.Emission = emissionColor;
    mat.TextureEnable = FALSE;
    Renderer::SetMaterial(mat);

    Renderer::SetTexture(texToDraw);

    // ─── 描画 ───
    ctx->Draw(4, 0);

    // ─── 後処理（行列とトポロジーを元に戻す）───
    Renderer::SetViewMatrix(oldView);
    Renderer::SetProjectionMatrix(oldProj);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
