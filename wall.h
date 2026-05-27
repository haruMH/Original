#pragma once
#include "gameobject.h"

class Wall : public GameObject
{
private:
    ID3D11ShaderResourceView* m_Texture = nullptr;

public:
    void Init()   override;
    void Uninit() override;
    void Update() override;
    void Draw()   override;
};
