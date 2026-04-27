#include "LevelGeneration.h"
#include "../scene.h"
#include <cstring>
#include <SDL3/SDL_log.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <core/components.h>
#include <random>

/*
TO ENSURE THIS WORKS CORRECTLY, EMPTY DOORWAY NODES SHOULD PLACED AND NAMED AS FOLLOWS:
	NORTH DOORWAY: "Doorway_N"
	EAST DOORWAY: "Doorway_E"
	SOUTH DOORWAY: "Doorway_S"
	WEST DOORWAY: "Doorway_W"
	NORTH WALL: "Wall_N"
	EAST WALL: "Wall_E"
	SOUTH WALL: "Wall_S"
	WEST WALL: "Wall_W"
*/

static std::mt19937 rng(std::random_device{}());

// Just literally checks if each room has the necessary wall type to connect in the direction specified
bool LevelGeneration::CanNeighbour(int roomAIndex, int roomBIndex, DoorDirection direction)
{
	const Room& roomA = palette[roomAIndex];
	const Room& roomB = palette[roomBIndex];

	WallType roomAWallType = GetWallType(roomA.doorwayMask, direction);
	WallType roomBWallType = GetWallType(roomB.doorwayMask, (DoorDirection)(((int)direction + 4) % 8));

	return roomAWallType == roomBWallType;
}

uint16_t LevelGeneration::RequiredMask(int x, int y)
{
	// Set default to be all walls
	uint16_t mask = 170;

	// In each direction, get the neighbours wall type and set required mask to match
	for (int i = 0; i < 4; ++i)
	{
		glm::ivec2 neighbourCoords = { x + neighbourOffsets[i].x, y + neighbourOffsets[i].y };

		if (neighbourCoords.x < 0 || neighbourCoords.x >= gridWidth || neighbourCoords.y < 0 || neighbourCoords.y >= gridHeight)
			continue;

		GridCell& neighbourRoom = grid[neighbourCoords.y * gridWidth + neighbourCoords.x];

		if (!neighbourRoom.isCollapsed)
			continue;

		WallType neighbourWallType = GetWallType(palette[neighbourRoom.validRooms[0]].doorwayMask, (DoorDirection)(((int)directions[i] + 4) % 8));
		SetWallType(mask, directions[i], neighbourWallType);
	}

	return mask;
}

// Scans vector of room assets to fill a palette of room variations
void LevelGeneration::BuildPalette(const std::vector<Asset*>& roomAssets)
{
	palette.clear();

	// Scan every room asset and store all non-duplicate rotations of it
	for (Asset* asset : roomAssets)
	{
		uint16_t defaultMask = ScanDoorways(asset);

		// For every room rotation, check if its a dupe, weight it, then push it to the palette
		for (int i = 0; i < 4; ++i)
		{
			uint16_t rotatedMask = RotateMask(defaultMask, i);
			float rotation = i * -90.0f;

			bool roomDuplicate = false;
			for (const auto& room : palette)
			{
				if (room.asset == asset && room.doorwayMask == rotatedMask)
				{
					roomDuplicate = true;
					break;
				}
			}

			if (!roomDuplicate)
			{
				int doors = 0;
				int opens = 0;
				int walls = 0;

				// Count wall types
				for (int j = 0; j < 4; ++j)
				{
					WallType wallType = GetWallType(rotatedMask, directions[j]);
					if (wallType == DOOR)
						doors++;
					else if (wallType == OPEN)
						opens++;
					else
						walls++;
				}

				//                             CHANGE ROOM WEIGHTINGS HERE !!!!
				float weight = 1.0f;

				switch (opens)
				{
				case 4:
					weight *= 0.1f;
					break;
				case 3:
					weight *= 0.4f;
					break;
				case 2:
					weight *= 0.6f;
					break;
				case 1:
					weight *= 1.0f;
					break;
				}

				switch (doors)
				{
				case 4:
					weight *= 0.5f;
					break;
				case 3:
					weight *= 2.0f;
					break;
				case 2:
					weight *= 3.0f;
					break;
				case 1:
					weight *= 0.5f;
					break;
				}

				palette.push_back({ asset, rotatedMask, rotation, weight });
			}
		}
	}
}

// Updates all rooms valid room lists if they have been affected by recent collapses
void LevelGeneration::UpdatePossibilities(std::vector<glm::ivec2> updateRooms)
{
	while (!updateRooms.empty())
	{
		glm::ivec2 currentRoom = updateRooms.back();
		updateRooms.pop_back();

		for (int i = 0; i < 4; ++i)
		{
			// Get neighbouring room and check if valid within level boundaries
			glm::ivec2 neighbourRoom = { currentRoom.x + neighbourOffsets[i].x, currentRoom.y + neighbourOffsets[i].y };
			if (neighbourRoom.x < 0 || neighbourRoom.x >= gridWidth || neighbourRoom.y < 0 || neighbourRoom.y >= gridHeight)
				continue;

			// Find neighbour gridcell and check if its already collapsed
			int currentGridIndex = currentRoom.y * gridWidth + currentRoom.x;
			GridCell& current = grid[currentGridIndex];
			int neighbourGridIndex = neighbourRoom.y * gridWidth + neighbourRoom.x;
			GridCell& neighbour = grid[neighbourGridIndex];
			if (neighbour.isCollapsed)
				continue;

			// Check all combinations of current and neighbouring rooms
			std::vector<int> updatedValidRooms;
			for (int neighbourValidRoom : neighbour.validRooms)
			{
				bool compatible = false;
				for (int currentValidRoom : current.validRooms)
				{
					if (CanNeighbour(currentValidRoom, neighbourValidRoom, directions[i]))
					{
						compatible = true;
						break;
					}
				}

				// If the rooms could connect, add to valid rooms
				if (compatible)
					updatedValidRooms.push_back(neighbourValidRoom);
			}

			// If neighbours valid rooms changed update them, then need to update their neighbours valid rooms too
			if (updatedValidRooms.size() < neighbour.validRooms.size())
			{
				neighbour.validRooms = updatedValidRooms;
				updateRooms.push_back({ neighbourRoom.x, neighbourRoom.y });
			}
		}
	}
}

// Generates and instantiates a level
void LevelGeneration::GenerateGrid(int width, int height, glm::ivec2 start, glm::ivec2 goal, uint16_t startDoorwayMask, int maxRooms)
{
	while (true)
	{
		gridWidth = width;
		gridHeight = height;
		grid.assign(width * height, GridCell());

		// Give every grid cell every room as possible neighbours
		std::vector<int> allRooms;
		for (int i = 0; i < palette.size(); ++i)
			allRooms.push_back(i);

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				int gridIndex = y * width + x;
				GridCell& cell = grid[gridIndex];
				cell.validRooms = allRooms;

				// For rooms on the edge, remove possibility for doorway or open space into the edge
				auto& p = cell.validRooms;
				p.erase(std::remove_if(p.begin(), p.end(), [&](int index)
					{
						uint16_t mask = palette[index].doorwayMask;
						if (y == height - 1 && GetWallType(mask, NORTH) != WALL)
							return true;
						if (y == 0 && GetWallType(mask, SOUTH) != WALL)
							return true;
						if (x == width - 1 && GetWallType(mask, EAST) != WALL)
							return true;
						if (x == 0 && GetWallType(mask, WEST) != WALL)
							return true;
						return false;
					}
				), p.end());
			}
		}

		// Assign starting cell its room and update collapsed state
		int gridIndex = start.y * width + start.x;
		GridCell& startingCell = grid[gridIndex];
		int roomChoice = -1;

		for (int i = 0; i < palette.size(); ++i)
			if (palette[i].doorwayMask == startDoorwayMask)
				roomChoice = i;

		startingCell.validRooms = { roomChoice };
		startingCell.isCollapsed = true;

		// Create vector of room coordinates to update their neighbours list of valid rooms after the collapse
		std::vector<glm::ivec2> updateRooms;
		updateRooms.push_back(start);
		UpdatePossibilities(updateRooms);

		// Place rooms connected by doors until the room limit is reached
		int roomsPlaced = 1;
		while (roomsPlaced < maxRooms)
		{
			std::vector<glm::ivec2> leafRooms;

			// For each cell, if its neighbour has an unconnected door or open side facing it, add to possible placements
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					if (grid[y * width + x].isCollapsed)
						continue;

					uint16_t requiredMask = RequiredMask(x, y);
					
					if (requiredMask != 170)
						leafRooms.push_back({ x, y });
				}
			}

			// If no more rooms can be placed, all edges sealed therefore completed level
			if (leafRooms.empty())
				break;

			// Pick a random room from the list of placeable rooms to place
			glm::ivec2 nextRoomCoords = { -1, -1 };
			std::uniform_int_distribution<size_t> pick(0, leafRooms.size() - 1);
			nextRoomCoords = leafRooms[pick(rng)];

			if (nextRoomCoords.x < 0 || nextRoomCoords.x >= width || nextRoomCoords.y < 0 || nextRoomCoords.y >= height)
				break;

			// Randomly (with weighting) pick the room layout from the list of the cells valid rooms and update collapsed state
			gridIndex = nextRoomCoords.y * width + nextRoomCoords.x;
			GridCell& cell = grid[gridIndex];

			std::vector<float> weights;
			for (int roomIndex : cell.validRooms)
				weights.push_back(palette[roomIndex].weight);

			std::discrete_distribution<> distribution(weights.begin(), weights.end());
			int roomChoice = cell.validRooms[distribution(rng)];

			cell.validRooms = { roomChoice };
			cell.isCollapsed = true;
			roomsPlaced++;

			// Update their neighbours list of valid rooms after the collapse
			updateRooms.push_back({ nextRoomCoords.x, nextRoomCoords.y });
			UpdatePossibilities(updateRooms);
		}

		// After max rooms placed, check for any doors or open spaces leading into abyss and seal them
		while (true)
		{
			std::vector<glm::ivec2> nonTerminatingRooms;

			// For each cell, if its neighbour has an unconnected door or open side facing it, add to necessary placements
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					if (grid[y * width + x].isCollapsed)
						continue;

					if (RequiredMask(x, y) != 170)
						nonTerminatingRooms.push_back({ x, y });
				}
			}

			// Once no more holes, level is finished
			if (nonTerminatingRooms.empty())
				break;

			// For every unsealed wall, place a sealing room
			for (glm::ivec2& room : nonTerminatingRooms)
			{
				// Find the mask that only seals the open sides
				GridCell& cell = grid[room.y * width + room.x];
				uint8_t requiredMask = RequiredMask(room.x, room.y);

				// Add all possible terminating room layouts to a vector
				std::vector<int> terminatingLayouts;
				for (int i = 0; i < palette.size(); ++i)
					if (palette[i].doorwayMask == requiredMask)
						terminatingLayouts.push_back(i);

				// Pick and place a terminating room to seal the level
				if (!terminatingLayouts.empty())
				{
					std::uniform_int_distribution<int> pick(0, terminatingLayouts.size() - 1);
					cell.validRooms = { terminatingLayouts[pick(rng)] };
					cell.isCollapsed = true;
				}
				else
				{
					SDL_Log("No terminating room type found for grid cell x:%d, y:%d", room.x, room.y);
					cell.isCollapsed = true;
				}
			}
		}

		// Only create the level if the goal is reached and level reached correct size
		if (grid[goal.y * width + goal.x].isCollapsed && roomsPlaced >= maxRooms)
			break;

	}
}

void LevelGeneration::InstantiateLevel(Scene* scene)
{
	// For every grid cell, spawn the chosen room in its position with the necessary rotation
	for (int y = 0; y < gridHeight; ++y)
	{
		for (int x = 0; x < gridWidth; ++x)
		{
			int gridIndex = y * gridWidth + x;
			GridCell& cell = grid[gridIndex];

			if (!cell.isCollapsed || cell.validRooms.empty())
				continue;

			int roomIndex = cell.validRooms[0];
			Room& room = palette[roomIndex];

			glm::vec3 worldPos(x * 10.0f, 0.0f, y * -10.0f);
			glm::quat rotation = glm::angleAxis(glm::radians(room.rotation), glm::vec3(0, 1, 0));
			EntityID roomEntity = scene->InstantiatePrefab(room.asset, worldPos, rotation);
		}
	}
	SDL_Log("LevelGenerator: Successfully instantiated %d rooms.", gridWidth * gridHeight);
}



// Scans the loaded asset for doorway nodes, returning a bitmask of open doorway directions
uint16_t LevelGeneration::ScanDoorways(Asset* asset)
{
	if (!asset)
		return 0;
	 
	// Initialise mask, then check which doorways/walls the asset contains, setting mask bits accordingly
	uint16_t mask = 0;
	for (size_t i = 0; i < asset->node_count; i++)
	{
		const char* nodeName = asset->nodes[i].name;

		// Doorway type overrides wall type 
		if (strcmp(nodeName, "Doorway_N") == 0)
			SetWallType(mask, NORTH, DOOR);
		else if (strcmp(nodeName, "Wall_N") == 0)
			if (GetWallType(mask, NORTH) != DOOR)
				SetWallType(mask, NORTH, WALL);

		if (strcmp(nodeName, "Doorway_E") == 0)
			SetWallType(mask, EAST, DOOR);
		else if (strcmp(nodeName, "Wall_E") == 0)
			if (GetWallType(mask, EAST) != DOOR)
				SetWallType(mask, EAST, WALL);

		if (strcmp(nodeName, "Doorway_S") == 0)
			SetWallType(mask, SOUTH, DOOR);
		else if (strcmp(nodeName, "Wall_S") == 0)
			if (GetWallType(mask, SOUTH) != DOOR)
				SetWallType(mask, SOUTH, WALL);

		if (strcmp(nodeName, "Doorway_W") == 0)
			SetWallType(mask, WEST, DOOR);
		else if (strcmp(nodeName, "Wall_W") == 0)
			if (GetWallType(mask, WEST) != DOOR)
				SetWallType(mask, WEST, WALL);
	}
	return mask;
}

// Shifting the mask left with wrap around rotates the room clockwise
uint16_t LevelGeneration::RotateMask(uint16_t mask, int rotations)
{
	rotations = rotations % 4;
	for (int i = 0; i < rotations; i++)
	{
		// Store all current wall types, then reassign them clockwise 
		WallType northType = GetWallType(mask, NORTH);
		WallType eastType = GetWallType(mask, EAST);
		WallType southType = GetWallType(mask, SOUTH);
		WallType westType = GetWallType(mask, WEST);

		SetWallType(mask, EAST, northType);
		SetWallType(mask, SOUTH, eastType);
		SetWallType(mask, WEST, southType);
		SetWallType(mask, NORTH, westType);
	}
	return mask;
}

WallType LevelGeneration::GetWallType(uint16_t mask, DoorDirection direction)
{
	// Shifts the 2 direction bits to leftmost and returns them
	return (WallType)((mask >> direction) & 0x03);
}

void LevelGeneration::SetWallType(uint16_t& mask, DoorDirection direction, WallType wallType)
{
	// Clear the correct bits, then assign the new wall type to them
	mask &= ~(0x03 << direction);
	mask |= wallType << direction;
}