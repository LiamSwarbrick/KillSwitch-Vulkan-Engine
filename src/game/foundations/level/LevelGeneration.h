#ifndef LEVEL_GENERATOR_H
#define LEVEL_GENERATOR_H

#include "core/assetsys.h"
#include "core/ecs.h"
#include <map>

// Dedicating bits for NESW to combine into a mask
enum DoorDirection { 
    NORTH = 1, 
    EAST = 2, 
    SOUTH = 4,
    WEST = 8 
};

// Corresponds to unique mask, rotation and asset for each room possibility
struct Room {
    Asset* asset;
    uint8_t doorwayMask;
    float rotation;
};

// Stores current state of a cell in the level grid
struct GridCell {
    std::vector<int> validRooms;
    bool isCollapsed = false;
};

class LevelGeneration {
public:
    // Checks if a room is a valid neighbour in a specific direction
    bool CanNeighbour(int roomAIndex, int roomBIndex, DoorDirection direction);

    // Builds all possibilities from given assets
    void BuildPalette(const std::vector<Asset*>& roomAssets);

    // Fills a grid with rooms that connect properly
    void GenerateGrid(int width, int height);

    // Should place the entities into the Scene, need scene for instantiate prefab
    void InstantiateLevel(class Scene* scene);

private:
    int gridWidth;
    int gridHeight;

    std::vector<Room> palette;
    std::vector<GridCell> grid;

    // Helpers
    uint8_t ScanDoorways(Asset* asset);
    uint8_t RotateMask(uint8_t mask, int steps);
};

#endif