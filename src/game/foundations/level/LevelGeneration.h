#ifndef LEVEL_GENERATOR_H
#define LEVEL_GENERATOR_H

#include "core/assetsys.h"
#include "core/ecs.h"
#include <map>

// Dedicating 2 bits for each type of wall
enum WallType {
    OPEN = 0,
    DOOR = 1,
    WALL = 2
};

// Dedicating bits for NESW to combine into a mask
enum DoorDirection { 
    NORTH = 0, 
    EAST = 2, 
    SOUTH = 4,
    WEST = 6 
};

// Corresponds to unique mask, rotation and asset for each room possibility
struct Room {
    Asset* asset;
    uint16_t doorwayMask;
    float rotation;
    float weight;
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

    // Creates a minimum required mask, checking neighbours wall types to ensure it connects validly
    uint16_t RequiredMask(int x, int y);

    // Builds all possibilities from given assets
    void BuildPalette(const std::vector<Asset*>& roomAssets);

    // Updates the list of valid rooms for each room
    void UpdatePossibilities(std::vector<glm::ivec2> updateRooms);

    // Traverses from the start room to all connected rooms, and returns a list of the visited room coordinates
    std::vector<glm::ivec2> GetTraversableRooms(glm::ivec2 startRoom);

    // Fills a grid with rooms that connect properly
    void GenerateGrid(int width, int height, glm::ivec2 start, glm::ivec2 goal, uint16_t startDoorwayMask, int maxRooms);

    // Should place the entities into the Scene, need scene for instantiate prefab
    void InstantiateLevel(class Scene* scene);

private:
    int gridWidth;
    int gridHeight;

    std::vector<Room> palette;
    std::vector<GridCell> grid;

    const DoorDirection directions[4] = { NORTH, EAST, SOUTH, WEST };
    const glm::ivec2 neighbourOffsets[4] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };

    // Helpers
    uint16_t ScanDoorways(Asset* asset);
    uint16_t RotateMask(uint16_t mask, int steps);
    WallType GetWallType(uint16_t mask, DoorDirection direction);
    void SetWallType(uint16_t& mask, DoorDirection direction, WallType wallType);
};

#endif