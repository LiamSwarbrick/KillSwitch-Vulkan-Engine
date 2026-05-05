#ifndef PHYSICS_CORE_PHYSICS_SETTINGS_H
#define PHYSICS_CORE_PHYSICS_SETTINGS_H

struct PhysicsSettings
{
	// For rigidbodies
	float initialEnergy = 1.0f;
	float energyThreshold = 0.1f;
	float sleepTimeRequired = 0.5f;
	float sleepBiasRate = 0.2; // To turn to sleepBias = std::pow(sleepBiasRate, dt);

	// World Up vector and Gravity depend on PhysicsWorld, so they stay there
	float fixedStepDuration = 1.0f / 120.0f; // 0.0083333333f
	float maxIterationSteps = 4; // Should prob be 8 or more
	// At 30 fps we would reach the maximum number of 4 iteration steps.
	
};

// Globalization of the settings
inline PhysicsSettings g_PhysicsSettings;

#endif // !PHYSICS_CORE_PHYSICS_SETTINGS_H
