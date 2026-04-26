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
*/

static std::mt19937 rng(std::random_device{}());

// Just literally checks if each room has the necessary door to connect in the direction specified
bool LevelGeneration::CanNeighbour(int roomAIndex, int roomBIndex, DoorDirection direction)
{
	const Room& roomA = palette[roomAIndex];
	const Room& roomB = palette[roomBIndex];
	if (direction == NORTH)
	{
		bool roomANorth = (roomA.doorwayMask & NORTH) != 0;
		bool roomBSouth = (roomB.doorwayMask & SOUTH) != 0;
		return roomANorth == roomBSouth;
	}
	if (direction == EAST)
	{
		bool roomAEast = (roomA.doorwayMask & EAST) != 0;
		bool roomBWest = (roomB.doorwayMask & WEST) != 0;
		return roomAEast == roomBWest;
	}
	if (direction == SOUTH)
	{
		bool roomASouth = (roomA.doorwayMask & SOUTH) != 0;
		bool roomBNorth = (roomB.doorwayMask & NORTH) != 0;
		return roomASouth == roomBNorth;
	}
	if (direction == WEST)
	{
		bool roomAWest = (roomA.doorwayMask & WEST) != 0;
		bool roomBEast = (roomB.doorwayMask & EAST) != 0;
		return roomAWest == roomBEast;
	}
	return false;
}

uint8_t LevelGeneration::RequiredMask(int x, int y)
{
	// Check the neighbouring rooms and generate a required mask for the current room
	uint8_t mask = 0;
	if (y + 1 < gridHeight && grid[(y + 1) * gridWidth + x].isCollapsed && palette[grid[(y + 1) * gridWidth + x].validRooms[0]].doorwayMask & SOUTH)
		mask |= NORTH;
	else if (y - 1 >= 0 && grid[(y - 1) * gridWidth + x].isCollapsed && palette[grid[(y - 1) * gridWidth + x].validRooms[0]].doorwayMask & NORTH)
		mask |= SOUTH;
	else if (x + 1 < gridWidth && grid[y * gridWidth + (x + 1)].isCollapsed && palette[grid[y * gridWidth + (x + 1)].validRooms[0]].doorwayMask & WEST)
		mask |= EAST;
	else if (x - 1 >= 0 && grid[y * gridWidth + (x - 1)].isCollapsed && palette[grid[y * gridWidth + (x - 1)].validRooms[0]].doorwayMask & EAST)
		mask |= WEST;
	return mask;
}

// Scans vector of room assets to fill a palette of room variations
void LevelGeneration::BuildPalette(const std::vector<Asset*>& roomAssets)
{
	palette.clear();

	// Scan every room asset and store all non-duplicate rotations of it
	for (Asset* asset : roomAssets)
	{
		uint8_t defaultMask = ScanDoorways(asset);

		// For every room rotation, check if its a dupe, then push it to the palette
		for (int i = 0; i < 4; ++i)
		{
			uint8_t rotatedMask = RotateMask(defaultMask, i);
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
				// Get the number of doors the room has, weight its placement then add to palette
				int doorCount = 0;
				int doors = rotatedMask;
				while (doors > 0)
				{
					doors &= doors - 1;
					doorCount++;
				}

				//						CHANGE ROOM LAYOUT WEIGHTING HERE
				float weight = 0.0f;
				switch (doorCount) 
				{
				case 4:
					weight = 0.5f;
					break;
				case 3:
					weight = 1.0f;
					break;
				case 2:
					weight = 1.0f;
					break;
				case 1:
					weight = 0.5f;
					break;
				case 0:
					weight = 0.01f;
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

		// For easy neighbour querying
		DoorDirection directions[4] = { NORTH, EAST, SOUTH, WEST };
		glm::ivec2 neighbourDirections[4] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };

		for (int i = 0; i < 4; ++i)
		{
			// Instantiate neighbouring room and check if valid within level boundaries
			glm::ivec2 neighbourRoom = { currentRoom.x + neighbourDirections[i].x, currentRoom.y + neighbourDirections[i].y };
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

		// For easy neighbour querying
		DoorDirection directions[4] = { NORTH, EAST, SOUTH, WEST };
		DoorDirection opposites[4] = { SOUTH, WEST, NORTH, EAST };
		glm::ivec2 neighbourDirections[4] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };

		// Go through each non-visited valid neighbouring room
		for (int i = 0; i < 4; ++i)
		{
			glm::ivec2 neighbourRoom = { currentRoom.x + neighbourDirections[i].x, currentRoom.y + neighbourDirections[i].y };
			if (neighbourRoom.x < 0 || neighbourRoom.x >= gridWidth || neighbourRoom.y < 0 || neighbourRoom.y >= gridHeight)
				continue;

			int currentIndex = currentRoom.y * gridWidth + currentRoom.x;
			int neighbourIndex = neighbourRoom.y * gridWidth + neighbourRoom.x;

			if (visited[neighbourIndex])
				continue;

			// If there is a doorway between the rooms, push the neighbour to the stack and specify it as visited
			uint8_t currentMask = palette[grid[currentIndex].validRooms[0]].doorwayMask;
			uint8_t neighbourMask = palette[grid[neighbourIndex].validRooms[0]].doorwayMask;

			if ((currentMask & directions[i]) && (neighbourMask & opposites[i]))
			{
				visited[neighbourIndex] = true;
				roomStack.push_back(neighbourRoom);
			}
		}
	}
	return traversableRooms;
}

// Generates and instantiates a level
void LevelGeneration::GenerateGrid(int width, int height, glm::ivec2 start, glm::ivec2 goal, uint8_t startDoorwayMask, int maxRooms)
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

				// For rooms on the edge, remove possibility for doorway into the edge
				auto& p = cell.validRooms;
				p.erase(std::remove_if(p.begin(), p.end(), [&](int index)
					{
						uint8_t mask = palette[index].doorwayMask;
						if (y == height - 1 && (mask & NORTH))
							return true;
						if (y == 0 && (mask & SOUTH))
							return true;
						if (x == width - 1 && (mask & EAST))
							return true;
						if (x == 0 && (mask & WEST))
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

			// For each cell, if its neighbour has an unconnected door facing it, add to possible placements
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					if (grid[y * width + x].isCollapsed)
						continue;

					if (RequiredMask(x, y) != 0)
						leafRooms.push_back({ x, y });
				}
			}

			// If no more rooms can be placed connected to doors, all edges sealed therefore completed level
			if (leafRooms.empty())
				break;

			// Pick a random room from the list of placeable rooms to place
			glm::ivec2 nextRoomCoords = { -1, -1 };
			std::uniform_int_distribution<size_t> pick(0, leafRooms.size() - 1);
			nextRoomCoords = leafRooms[pick(rng)];

			if (nextRoomCoords.x < 0 || nextRoomCoords.x >= width || nextRoomCoords.y < 0 || nextRoomCoords.y >= height)
				break;

			// Randomly pick the room layout from the list of the cells valid rooms and update collapsed state
			gridIndex = nextRoomCoords.y * width + nextRoomCoords.x;
			GridCell& cell = grid[gridIndex];

			std::vector<float> weights;
			for (int roomIndex : cell.validRooms)
				weights.push_back(palette[roomIndex].weight);

			std::discrete_distribution<> dist(weights.begin(), weights.end());
			int chosenIndex = dist(rng);
			int roomChoice = cell.validRooms[chosenIndex];

			cell.validRooms = { roomChoice };
			cell.isCollapsed = true;
			roomsPlaced++;

			// Update their neighbours list of valid rooms after the collapse
			updateRooms.push_back({ nextRoomCoords.x, nextRoomCoords.y });
			UpdatePossibilities(updateRooms);
		}

		// After max rooms placed, check for any doors leading into abyss and seal them
		while (true)
		{
			std::vector<glm::ivec2> nonTerminatingRooms;

			// For each cell, if its neighbour has an unconnected door facing it, add to necessary placements
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					if (grid[y * width + x].isCollapsed)
						continue;

					if (RequiredMask(x, y) != 0)
						nonTerminatingRooms.push_back({ x, y });
				}
			}

			// Once no more door holes, level is finished
			if (nonTerminatingRooms.empty())
				break;

			// For every unsealed doorway, place a sealing room
			for (glm::ivec2& room : nonTerminatingRooms)
			{
				// Find the mask that only seals the open doors
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

		// Only create a valid level if the goal is reached and level reached correct size
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
uint8_t LevelGeneration::ScanDoorways(Asset* asset)
{
	if (!asset)
		return 0;
	 
	// Initialise mask, then check which doorways the asset contains, setting mask bits accordingly
	uint8_t mask = 0;
	for (size_t i = 0; i < asset->node_count; i++)
	{
		const char* nodeName = asset->nodes[i].name;
		if (strcmp(nodeName, "Doorway_N") == 0)
			mask |= NORTH;
		else if (strcmp(nodeName, "Doorway_E") == 0)
			mask |= EAST;
		else if (strcmp(nodeName, "Doorway_S") == 0)
			mask |= SOUTH;
		else if (strcmp(nodeName, "Doorway_W") == 0)
			mask |= WEST;
	}
	return mask;
}

// Shifting the mask left with wrap around rotates the room clockwise
uint8_t LevelGeneration::RotateMask(uint8_t mask, int rotations)
{
	// 0, 1, 2, 3 rotations, each one rotating clockwise by shifting bits left
	rotations = rotations % 4;
	for (int i = 0; i < rotations; i++)
	{
		uint8_t shiftedMask = mask << 1;

		// Wrap around if the west bit was shifted
		if (shiftedMask & 16)
			shiftedMask |= 1;
		
		// Only keep the last 4 bits
		mask = shiftedMask & 0x0F;
	}
	return mask;
}