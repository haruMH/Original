#include "clear_scene.h"
#include "input.h"
#include "manager.h"

ClearScene::ClearScene() {}
ClearScene::~ClearScene() {}

void ClearScene::Init() {}
void ClearScene::Uninit() {}

void ClearScene::Update()
{
    // エンターキーが押されたらタイトル画面に戻る
    if (Input::GetKeyTrigger(VK_RETURN)) {
        Manager::ChangeScene(Scene::TITLE);
    }
}

void ClearScene::Draw() {}
