#pragma once
#include "main.h"
#include <vector>

class GameObject;

// =================================================================
// ドロップシャドウ（接地丸影 / Blob Shadow）管理システム
// =================================================================
// 3D空間におけるキャラクター・敵・ボスの距離感と接地感を正確に
// 把握できるように、オブジェクト直下の床面に半透明の丸影を描画します。
// 各種サイズや濃さの設定は game_constants.h の Constants::Shadow に集約されています。
class ShadowSystem {
private:
    // GPU リソース
    static ID3D11Buffer*            m_QuadVB;       // 地面に水平なクアッド頂点バッファ
    static ID3D11VertexShader*      m_VS;           // 頂点シェーダー
    static ID3D11PixelShader*       m_PS;           // ピクセルシェーダー
    static ID3D11InputLayout*       m_IL;           // 入力レイアウト
    static ID3D11DepthStencilState*  m_DepthState;    // 深度ステート（テストON / 書き込みOFF）

public:
    static void Init();
    static void Uninit();

    // 指定されたゲームオブジェクトリストの直下に丸影を一括描画
    static void Draw(const std::vector<GameObject*>& objects);
};
