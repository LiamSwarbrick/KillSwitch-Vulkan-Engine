#ifndef ECS_REGISTRY_H
#define ECS_REGISTRY_H

#include "SDL3/SDL.h"

#include "types.h"
#include "sparse_set.h"

#include <limits>
#include <vector>
#include <bitset>
#include <memory>

namespace AdvEng
{
	// forward declaring View
	template <typename...>
	class View;
	class Entity;
	
	class ECS
	{

	private:


		// Just for debugging.
		// Needs to be inline
		inline static std::vector<std::string> m_componentNames;

		SparseSet<ComponentMask> m_entityMasks;
		SparseSet<std::string> m_entityTags;

		// inactive entities which can be replaced
		std::vector<EntityID> m_availableEntities;

		std::vector<std::unique_ptr<ISparseSet>> m_componentPools;

		// Highest recorded entity ID
		EntityID m_maxID = 0;


	// private:
#warning NOTE(Liam): I'm making all of these public since they're called outside this class in multiple files. Maybe adjust the API
	public:

		// Metaprogramming magic with next method
		static size_t GetNextComponentIndex(std::string typeName)
		{
			static size_t index = 0;
			m_componentNames.push_back(typeName);
			return index++;
		};

		// Returns a unique ID for each type (Metaprogramming magic)
		template <typename T>
		static size_t GetComponentIndex()
		{
			static size_t index = GetNextComponentIndex(typeid(T).name());
			return index;
		};

		// Same as GetComponentTypeIndex, but will register if the component doesn't exist yet.
		template <typename T>
		size_t GetOrRegisterComponentIndex()
		{
			size_t index = GetComponentIndex<T>();

			if (index >= m_componentPools.size() || !m_componentPools[index])
				RegisterComponent<T>();

			return index;
		}

		// Returns pointer to the component pool 
		template <typename T>
		ISparseSet* GetComponentPoolPtr()
		{
			size_t index = GetOrRegisterComponentIndex<T>();
			return m_componentPools[index].get();
		}

		// Returns reference to the component pool
		template <typename T>
		SparseSet<T>& GetComponentPool()
		{
			ISparseSet* genericPtr = GetComponentPoolPtr<T>();
			SparseSet<T>* pool = static_cast<SparseSet<T>*>(genericPtr);

			return *pool;
		}

		template <typename Component>
		void SetComponentBit(ComponentMask& mask, bool val)
		{
			size_t bitPos = GetComponentIndex<Component>();
			mask[bitPos] = val;
		}

		template <typename Component>
		ComponentMask::reference GetComponentBit(ComponentMask& mask)
		{
			size_t bitPos = GetComponentIndex<Component>();
			return mask[bitPos];
		}

		ComponentMask& GetEntityMask(EntityID id)
		{
			SDL_assert(IsEntityValid(id));

			ComponentMask* mask = m_entityMasks.GetPtr(id);
			return *mask;
		}

	public:

		ECS()
		{
			// Paranoid
			Clear();
		}

		void Clear()
		{
			m_componentPools.clear();
			m_entityMasks.Clear();
			m_entityTags.Clear();
			m_availableEntities.clear();
			m_maxID = 0;
		}

		
		//Creates Entity with tag / name
		EntityID CreateEntity(std::string tag = "")
		{
			// Init with NULL_ENTITY ID
			EntityID id = NULL_ID;

			// If we don't have recyclable entities
			if (m_availableEntities.size() == 0)
			{
				if (m_maxID >= MAX_ENTITIES)
				{
					// SDL_ log (not logarithm) ("Max number of entities reached");
					SDL_assert(false);
				}
				// Don't forget to increment the maxID
				id = m_maxID++;
			}
			else
			{
				// Recycle EntityID
				id = m_availableEntities.back();
				m_availableEntities.pop_back();
			}

			// We have a proper EntityID
			// Empty constructor creates all 0 bitmask
			m_entityMasks.Set(id, {});

			if (!tag.empty())
				m_entityTags.Set(id, tag);

			return id;
		}

		bool IsEntityValid(EntityID id)
		{
			return (id < m_maxID) && 
				(m_entityMasks.GetPtr(id) != nullptr);
		}

		std::string GetEntityTag(EntityID id)
		{
			SDL_assert(IsEntityValid(id));

			std::string* tag = m_entityTags.GetPtr(id);
			if (tag)
				return *tag;

			return "Entity";
		}


		void DeleteEntity(EntityID id)
		{
			SDL_assert(IsEntityValid(id));

			std::string tag = GetEntityTag(id);
			ComponentMask& mask = GetEntityMask(id);

			// Delete all associated components
			for (int i = 0; i < MAX_COMPONENTS; i++)
			{
				if (mask[i] == 1)
					m_componentPools[i]->Delete(id);
			}
			
			m_entityMasks.Delete(id);
			m_entityTags.Delete(id);
			m_availableEntities.push_back(id);
		}

		// Register a component
		template <typename T>
		void RegisterComponent()
		{
			if (m_componentPools.size() <= MAX_COMPONENTS)
			{
				// SDL_assert
			}

			size_t index = GetComponentIndex<T>();
			if (index >= m_componentPools.size())
				m_componentPools.resize(index + 1);

			// Check if we're not registering a component twice
			SDL_assert(!m_componentPools[index]);

			m_componentPools[index] = std::make_unique<SparseSet<T>>();
		}

		// Adds a component to an entity
		// ecs.AddComponent<RenderableComponent>(id, Renderable{...data...});
		template <typename T>
		T& AddComponent(EntityID id, T&& component = {})
		{
			SDL_assert(IsEntityValid(id));

			SparseSet<T>& pool = GetComponentPool<T>();

			// If component already exists, overwrite
			if (pool.GetPtr(id))
				return *pool.Set(id, std::move(component));

			ComponentMask& mask = GetEntityMask(id);
			SetComponentBit<T>(mask, 1);

			return *pool.Set(id, std::move(component));
		}

		// Removes a component from an entity
		// ecs.RemoveComponent<RenderableComponent>(id);
		template <typename T>
		void RemoveComponent(EntityID id)
		{
			SDL_assert(IsEntityValid(id));

			SparseSet<T>& pool = GetComponentPool<T>();

			if (!pool.GetPtr(id)) return;

			ComponentMask& mask = GetEntityMask(id);
			SetComponentBit<T>(mask, 0);

			pool.Delete(id);
		}

		template <typename T>
		T& GetComponent(EntityID id)
		{
			SDL_assert(IsEntityValid(id));
			// We could check the pointer after returning if this is not too performant
			// SDL_assert(GetComponentBit<T>(id));
			// SDL_assert(component); // after calling pool.GetComponent(id);

			SparseSet<T>& pool = GetComponentPool<T>();
			T& component = pool.Get(id);

			return component;
		}

		// Returns the component pointer of the entity
		template <typename T>
		T* GetComponentPtr(EntityID id)
		{
			SDL_assert(IsEntityValid(id));
			// SDL_assert(GetComponentBit<T>(mask));

			SparseSet<T>& pool = GetComponentPool<T>();
			return pool.GetPtr(id);
		}

		// Variadic Templates
		template <typename... Types>
		bool Has(EntityID id)
		{
			auto& mask = GetEntityMask(id);
			return (GetComponentBit<Types>(mask) && ...);
		}

		template <typename... Types>
		bool HasAny(EntityID id)
		{
			return (Has<Types>(id) || ...);
		}

		size_t GetEntityCount()
		{
			return m_entityMasks.Size();
		}

		template <typename T>
		size_t GetComponentCount()
		{
			return GetComponentPool<T>().Size();
		}

		size_t GetPoolCount()
		{
			return m_componentPools.size();
		}

		template <typename... Types>
		View<Types...> GetView()
		{
			// default constructor using our ECS
			// return
			return View<Types...>(this);
			//return { this };
		}

		// For debug UI
        std::vector<EntityID> GetAllEntities()
        {
            return m_entityMasks.GetIDList();
        }

        ComponentMask GetEntityComponentMask(EntityID id)
        {
            SDL_assert(IsEntityValid(id));
            return GetEntityMask(id);
        }

        std::string GetComponentName(size_t bit_index)
        {
            if (bit_index < m_componentNames.size())
                return m_componentNames[bit_index];
            return "Unknown";
        }

        template <typename T>
        size_t GetComponentBitIndex()
        {
            return GetOrRegisterComponentIndex<T>();
        }

	};

	

}

#endif //ECS_REGISTRY_H