#pragma once
#include <unordered_map>
#include <vector>

class SpatialGrid {
private:
    struct GridKey {
        int x, y, z;

        bool operator==(const GridKey& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct GridKeyHash {
        size_t operator()(const GridKey& k) const {
            return ((size_t)k.x * 73856093) ^
                ((size_t)k.y * 19349663) ^
                ((size_t)k.z * 83492791);
        }
    };

    float m_cellSize;
    std::unordered_map<GridKey, std::vector<uint32_t>, GridKeyHash> m_cells;

    GridKey getKey(float x, float y, float z) const {
        return {
            (int)floorf(x / m_cellSize),
            (int)floorf(y / m_cellSize),
            (int)floorf(z / m_cellSize)
        };
    }

public:
    SpatialGrid(float cellSize) : m_cellSize(cellSize) {}

    void clear() {
        m_cells.clear();
    }

    void insert(uint32_t objectIndex, float x, float y, float z) {
        GridKey key = getKey(x, y, z);
        m_cells[key].push_back(objectIndex);
    }

    void queryNeighbors(float x, float y, float z, std::vector<uint32_t>& results) {
        results.clear();
        GridKey center = getKey(x, y, z);

        // Check 3x3x3 cube of cells around the object
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    GridKey key = { center.x + dx, center.y + dy, center.z + dz };
                    auto it = m_cells.find(key);
                    if (it != m_cells.end()) {
                        const auto& objList = m_cells[key];
						// this is faster than results.insert(results.end(), objList.begin(), objList.end());
						for (int objIdx : objList) {
                            results.push_back(objIdx);
                        }
                    }
                }
            }
        }
    }
};
