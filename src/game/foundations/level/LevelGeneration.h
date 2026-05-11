#ifndef LEVEL_GENERATOR_H
#define LEVEL_GENERATOR_H

#include "core/assetsys.h"
#include "core/ecs.h"
#include <map>
#include <random>
#include <filesystem>

// Enum of room themes, add to this when making new theme
enum Theme {
    OUTSIDE = 0,
    INSIDE = 1,
    THEME_COUNT = 2
};

// Dedicating 2 bits for each type of wall
enum WallType {
    OPEN = 0,
    DOOR = 1,
    WALL = 2
};

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

    // Builds all possibilities from given assets
    void BuildPalette(const std::vector<Asset*>& roomAssets);

    // Updates the list of valid rooms for each room
    void UpdatePossibilities(std::vector<glm::ivec2> updateRooms);

    // Finds all the coordinates of the rooms visitable from the starting room
    std::vector<glm::ivec2> GetTraversableRooms(glm::ivec2 startRoom);

    // Fills a grid with rooms that connect properly
    void GenerateGrid(int width, int height, glm::ivec2 start, glm::ivec2 goal, uint8_t startDoorwayMask);

    // Should place the entities into the Scene, need scene for instantiate prefab
    void InstantiateLevel(class Scene* scene, LevelFloor& floor);

    // Does all the handywork to create a single floor clearly from all assets in a folder
    LevelFloor LevelGeneration::CreateFullLevel(Scene* scene, const std::string& folder);

    void GenerateNextFloor(Scene* scene);
    void CleanupOldFloors(Scene* scene);

    std::vector<LevelFloor> activeFloors;
    int currentFloor;

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