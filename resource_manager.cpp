#include "resource_manager.h"
#include "renderer.h"

// 静的メンバ変数の実体定義
std::unordered_map<std::string, ID3D11ShaderResourceView*> ResourceManager::m_Textures;

ID3D11ShaderResourceView* ResourceManager::GetTexture(const std::string& fileName) {
    if (fileName.empty()) {
        return nullptr;
    }

    // 既に読み込み済みか検索
    auto it = m_Textures.find(fileName);
    if (it != m_Textures.end()) {
        return it->second;
    }

    // 新規ロード
    ID3D11ShaderResourceView* texture = nullptr;
    Renderer::CreateTexture(fileName.c_str(), &texture);
    if (texture) {
        m_Textures[fileName] = texture;
    }
    return texture;
}

void ResourceManager::Uninit() {
    // 全てのテクスチャリソースをリリース
    for (auto& pair : m_Textures) {
        if (pair.second) {
            pair.second->Release();
        }
    }
    m_Textures.clear();
}
