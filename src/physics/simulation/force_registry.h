#ifndef PHYSICS_SIMULATION_FORCE_REGISTRY_H
#define PHYSICS_SIMULATION_FORCE_REGISTRY_H

#include "physics/core/types.h"

#include "force_generators.h"


class ForceRegistry
{
public:
	void addGenerator(IForceGenerator* gen);
	void removeGenerator(IForceGenerator* gen);

	void addPair(RigidBody* body, IForceGenerator* gen);
	void removePair(RigidBody* body, IForceGenerator* gen);

	void removeAll(RigidBody* body);
	void removeAll(IForceGenerator* gen);

	void applyAll(std::vector<RigidBody>& bodies, float dt);
	void applyAll(std::vector<BodySlot>& bodies, float dt);

private:

	struct PairedEntry
	{
		RigidBody* body;
		IForceGenerator* generator;
	};

	std::vector<IForceGenerator*> globalGenerators;
	std::vector<PairedEntry> pairedGenerators;
};

#endif // !PHYSICS_SIMULATION_FORCE_REGISTRY_H
