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

    // パス解決
    std::string path = fileName;
#ifdef NDEBUG
    // リリースビルド時は、Assets/texture/ から始まっていない場合のみ付与する
    if (fileName.rfind("Assets/texture/", 0) != 0) {
        path = "Assets/texture/" + fileName;
    }
#else
    // デバッグビルド時は、もし Assets/texture/ から始まっていたらそれを取り除いてプレーン名にする
    if (fileName.rfind("Assets/texture/", 0) == 0) {
        path = fileName.substr(15); // "Assets/texture/" の長さは 15
    }
#endif

    // 新規ロード
    ID3D11ShaderResourceView* texture = nullptr;
    Renderer::CreateTexture(path.c_str(), &texture);
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
