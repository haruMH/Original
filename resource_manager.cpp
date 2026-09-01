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

    // パス解決
    std::string path = fileStr;
#ifdef NDEBUG
    // リリースビルド時は、Assets/texture/ から始まっていない場合のみ付与する
    if (fileStr.rfind("Assets/texture/", 0) != 0) {
        path = "Assets/texture/" + fileStr;
    }
#else
    // デバッグビルド時は、もし Assets/texture/ から始まっていたらそれを取り除いてプレーン名にする
    if (fileStr.rfind("Assets/texture/", 0) == 0) {
        path = fileStr.substr(15); // "Assets/texture/" の長さは 15
    }
#endif

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
