#include "force_registry.h"


void ForceRegistry::addGenerator(IForceGenerator* gen)
{
	globalGenerators.push_back(gen);
}

void ForceRegistry::removeGenerator(IForceGenerator* gen)
{
	globalGenerators.erase(
		std::remove_if(globalGenerators.begin(), globalGenerators.end(),
			[&](const IForceGenerator* otherGen)
			{
				return gen == otherGen;
			}),
		globalGenerators.end()
	);
}

void ForceRegistry::addPair(RigidBody* body, IForceGenerator* gen)
{

}

void ForceRegistry::removePair(RigidBody* body, IForceGenerator* gen)
{
	pairedGenerators.erase(
		std::remove_if(pairedGenerators.begin(), pairedGenerators.end(),
			[&](const Entry& entry)
			{
				return body == entry.body && gen == entry.generator;
			}),
		pairedGenerators.end()
	);
}

void ForceRegistry::removeAll(RigidBody* body)
{
	pairedGenerators.erase(
		std::remove_if(pairedGenerators.begin(), pairedGenerators.end(),
			[&](const Entry& entry)
			{
				return body == entry.body;
			}),
		pairedGenerators.end()
	);
}

void ForceRegistry::removeAll(IForceGenerator* gen)
{
	removeGenerator(gen);

	pairedGenerators.erase(
		std::remove_if(pairedGenerators.begin(), pairedGenerators.end(),
			[&](const Entry& entry)
			{
				return gen == entry.generator;
			}),
		pairedGenerators.end()
	);
}

void ForceRegistry::applyAll(std::vector<RigidBody>& bodies, float dt)
{
	for (RigidBody& body : bodies)
	{
		if (body.isStatic) continue;

		for (IForceGenerator* gen : globalGenerators)
		{
			if (!(body.forceLayers & gen->targetLayers)) continue;
			gen->apply(body, dt);
		}
	}

	for (Entry& entry : pairedGenerators)
	{
		if (entry.body->isStatic) continue;
		// We skip the check of forceLayers, because its a pair
		entry.generator->apply(*entry.body, dt);
	}

}