#ifndef ECS_ENTITY_HPP
#define ECS_ENTITY_HPP

#include "types.hpp"
#include "registry.hpp"

namespace AdvEng
{
	// we should probably NOT use the following class
	// at least NOT in the Scene hierarchy.
	// this is only a "helper" class
	class Entity
	{
	private:
		ECS* m_ecs;
		EntityID m_id;

	public:
		Entity(ECS* ecs, EntityID id)
			: m_ecs(ecs), m_id(id)
		{
		}

		template <typename T>
		inline void AddComponent(T&& component = {})
		{
			m_ecs->AddComponent<T>(m_id, component);
		}

		template <typename T>
		inline void RemoveComponent()
		{
			m_ecs->RemoveComponent<T>(m_id);
		}

		template <typename T>
		inline T& GetComponent()
		{
			return m_ecs->GetComponent<T>(m_id);
		}
	};
}

#endif // !ECS_ENTITY_HPP
