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
	if (roomAIndex < 0 || roomAIndex >= palette.size() || roomBIndex < 0 || roomBIndex >= palette.size())
		SDL_Log("Trying to check neighbour connections on invalid room numbers, RoomA: %d, RoomB: %d", roomAIndex, roomBIndex);

	const Room& roomA = palette[roomAIndex];
	const Room& roomB = palette[roomBIndex];

	WallType roomAWallType = GetWallType(roomA.doorwayMask, direction);
	WallType roomBWallType = GetWallType(roomB.doorwayMask, (DoorDirection)(((int)direction + 4) % 8));

	// Rooms can only connect if both walls are same type
	if (roomAWallType != roomBWallType)
		return false;

	// Rooms can only connect if themes match when connection is open
	if (roomAWallType == OPEN && roomA.theme != roomB.theme)
		return false;

	return true;
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

// Gets a list of the themes of a cells neighbours, then randomly picks a theme from the list
Theme LevelGeneration::GetPreferredTheme(glm::ivec2 room)
{
	// Iterate through all neighbours and if connected by open or door, add their theme to a list
	std::vector<Theme> neighbourThemes;
	for (int i = 0; i < 4; ++i)
	{
		glm::ivec2 neighbourCoords = { room.x + neighbourOffsets[i].x, room.y + neighbourOffsets[i].y };
		
		if (neighbourCoords.x >= 0 && neighbourCoords.x < gridWidth && neighbourCoords.y >= 0 && neighbourCoords.y < gridHeight)
		{
			GridCell& neighbourCell = grid[neighbourCoords.y * gridWidth + neighbourCoords.x];
			if (neighbourCell.isCollapsed && GetWallType(palette[neighbourCell.validRooms[0]].doorwayMask, (DoorDirection)(((int)directions[i] + 4) % 8)) != WALL)
				neighbourThemes.push_back(palette[neighbourCell.validRooms[0]].theme);
		}
	}

	// Default theme is outside
	if (neighbourThemes.empty())
	{
		SDL_Log("No neighbours to propagate theme at grid cell: %d, %d", room.x, room.y);
		return OUTSIDE;
	}

	// Pick a random theme from the list, with higher weighting to those appearing multiple times
	std::uniform_int_distribution<size_t> randomTheme(0, neighbourThemes.size() - 1);
	return neighbourThemes[randomTheme(rng)];
}

// Scans vector of room assets to fill a palette of room variations
void LevelGeneration::BuildPalette(const std::vector<Asset*>& roomAssets)
{
	palette.clear();

	// Scan every room asset and store all non-duplicate rotations of it
	for (Asset* asset : roomAssets)
	{
		Theme theme = OUTSIDE;
		uint16_t defaultMask = ScanDoorways(asset, theme);

		// For every room rotation, check if its a dupe, weight it, then push it to the palette
		for (int i = 0; i < 4; ++i)
		{
			uint16_t rotatedMask = RotateMask(defaultMask, i);
			float rotation = i * -90.0f;

			bool roomDuplicate = false;
			for (const auto& room : palette)
			{
				if (room.asset == asset && room.doorwayMask == rotatedMask && room.theme == theme)
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
					weight *= 0.25f;
					break;
				case 3:
					weight *= 0.25f;
					break;
				case 2:
					weight *= 3.5f;
					break;
				case 1:
					weight *= 0.5f;
					break;
				default:
					weight *= 1.0f;
					break;
				}

				switch (doors)
				{
				case 4:
					weight *= 0.5f;
					break;
				case 3:
					weight *= 0.5f;
					break;
				case 2:
					weight *= 1.5f;
					break;
				case 1:
					weight *= 0.5f;
					break;
				default:
					weight *= 1.0f;
					break;
				}

				switch (walls)
				{
				case 4:
					weight *= 4.0f;
					break;
				case 3:
					weight *= 1.25f;
					break;
				case 2:
					weight *= 1.5f;
					break;
				case 1:
					weight *= 1.5f;
					break;
				default:
					weight *= 0.4f;
					break;
				}

				palette.push_back({ asset, rotatedMask, rotation, weight, theme });
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

			// If no rooms are valid, then a room type is missing, output which room type
			if (updatedValidRooms.empty()) 
			{
				uint16_t requiredMask = RequiredMask(neighbourRoom.x, neighbourRoom.y);
				SDL_Log("Room at %d,%d has no valid rooms left, it needs: N:%d E:%d S:%d W:%d", neighbourRoom.x, neighbourRoom.y,
					GetWallType(requiredMask, NORTH), GetWallType(requiredMask, EAST), GetWallType(requiredMask, SOUTH), GetWallType(requiredMask, WEST));
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

std::vector<glm::ivec2> LevelGeneration::GetTraversableRooms(glm::ivec2 startRoom)
{
	// Create vectors and push the starting room onto the stack
	std::vector<glm::ivec2> traversableRooms;
	std::vector<bool> visited(gridWidth * gridHeight, false);
	std::vector<glm::ivec2> roomStack;

	roomStack.push_back(startRoom);
	int gridIndex = startRoom.y * gridWidth + startRoom.x;
	visited[gridIndex] = true;

	// Visit all rooms connected to the start room, and return their coords
	while (!roomStack.empty())
	{
		glm::ivec2 currentRoom = roomStack.back();
		roomStack.pop_back();
		traversableRooms.push_back(currentRoom);

		// Go through each non-visited valid neighbouring room
		for (int i = 0; i < 4; ++i)
		{
			glm::ivec2 neighbourRoom = { currentRoom.x + neighbourOffsets[i].x, currentRoom.y + neighbourOffsets[i].y };
			if (neighbourRoom.x < 0 || neighbourRoom.x >= gridWidth || neighbourRoom.y < 0 || neighbourRoom.y >= gridHeight)
				continue;

			int currentIndex = currentRoom.y * gridWidth + currentRoom.x;
			int neighbourIndex = neighbourRoom.y * gridWidth + neighbourRoom.x;

			if (visited[neighbourIndex])
				continue;

			// If there is a doorway or opening between the rooms, push the neighbour to the stack and specify it as visited
			uint8_t currentMask = palette[grid[currentIndex].validRooms[0]].doorwayMask;
			uint8_t neighbourMask = palette[grid[neighbourIndex].validRooms[0]].doorwayMask;

			if ((GetWallType(currentMask, directions[i]) == OPEN && GetWallType(neighbourMask, (DoorDirection)(((int)directions[i] + 4) % 8)) == OPEN) || 
				(GetWallType(currentMask, directions[i]) == DOOR && GetWallType(neighbourMask, (DoorDirection)(((int)directions[i] + 4) % 8)) == DOOR))
			{
				visited[neighbourIndex] = true;
				roomStack.push_back(neighbourRoom);
			}
		}
	}
	return traversableRooms;
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

		// Collapsing starting cell
		int gridIndex = start.y * width + start.x;
		GridCell& startingCell = grid[gridIndex];
		int roomChoice = -1;

		// Pick a random theme from themes and choose starting room
		std::uniform_int_distribution<int> theme(0, THEME_COUNT - 1);
		Theme randomTheme = (Theme)(theme(rng));
		for (int i = 0; i < palette.size(); ++i)
			if (palette[i].doorwayMask == startDoorwayMask && palette[i].theme == randomTheme)
				roomChoice = i;

		SDL_assert(roomChoice != -1);
		startingCell.validRooms = { roomChoice };
		startingCell.isCollapsed = true;

		// Create and add the starting room to a vector of rooms that need their neighbours updated
		std::vector<glm::ivec2> updateRooms;
		updateRooms.push_back(start);

		// For each cardinal direction, also make a room 2-5 cells away from the starting room
		for (int i = 0; i < 4; ++i)
		{
			// Get random offset from start
			std::uniform_int_distribution<int> dirDist(2, 5);
			glm::ivec2 offset = neighbourOffsets[i] * dirDist(rng);
			glm::ivec2 position = start + offset;

			gridIndex = position.y * width + position.x;
			GridCell& Cell = grid[gridIndex];
			roomChoice = -1;

			// Pick a random theme 
			std::uniform_int_distribution<int> theme(0, THEME_COUNT - 1);
			randomTheme = (Theme)(theme(rng));
			for (int i = 0; i < palette.size(); ++i)
				if (palette[i].doorwayMask == ((DOOR << NORTH) + (DOOR << EAST) + (DOOR << SOUTH) + (DOOR << WEST)) && palette[i].theme == randomTheme)
					roomChoice = i;

			SDL_assert(roomChoice != -1);
			Cell.validRooms = { roomChoice };
			Cell.isCollapsed = true;
			updateRooms.push_back({ position.x, position.y });
		}

		UpdatePossibilities(updateRooms);

		// Place rooms connected by doors until the room limit is reached
		int roomsPlaced = 5;
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

			gridIndex = nextRoomCoords.y * width + nextRoomCoords.x;
			GridCell& cell = grid[gridIndex];
			
			// Select a theme and weight that theme so it has a higher chance of being picked
			Theme preferredTheme = GetPreferredTheme(nextRoomCoords);
			std::vector<float> weights;
			for (int roomIndex : cell.validRooms)
			{
				float weight = palette[roomIndex].weight;

				if (palette[roomIndex].theme == preferredTheme)
					weight *= 10.0f;

				weights.push_back(weight);
			}

			// Randomly (with weighting) pick the room layout from the list of the cells valid rooms and update collapsed state
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
				Theme preferredTheme = GetPreferredTheme(room);

				// Add all possible terminating room layouts to a vector
				std::vector<int> terminatingLayouts;
				for (int i = 0; i < palette.size(); ++i)
					if (palette[i].doorwayMask == requiredMask && palette[i].theme == preferredTheme)
						terminatingLayouts.push_back(i);

				// Pick and place a terminating room to seal the level, keep adding to roomsPlaced
				if (!terminatingLayouts.empty())
				{
					std::uniform_int_distribution<int> pick(0, terminatingLayouts.size() - 1);
					cell.validRooms = { terminatingLayouts[pick(rng)] };
					cell.isCollapsed = true;
					roomsPlaced++;
				}
				else
				{
					// Try again, without the theme constraint
					for (int i = 0; i < palette.size(); ++i)
						if (palette[i].doorwayMask == requiredMask)
							terminatingLayouts.push_back(i);

					// If no terminating room exists, missing some assets
					SDL_assert(!terminatingLayouts.empty());

					std::uniform_int_distribution<int> pick(0, terminatingLayouts.size() - 1);
					cell.validRooms = { terminatingLayouts[pick(rng)] };
					cell.isCollapsed = true;
					roomsPlaced++;
				}
			}
		}

		// Find the coords of all rooms reachable from the starting room
		std::vector<glm::ivec2> traversableRooms = GetTraversableRooms(start);

		// Only create a valid level if it reaches the goal, is traversable enough and has placed enough rooms
		float amountTraversable = (float)traversableRooms.size() / (roomsPlaced);
		if (grid[goal.y * width + goal.x].isCollapsed && amountTraversable > 0.99f && roomsPlaced >= maxRooms)
			break;
		else
			SDL_Log("Flipped up trying again..., traversable: %f, roomsplaced: %d, maxrooms: %d", amountTraversable, roomsPlaced, maxRooms);

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

			// Placing gaps instead of solid rooms
			if (room.doorwayMask == 170)
				continue;

			glm::vec3 worldPos(x * 10.0f, 0.0f, y * -10.0f);
			glm::quat rotation = glm::angleAxis(glm::radians(room.rotation), glm::vec3(0, 1, 0));
			EntityID roomEntity = scene->InstantiatePrefab(room.asset, worldPos, rotation);
		}
	}
}



// Scans the loaded asset for doorway nodes, returning a bitmask of open doorway directions
uint16_t LevelGeneration::ScanDoorways(Asset* asset, Theme& theme)
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
		else if (strcmp(nodeName, "Wall_N") == 0 || strcmp(nodeName, "Wall_N_Full") == 0)
			if (GetWallType(mask, NORTH) != DOOR)
				SetWallType(mask, NORTH, WALL);

		if (strcmp(nodeName, "Doorway_E") == 0)
			SetWallType(mask, EAST, DOOR);
		else if (strcmp(nodeName, "Wall_E") == 0 || strcmp(nodeName, "Wall_E_Full") == 0)
			if (GetWallType(mask, EAST) != DOOR)
				SetWallType(mask, EAST, WALL);

		if (strcmp(nodeName, "Doorway_S") == 0)
			SetWallType(mask, SOUTH, DOOR);
		else if (strcmp(nodeName, "Wall_S") == 0 || strcmp(nodeName, "Wall_S_Full") == 0)
			if (GetWallType(mask, SOUTH) != DOOR)
				SetWallType(mask, SOUTH, WALL);

		if (strcmp(nodeName, "Doorway_W") == 0)
			SetWallType(mask, WEST, DOOR);
		else if (strcmp(nodeName, "Wall_W") == 0 || strcmp(nodeName, "Wall_W_Full") == 0)
			if (GetWallType(mask, WEST) != DOOR)
				SetWallType(mask, WEST, WALL);

		// Get theme
		if (strcmp(nodeName, "Floor_Inside") == 0)
			theme = INSIDE;
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