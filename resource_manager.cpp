#include "resource_manager.h"
#include "renderer.h"

// 静的メンバ変数の実体定義
std::unordered_map<std::string, ID3D11ShaderResourceView*> ResourceManager::m_Textures;

ID3D11ShaderResourceView* ResourceManager::GetTexture(std::string_view fileName) {
    if (fileName.empty()) {
        return nullptr;
    }

    std::string fileStr(fileName);

    // 既に読み込み済みか検索
    auto it = m_Textures.find(fileStr);
    if (it != m_Textures.end()) {
        return it->second;
    }

    // パス解決（Debug / Release 共に Assets/texture/ を最優先に検索）
    std::string path = fileStr;
    if (fileStr.rfind("Assets/texture/", 0) != 0) {
        // "player.png" 等のプレーン名で呼ばれた場合は Assets/texture/player.png を試す
        path = "Assets/texture/" + fileStr;
    }

    // 新規ロード
    ID3D11ShaderResourceView* texture = nullptr;
    Renderer::CreateTexture(path.c_str(), &texture);
    if (texture) {
        m_Textures[fileStr] = texture;
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
