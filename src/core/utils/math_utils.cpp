#include "math_utils.h"

glm::vec3 Math::RotateTowardTarget(const glm::vec3& currentDir, const glm::vec3& targetDir, float turnSpeed, float deltaTime, std::function<float(float)> easingFn)
{
	float cosAngle = glm::dot(currentDir, targetDir);
	float angle = glm::acos(cosAngle);

	if (angle < 1e-6f)
		return targetDir;

	float maxAngle = turnSpeed * deltaTime;

	if (angle <= maxAngle)
		return targetDir;

	float t = maxAngle / angle;

	return Nlerp(currentDir, targetDir, t, easingFn);
}

glm::quat Math::RotateTowardTarget(const glm::quat& currentRot, const glm::quat& targetRot, float turnSpeed, float deltaTime, std::function<float(float)> easingFn)
{
	// Choose the closest path
	glm::quat correctedTarget = glm::dot(currentRot, targetRot) < 0.0f ? -targetRot : targetRot;

	float angle = glm::angle(glm::inverse(currentRot) * correctedTarget); // or invert the correctedTarget

	if (angle < 1e-6f)
		return targetRot;

	// We should guard against exact opposite directions, because it might choose the wrong 

	float maxAngle = turnSpeed * deltaTime;

	if (angle <= maxAngle)
		return targetRot;

	float t = maxAngle / angle;
	return Slerp(currentRot, targetRot, t, easingFn);
}

glm::quat Math::RotateTowardTargetAccurate(const glm::quat& currentRot, const glm::quat& targetRot, float turnSpeed, float deltaTime, std::function<float(float)> easingFn)
{
	// Choose the closest path
	glm::quat correctedTarget = glm::dot(currentRot, targetRot) < 0.0f ? -targetRot : targetRot;

	float angle = glm::angle(glm::inverse(currentRot) * correctedTarget); // or invert the correctedTarget

	if (angle < 1e-6f)
		return targetRot;

	float maxAngle = turnSpeed * deltaTime;

	if (angle <= maxAngle)
		return targetRot;

	float t = maxAngle / angle;
	return Slerp(currentRot, targetRot, t, easingFn);
}

glm::vec3 Math::TranslateTowardTarget(const glm::vec3& currentPos, const glm::vec3& targetPos, float moveSpeed, float deltaTime, std::function<float(float)> easingFn)
{
	glm::vec3 currentToTarget = targetPos - currentPos;
	float distance = glm::length(currentToTarget);

	if (distance < 1e-6f)
		return targetPos;

	float maxStep = moveSpeed * deltaTime;

	if (distance <= maxStep)
		return targetPos;

	float t = easingFn(maxStep / distance);

	return glm::mix(currentPos, targetPos, t);
}

glm::vec3 Math::Slerp(const glm::vec3& a, const glm::vec3& b, float t, std::function<float(float)> easingFn)
{
	float cosAngle = glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
	float angle = glm::acos(cosAngle);

	if (angle < 1e-6f)
		return b;

	// If the vectors are opposite (cosAngle = -1.0f, or angle = M_PI (180º))
	glm::vec3 correctedB = (cosAngle < -1.0f + 1e-5f) ? RotateAroundY(b, glm::radians(10.0f)) : b;

	t = easingFn(t);

	float invSinAngle = 1.0f / glm::sin(angle);
	float weightA = glm::sin((1.0f - t) * angle) * invSinAngle;
	float weightB = glm::sin(t * angle) * invSinAngle;

	return a * weightA + correctedB * weightB;
}


