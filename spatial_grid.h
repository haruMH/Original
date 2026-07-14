#pragma once
#include <vector>
#include <directxmath.h>
#include "game_constants.h"

class Enemy;

class SpatialGrid {
public:
    static constexpr float CELL_SIZE = Constants::Collision::GRID_CELL_SIZE;
    static constexpr int GRID_COLS = Constants::Collision::GRID_COLS;
    static constexpr int GRID_ROWS = Constants::Collision::GRID_ROWS;
    static constexpr float GRID_MIN_X = Constants::Collision::GRID_MIN_X;
    static constexpr float GRID_MIN_Z = Constants::Collision::GRID_MIN_Z;

    void Clear() {
        for (int r = 0; r < GRID_ROWS; ++r) {
            for (int c = 0; c < GRID_COLS; ++c) {
                m_Cells[r][c].clear();
            }
        }
    }

    void Register(Enemy* enemy);

    const std::vector<Enemy*>& GetCell(int row, int col) const {
        static const std::vector<Enemy*> emptyCell;
        if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
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
    std::vector<Enemy*> m_Cells[GRID_ROWS][GRID_COLS];
};
