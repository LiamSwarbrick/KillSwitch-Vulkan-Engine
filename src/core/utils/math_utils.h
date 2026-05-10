#ifndef CORE_UTILS_MATH_UTILS_H
#define CORE_UTILS_MATH_UTILS_H

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/quaternion.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

// Might be wise to add a Namespace
namespace Math
{
	// Easing functions (after its the Rotate/Translate Towards AND Lerp, N/Slerp)
	inline float EaseLinear(float t)
	{
		return 0.0f;
	}

	// Modeled after the parabola y = x^2
	inline float EaseInQuadratic(float p)
	{
		return p * p;
	}

	// Modeled after the parabola y = -x^2 + 2x
	inline float EaseOutQuadratic(float p)
	{
		return p * (2.0f - p);
	}

	// Modeled after the piecewise quadratic
	// y = (1/2)((2x)^2)             ; [0, 0.5)
	// y = -(1/2)((2x-1)*(2x-3) - 1) ; [0.5, 1]
	inline float EaseInOutQuadratic(float p)
	{
		if (p < 0.5f)
		{
			return 2.0f * p * p;
		}
		else
		{
			return (-2.0f * p * p) + (4.0f * p) - 1;
		}
	}

	inline constexpr auto& EaseIn = EaseInQuadratic;
	inline constexpr auto& EaseOut = EaseOutQuadratic;
	inline constexpr auto& EaseInOut = EaseInOutQuadratic;

	inline float Smoothstep(float p)
	{
		return p + p * (3.0f - 2.0f * p);
	}

	// Modeled after the cubic y = x^3
	inline float EaseInCubic(float p)
	{
		return p * p * p;
	}

	// Modeled after the cubic y = (x - 1)^3 + 1
	inline float EaseOutCubic(float p)
	{
		float f = (p - 1.0f);
		return f * f * f + 1.0f;
	}

	// Modeled after the piecewise cubic
	// y = (1/2)((2x)^3)       ; [0, 0.5)
	// y = (1/2)((2x-2)^3 + 2) ; [0.5, 1]
	inline float EaseInOutCubic(float p)
	{
		if (p < 0.5f)
		{
			return 4 * p * p * p;
		}
		else
		{
			float f = ((2.0f * p) - 2.0f);
			return 0.5f * f * f * f + 1.0f;
		}
	}

	// Modeled after the quartic x^4
	inline float EaseInQuartic(float p)
	{
		return p * p * p * p;
	}

	// Modeled after the quartic y = 1 - (x - 1)^4
	inline float EaseOutQuartic(float p)
	{
		float f = (p - 1.0f);
		return f * f * f * (1.0f - p) + 1.0f;
	}

	// Modeled after the piecewise quartic
	// y = (1/2)((2x)^4)        ; [0, 0.5)
	// y = -(1/2)((2x-2)^4 - 2) ; [0.5, 1]
	inline float EaseInOutQuartic(float p)
	{
		if (p < 0.5f)
		{
			return 8.0f * p * p * p * p;
		}
		else
		{
			float f = (p - 1.0f);
			return -8.0f * f * f * f * f + 1;
		}
	}

	// Modeled after the quintic y = x^5
	inline float EaseInQuintic(float p)
	{
		return p * p * p * p * p;
	}

	// Modeled after the quintic y = (x - 1)^5 + 1
	inline float EaseOutQuintic(float p)
	{
		float f = (p - 1.0f);
		return f * f * f * f * f + 1;
	}

	// Modeled after the piecewise quintic
	// y = (1/2)((2x)^5)       ; [0, 0.5)
	// y = (1/2)((2x-2)^5 + 2) ; [0.5, 1]
	inline float EaseInOutQuintic(float p)
	{
		if (p < 0.5f)
		{
			return 16.0f * p * p * p * p * p;
		}
		else
		{
			float f = ((2.0f * p) - 2.0f);
			return  0.5f * f * f * f * f * f + 1;
		}
	}

	// Modeled after quarter-cycle of sine wave
	inline float EaseInSine(float p)
	{
		return sin((p - 1.0f) * M_PI_2) + 1;
	}

	// Modeled after quarter-cycle of sine wave (different phase)
	inline float EaseOutSine(float p)
	{
		return sin(p * M_PI_2);
	}

	// Modeled after half sine wave
	inline float EaseInOutSine(float p)
	{
		return 0.5f * (1 - cos(p * M_PI));
	}

	// Modeled after shifted quadrant IV of unit circle
	inline float EaseInCircular(float p)
	{
		return 1.0f - sqrt(1.0f - (p * p));
	}

	// Modeled after shifted quadrant II of unit circle
	inline float EaseOutCircular(float p)
	{
		return sqrt((2.0f - p) * p);
	}

	// Modeled after the piecewise circular function
	// y = (1/2)(1 - sqrt(1 - 4x^2))           ; [0, 0.5)
	// y = (1/2)(sqrt(-(2x - 3)*(2x - 1)) + 1) ; [0.5, 1]
	inline float EaseInOutCircular(float p)
	{
		if (p < 0.5f)
		{
			return 0.5f * (1.0f - sqrt(1.0f - 4.0f * (p * p)));
		}
		else
		{
			return 0.5f * (sqrt(-((2.0f * p) - 3.0f) * ((2.0f * p) - 1.0f)) + 1.0f);
		}
	}

	// Modeled after the exponential function y = 2^(10(x - 1))
	inline float EaseInExponential(float p)
	{
		return (p == 0.0f) ? p : pow(2.0f, 10.0f * (p - 1));
	}

	// Modeled after the exponential function y = -2^(-10x) + 1
	inline float EaseOutExponential(float p)
	{
		return (p == 1.0f) ? p : 1.0f - pow(2.0f, -10.0f * p);
	}

	// Modeled after the piecewise exponential
	// y = (1/2)2^(10(2x - 1))         ; [0,0.5)
	// y = -(1/2)*2^(-10(2x - 1))) + 1 ; [0.5,1]
	inline float EaseInOutExponential(float p)
	{
		if (p == 0.0f || p == 1.0f) return p;

		if (p < 0.5f)
		{
			return 0.5f * pow(2.0f, (20.0f * p) - 10.0f);
		}
		else
		{
			return -0.5f * pow(2.0f, (-20.0f * p) + 10.0f) + 1.0f;
		}
	}

	// Modeled after the damped sine wave y = sin(13pi/2*x)*pow(2, 10 * (x - 1))
	inline float EaseInElastic(float p)
	{
		return sin(13.0f * M_PI_2 * p) * pow(2.0f, 10.0f * (p - 1));
	}

	// Modeled after the damped sine wave y = sin(-13pi/2*(x + 1))*pow(2, -10x) + 1
	inline float EaseOutElastic(float p)
	{
		return sin(-13.0f * M_PI_2 * (p + 1.0f)) * pow(2.0f, -10.0f * p) + 1;
	}

	// Modeled after the piecewise exponentially-damped sine wave:
	// y = (1/2)*sin(13pi/2*(2*x))*pow(2, 10 * ((2*x) - 1))      ; [0,0.5)
	// y = (1/2)*(sin(-13pi/2*((2x-1)+1))*pow(2,-10(2*x-1)) + 2) ; [0.5, 1]
	inline float EaseInOutElastic(float p)
	{
		if (p < 0.5f)
		{
			return 0.5f * sin(13.0f * M_PI_2 * (2.0f * p)) * pow(2.0f, 10.0f * ((2.0f * p) - 1.0f));
		}
		else
		{
			return 0.5f * (sin(-13.0f * M_PI_2 * ((2.0f * p - 1) + 1)) * pow(2.0f, -10.0f * (2.0f * p - 1.0f)) + 2.0f);
		}
	}

	// Modeled after the overshooting cubic y = x^3-x*sin(x*pi)
	inline float EaseInBack(float p)
	{
		return p * p * p - p * sin(p * M_PI);
	}

	// Modeled after overshooting cubic y = 1-((1-x)^3-(1-x)*sin((1-x)*pi))
	inline float EaseOutBack(float p)
	{
		float f = (1.0f - p);
		return 1.0f - (f * f * f - f * sin(f * M_PI));
	}

	// Modeled after the piecewise overshooting cubic function:
	// y = (1/2)*((2x)^3-(2x)*sin(2*x*pi))           ; [0, 0.5)
	// y = (1/2)*(1-((1-x)^3-(1-x)*sin((1-x)*pi))+1) ; [0.5, 1]
	inline float EaseInOutBack(float p)
	{
		if (p < 0.5f)
		{
			float f = 2.0f * p;
			return 0.5f * (f * f * f - f * sin(f * M_PI));
		}
		else
		{
			float f = (1.0f - (2.0f * p - 1.0f));
			return 0.5f * (1.0f - (f * f * f - f * sin(f * M_PI))) + 0.5f;
		}
	}

	inline float EaseOutBounce(float p)
	{
		if (p < 4.0f / 11.0f)
		{
			return (121.0f * p * p) / 16.0f;
		}
		else if (p < 8.0f / 11.0f)
		{
			return (363.0f / 40.0 * p * p) - (99.0f / 10.0f * p) + 17.0f / 5.0f;
		}
		else if (p < 9.0f / 10.0f)
		{
			return (4356.0f / 361.0f * p * p) - (35442.0f / 1805.0f * p) + 16061.0f / 1805.0f;
		}
		else
		{
			return (54.0f / 5.0f * p * p) - (513.0f / 25.0f * p) + 268.0f / 25.0f;
		}
	}

	inline float EaseInBounce(float p)
	{
		return 1 - EaseOutBounce(1.0f - p);
	}

	inline float EaseInOutBounce(float p)
	{
		if (p < 0.5f)
		{
			return 0.5f * EaseInBounce(p * 2.0f);
		}
		else
		{
			return 0.5f * EaseOutBounce(p * 2.0f - 1.0f) + 0.5f;
		}
	}

	inline glm::vec3 RotateAroundY(const glm::vec3& dir, float angle)
	{
		float cosA = glm::cos(angle);
		float sinA = glm::sin(angle);

		return glm::normalize(glm::vec3(
			dir.x * cosA - dir.z * sinA,
			0.0f,
			dir.x * sinA + dir.z * cosA
		));
	}

	glm::vec3 RotateTowardTarget(
		const glm::vec3& currentDir,
		const glm::vec3& targetDir,
		float turnSpeed,
		float deltaTime,
		std::function<float(float)> easingFn = EaseLinear
	);

	glm::quat RotateTowardTarget(
		const glm::quat& currentRot,
		const glm::quat& targetRot,
		float turnSpeed,
		float deltaTime,
		std::function<float(float)> easingFn = EaseLinear
	);

	glm::quat RotateTowardTargetAccurate(
		const glm::quat& currentRot,
		const glm::quat& targetRot,
		float turnSpeed,
		float deltaTime,
		std::function<float(float)> easingFn = EaseLinear
	);

	// Unsure if we're going to need this, maybe to move things outside of rigidbodies
	glm::vec3 TranslateTowardTarget(
		const glm::vec3& currentPos,
		const glm::vec3& targetPos,
		float moveSpeed,
		float deltaTime,
		std::function<float(float)> easingFn = EaseLinear
	);

	// Scalars, please DO NOT use for glm::vec3, use glm::mix(a, b, EasingFunction(t)); / or translate t using the easing function first if inline fucks up
	template<typename T>
	inline T Lerp(const T& a, const T& b, float t, std::function<float(float)> easingFn = EaseLinear)
	{
		static_assert(!std::is_same_v<T, glm::quat>, "Use NLerp or SLerp for quaternion interpolation");
		t = easingFn(t);
		return a + (b - a) * t;
	}

	// Won't work for directions
	inline glm::vec3 Nlerp(const glm::vec3& a, const glm::vec3& b, float t, std::function<float(float)> easingFn = EaseLinear)
	{
		return glm::normalize(glm::mix(a, b, easingFn(t)));
	}

	inline glm::quat Nlerp(const glm::quat& a, const glm::quat& b, float t, std::function<float(float)> easingFn = EaseLinear)
	{
		glm::quat bCorrected = glm::dot(a, b) < 0.0f ? -b : b;
		t = easingFn(t);
		return glm::normalize(glm::mix(a, bCorrected, t));
	}

	inline glm::vec3 Slerp(const glm::vec3& a, const glm::vec3& b, float t, std::function<float(float)> easingFn = EaseLinear);

	inline glm::quat Slerp(const glm::quat& a, const glm::quat& b, float t, std::function<float(float)> easingFn = EaseLinear)
	{
		t = easingFn(t);
		return glm::slerp(a, b, t);
	}

	// Considering forward = z -> (0, 0, 1)
	inline glm::vec3 QuatToViewDir(const glm::quat& q)
	{
		return glm::normalize(q * glm::vec3(0.0f, 0.0f, 1.0f));
	}

	inline glm::quat ViewDirToQuat(const glm::vec3& viewDir)
	{
		return glm::rotation(glm::normalize(viewDir), glm::vec3(0.0f, 0.0f, 1.0f));
	}

	inline glm::quat GetRotationBetweenDirs(const glm::vec3& from, const glm::vec3& to)
	{
		return glm::rotation(glm::normalize(from), glm::normalize(to));
	}

}



#endif //!CORE_UTILS_MATH_UTILS_H