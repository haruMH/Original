#include "spatial_grid.h"
#include "enemy.h"
#include "math_helper.h"
#include <algorithm>

using namespace DirectX;

void SpatialGrid::Register(Enemy* enemy)
{
    if (!enemy) return;
    XMFLOAT3 pos = enemy->GetPosition();
    int col = static_cast<int>(floorf((pos.x - Constants::Collision::GRID_MIN_X) / Constants::Collision::GRID_CELL_SIZE));
    int row = static_cast<int>(floorf((pos.z - Constants::Collision::GRID_MIN_Z) / Constants::Collision::GRID_CELL_SIZE));

    col = (std::max)(0, (std::min)(col, Constants::Collision::GRID_COLS - 1));
    row = (std::max)(0, (std::min)(row, Constants::Collision::GRID_ROWS - 1));

    m_Cells[row][col].push_back(enemy);
}

void SpatialGrid::FindNearbyEnemies(const XMFLOAT3& centerPos, float radius, std::vector<Enemy*>& outEnemies) const
{
    int centerCol = static_cast<int>(floorf((centerPos.x - Constants::Collision::GRID_MIN_X) / Constants::Collision::GRID_CELL_SIZE));
    int centerRow = static_cast<int>(floorf((centerPos.z - Constants::Collision::GRID_MIN_Z) / Constants::Collision::GRID_CELL_SIZE));

    int cellRange = static_cast<int>(ceilf(radius / Constants::Collision::GRID_CELL_SIZE));
    float radiusSq = radius * radius;

    for (int r = centerRow - cellRange; r <= centerRow + cellRange; ++r) {
        for (int c = centerCol - cellRange; c <= centerCol + cellRange; ++c) {
            if (r >= 0 && r < Constants::Collision::GRID_ROWS && c >= 0 && c < Constants::Collision::GRID_COLS) {
                for (Enemy* enemy : m_Cells[r][c]) {
                    if (!enemy || enemy->IsDestroy() || enemy->GetEnemyState() == EnemyState::DEFEATED) continue;

                    XMFLOAT3 diff = enemy->GetPosition() - centerPos;
                    float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                    if (distSq <= radiusSq) {
                        outEnemies.push_back(enemy);
                    }
                }
            }
        }
    }
}
