#include "gameover_scene.h"
#include "input.h"
#include "manager.h"
#include "sound_manager.h"

GameOverScene::GameOverScene() {}
GameOverScene::~GameOverScene() {}

void GameOverScene::Init() {}
void GameOverScene::Uninit() {}

void GameOverScene::Update()
{
    // エンターキー、スペースキー、マウスクリックでタイトル画面に戻る
    if (Input::GetKeyTrigger(VK_RETURN) || Input::GetKeyTrigger(VK_SPACE) || Input::GetKeyTrigger(VK_LBUTTON)) {
        SoundManager::PlaySE(SoundID::SE_DECIDE);
        Manager::ChangeScene(Scene::TITLE);
    }
}

void GameOverScene::Draw() {}
