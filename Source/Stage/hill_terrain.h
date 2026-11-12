#pragma once
#include "main.h"

// =================================================================
// 遠景の丘と木（静的クラス）
// =================================================================
// 地面(±50m)の外側に、手続き生成の丘のメッシュと低ポリの木を置く。
// 起伏と木の層が視差で動くので、空に描いた山より奥行きが出る。
// シーン側が SetEnabled(true) にすると描画され、Clear() で無効に戻る。
// メッシュは初めて有効にした時に一度だけ生成する。
class HillTerrain {
public:
    static void Init();
    static void Uninit();
    static void Clear();                 // シーン遷移時: 無効化
    static void SetEnabled(bool enable);
    static void Draw();
};
