#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <list>
#include <d3d11.h>
#include <DirectXMath.h>
#include "render_component.h"

// 前方宣言
class GameObject;

enum class RenderPass {
    Normal,
    Shadow,
    Outline
};

// =================================================================
// RenderSystem クラス
// =================================================================
// インスタンス描画を管理し、複数のオブジェクトを一括で描画するシステムクラスです。
class RenderSystem {
public:
    // 1回の描画コールで処理できる最大インスタンス数
    static const int MAX_INSTANCES = 512;

    // インスタンスバッファに流し込むデータ構造（頂点シェーダーへ送る情報）
    struct InstanceData {
        DirectX::XMFLOAT4X4 World;      // ワールド行列
        float               TextureIndex; // テクスチャ配列インデックス
        float               Padding[3];   // 16バイトアライメントのためのパディング
    };

    RenderSystem();
    ~RenderSystem();

    // システムの初期化（インスタンスバッファやシェーダーの生成）
    bool Init(ID3D11Device* device);

    // システムのクリーンアップ（GPUリソースの解放）
    void Uninit();

    // キューブ形状を持つ GameObject 群をテクスチャごとにグループ化して一括描画する
    // context: デバイスコンテキスト
    // objects: シーン内の全 GameObject のリスト
    // pass: 描画パス（通常、シャドウ、アウトライン）
    void RenderCubeInstances(ID3D11DeviceContext* context, const std::list<GameObject*>& objects, RenderPass pass = RenderPass::Normal);

private:
    ID3D11Buffer*       m_InstanceBuffer = nullptr;       // 動的インスタンスバッファ（スロット1用）
    ID3D11InputLayout*  m_InstancedInputLayout = nullptr;  // インスタンス描画専用インプットレイアウト
    ID3D11VertexShader* m_InstancedVertexShader = nullptr;  // インスタンス描画専用頂点シェーダー
    ID3D11PixelShader*  m_InstancedPixelShader = nullptr;   // インスタンス描画専用ピクセルシェーダー

    ID3D11InputLayout*  m_InstancedOutlineInputLayout = nullptr; // インスタンスアウトライン専用インプットレイアウト
    ID3D11VertexShader* m_InstancedOutlineVertexShader = nullptr; // インスタンスアウトライン専用頂点シェーダー

    ID3D11ShaderResourceView* m_TextureArraySRV = nullptr;       // テクスチャ配列用SRV
    std::unordered_map<std::string, float> m_TextureIndexMap;    // テクスチャ名から配列インデックスへのマップ

    // シェーダーおよびレイアウトのリソース作成
    bool CreateResources(ID3D11Device* device);

    // テクスチャ配列の生成
    bool CreateTextureArray(ID3D11Device* device);
};
