#include "force_registry.h"

#include <memory>
#include <algorithm>


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
	pairedGenerators.push_back({ body, gen });
}

void ForceRegistry::removePair(RigidBody* body, IForceGenerator* gen)
{
	pairedGenerators.erase(
		std::remove_if(pairedGenerators.begin(), pairedGenerators.end(),
			[&](const PairedEntry& entry)
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
			[&](const PairedEntry& entry)
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
			[&](const PairedEntry& entry)
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
		if (body.sleeping || body.isStatic || body.isTrigger || body.isKinematic) continue;

		for (IForceGenerator* gen : globalGenerators)
		{
			if (!(body.forceLayers & gen->targetLayers)) continue;
			gen->apply(body, dt);
		}
	}

	for (PairedEntry& entry : pairedGenerators)
	{
		RigidBody& body = *entry.body;
		if (body.sleeping || body.isStatic || body.isTrigger || body.isKinematic) continue;
		// We skip the check of 
		// 
		// , because its a pair
		entry.generator->apply(body, dt);
	}

}