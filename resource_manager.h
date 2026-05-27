#pragma once
#include <string>
#include <unordered_map>
#include <d3d11.h>

// テクスチャなどのゲームリソースを一元管理・共有する静的クラス
class ResourceManager {
private:
    // 読み込み済みのテクスチャを管理するマップ
    static std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Textures;

public:
    // 指定されたファイル名のテクスチャを取得（未ロードの場合はロードしてキャッシュする）
    static ID3D11ShaderResourceView* GetTexture(const std::string& fileName);
    
    // 全てのリソースを解放
    static void Uninit();
};
