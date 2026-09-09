#include "clear_scene.h"
#include "input.h"
#include "manager.h"
#include "sound_manager.h"

ClearScene::ClearScene() {}
ClearScene::~ClearScene() {}

void ClearScene::Init() {}
void ClearScene::Uninit() {}

void ClearScene::Update()
{
    // エンターキー、スペースキー、マウスクリックでタイトル画面に戻る
    if (Input::GetKeyTrigger(VK_RETURN) || Input::GetKeyTrigger(VK_SPACE) || Input::GetKeyTrigger(VK_LBUTTON)) {
        SoundManager::PlaySE(SoundID::SE_DECIDE);
        Manager::ChangeScene(Scene::TITLE);
    }
}

void ClearScene::Draw() {}
