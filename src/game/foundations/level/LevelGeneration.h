#ifndef LEVEL_GENERATOR_H
#define LEVEL_GENERATOR_H

#include "core/assetsys.h"
#include "core/ecs.h"
#include <map>
#include <random>

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
    Theme theme;
};

// Stores current state of a cell in the level grid
struct GridCell {
    std::vector<int> validRooms;
    std::vector<int> validThemes;
    bool isCollapsed = false;
};

// Stores info about one floor of the game
struct LevelFloor {
    int floor = 0;
    glm::vec3 worldOffset = { 0,0,0 };

    std::vector<Room> palette;
    std::vector<GridCell> grid;
    int width, height;

    glm::ivec2 startCoords;
    glm::ivec2 goalCoords;

    std::vector<EntityID> roomEntities;
};

class LevelGeneration {
public:
    // Checks if a room is a valid neighbour in a specific direction
    bool CanNeighbour(LevelFloor& floor, int roomAIndex, int roomBIndex, DoorDirection direction);

    // Creates a minimum required mask, checking neighbours wall types to ensure it connects validly
    uint16_t RequiredMask(LevelFloor& floor, int x, int y);

    // Checks connected neighbours themes, selects a theme from that list at random
    Theme GetPreferredTheme(LevelFloor& floor, glm::ivec2 room, std::mt19937 rng);

    // Builds all possibilities from given assets
    void BuildPalette(const std::vector<Asset*>& roomAssets);

    // Updates the list of valid rooms for each room
    void UpdatePossibilities(LevelFloor& floor, std::vector<glm::ivec2> updateRooms);

    // Traverses from the start room to all connected rooms, and returns a list of the visited room coordinates
    std::vector<glm::ivec2> GetTraversableRooms(LevelFloor& floor, glm::ivec2 startRoom);

    // Fills a grid with rooms that connect properly
    LevelFloor GenerateGrid(int width, int height, glm::ivec2 start, uint16_t startDoorwayMask, Theme startTheme, glm::ivec2 goal, uint16_t goalDoorwayMask, Theme goalTheme, int maxRooms, int floorNum);

    // Should place the entities into the Scene, need scene for instantiate prefab
    void InstantiateLevel(class Scene* scene, LevelFloor& floor);

    void GenerateNextFloor(Scene* scene);
    void CleanupOldFloors(Scene* scene);

    std::vector<LevelFloor> activeFloors;
    int currentFloor;

private:
    std::vector<Room> palette;

    const DoorDirection directions[4] = { NORTH, EAST, SOUTH, WEST };
    const glm::ivec2 neighbourOffsets[4] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };

    // Helpers
    uint16_t ScanDoorways(Asset* asset, Theme& theme);
    uint16_t RotateMask(uint16_t mask, int steps);
    WallType GetWallType(uint16_t mask, DoorDirection direction);
    void SetWallType(uint16_t& mask, DoorDirection direction, WallType wallType);
};

#endif