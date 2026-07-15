#pragma once
#include <vector>
#include <directxmath.h>
#include "game_constants.h"

class Enemy;

class SpatialGrid {
public:
    void Clear() {
        for (int r = 0; r < Constants::Collision::GRID_ROWS; ++r) {
            for (int c = 0; c < Constants::Collision::GRID_COLS; ++c) {
                m_Cells[r][c].clear();
            }
        }
    }

    void Register(Enemy* enemy);

    const std::vector<Enemy*>& GetCell(int row, int col) const {
        static const std::vector<Enemy*> emptyCell;
        if (row >= 0 && row < Constants::Collision::GRID_ROWS && col >= 0 && col < Constants::Collision::GRID_COLS) {
            return m_Cells[row][col];
        }
        return emptyCell;
    }

    void FindNearbyEnemies(const DirectX::XMFLOAT3& centerPos, float radius, std::vector<Enemy*>& outEnemies) const;

    static SpatialGrid& GetInstance() {
        static SpatialGrid instance;
        return instance;
    }

private:
    std::vector<Enemy*> m_Cells[Constants::Collision::GRID_ROWS][Constants::Collision::GRID_COLS];
};
