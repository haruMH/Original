#include "gameover_scene.h"
#include "input.h"
#include "manager.h"

GameOverScene::GameOverScene() {}
GameOverScene::~GameOverScene() {}

void GameOverScene::Init() {}
void GameOverScene::Uninit() {}

void GameOverScene::Update()
{
    // エンターキーが押されたらタイトル画面に戻る
    if (Input::GetKeyTrigger(VK_RETURN)) {
        Manager::ChangeScene(Scene::TITLE);
    }
}

void GameOverScene::Draw() {}
