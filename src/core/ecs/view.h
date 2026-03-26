#ifndef ECS_VIEW_H
#define ECS_VIEW_H

#include "SDL3/SDL.h"

#include "types.h"
#include "sparse_set.h"
#include "registry.h"

#include <vector>
#include <algorithm>
#include <bitset>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <functional>
#include <typeinfo>

namespace AdvEng
{

	// Type container used later, providing compile-time index-type lookup with the ::get<> function, 
	// as well as the number of types via ::size
	template <class... Types>
	struct type_list
	{
		using type_tuple = std::tuple<Types...>;

		// compile-time getter to retrieve the type given to the type_list 
		// usage: type_list::get<0>
		template <size_t Index>
		using get = std::tuple_element_t<Index, type_tuple>;

		static constexpr size_t size = sizeof...(Types);
	};

	template <typename... Components>
	class View
	{
	private:

		using componentTypes = type_list<Components...>;

		ECS* m_ecs;

		std::array<ISparseSet*, sizeof...(Components)> m_viewPools;
		std::vector<ISparseSet*> m_excludedPools;

		// Pointer to the smallest component pool, slight optimization later used in the ForEach implementation
		ISparseSet* m_smallest = nullptr;

		// Returns true if all the pools contain the EntityID
		bool AllContain(EntityID id)
		{
			return std::all_of(m_viewPools.begin(), m_viewPools.end(), [id](ISparseSet* pool)
				{
					return pool->Has(id);
				});
			
		}

		// Returns true
		bool NotExcluded(EntityID id)
		{
			if (m_excludedPools.empty()) return true;
			return std::none_of(m_excludedPools.begin(), m_excludedPools.end(), [id](ISparseSet* pool)
				{
					return pool->Has(id);
				});
		}

		// Compile-time getter method making use of the type_list declared earlier
		template <size_t Index>
		auto GetPoolAt()
		{
			// Getting the component type at compile-time using the type_list
			using componentType = typename componentTypes::template get<Index>;
			// Casting the SparseSet of the component type
			return static_cast<SparseSet<componentType>*>(m_viewPools[Index]);
		}

		// Creates component tuple based on the index_sequence parameter, calling GetPoolAt<Indices>
		template <size_t... Indices>
		auto MakeComponentTuple(EntityID id, std::index_sequence<Indices...>)
		{
			return std::make_tuple((std::ref(GetPoolAt<Indices>()->Get(id)))...);
		}

		// ForEach implementation, that iterates over m_smallest
		template <typename Func>
		void ForEachImpl(Func func)
		{
			constexpr auto inds = std::make_index_sequence<sizeof...(Components)>{};

			// Note this list is a COPY, allowing safe deletion during iteration.
			for (EntityID id : m_smallest->GetIDList())
			{
				if (AllContain(id) && NotExcluded(id))
				{

					// [](EntityID id, Component& c1, Component& c2);
					// constexpr denotes this is evaluated at compile time, which prunes
					// invalid function call branches before runtime to prevent the
					// typical invoke errors you'd see after building.
					if constexpr (std::is_invocable_v<Func, EntityID, Components&...>)
					{
						std::apply(func, std::tuple_cat(std::make_tuple(id), MakeComponentTuple(id, inds)));
					}

					// [](Component& c1, Component& c2);
					else if constexpr (std::is_invocable_v<Func, Components&...>)
					{
						std::apply(func, MakeComponentTuple(id, inds));
					}

					else
					{
						SDL_assert(false);
					}
				}
			}
		}

	public:

		// These are the function signatures you can pass to .ForEach()
		using ForEachFunc = std::function<void(Components&...)>;
		using ForEachFuncWithID = std::function<void(EntityID, Components&...)>;

		View(ECS* ecs) :
			m_ecs(ecs), m_viewPools{ ecs->GetComponentPoolPtr<Components>()... }
		{
			// Should not happen if we're using Component Types that ARE in the ECS
			SDL_assert(m_viewPools.size() == componentTypes::size);

			// Determines the smallest pool of all the component pools given via template.
			// This will work cause in the ForEach loop, we check if the EntityID is contained / not excluded in the rest of the pools
			// Bigger pools will contain IDs not contained in smaller pools, so optimizing lookups
			auto smallestPool = std::min_element(m_viewPools.begin(), m_viewPools.end(),
				[](ISparseSet* a, ISparseSet* b) { return a->Size() < b->Size(); }
			);

		
			SDL_assert(smallestPool != m_viewPools.end());

			m_smallest = *smallestPool;
		}

		template <typename... ExcludedComponents>
		View& Without()
		{
			m_excludedPools = { m_ecs->GetComponentPoolPtr<ExcludedComponents>()... };
			return *this;
		}

		/*
		*  Executes a passed lambda on all the entities that match the
		*  passed parameter pack.
		*
		*  Provided function should follow one of two forms:
		*  [](Component& c1, Component& c2);
		*  OR
		*  [](EntityID id, Component& c1, Component& c2);
		*/
		void ForEach(ForEachFunc func)
		{
			ForEachImpl(func);
		}

		void ForEach(ForEachFuncWithID func)
		{
			ForEachImpl(func);
		}

		/*
		*	Holds an entity id and a tuple of references to the components returned by the view.
		*	Access components that are part of a pack like such:
		*	- auto [componentA, componentB] = pack.components;
		*/
		struct PackedEntity
		{
			EntityID id;
			std::tuple<Components&...> components;
		};

		/*
		*  Useful when you want a way to iterate a view via indices.
		*  e.g:
			auto packed = ecs.View<A, B>().GetPacked();
			for (size_t i = 0; i < packed.size(); i++) {
				auto [a1, b1] = packed[i].components;
			}
		*/
		std::vector<PackedEntity> GetPacked()
		{
			constexpr auto inds = std::make_index_sequence<sizeof...(Components)>{};
			std::vector<PackedEntity> result;

			for (EntityID id : m_smallest->GetIDList())
				if (AllContain(id) && NotExcluded(id))
					result.push_back({ id, MakeComponentTuple(id, inds) });
			return result;
		}


	};

}


#endif // !ECS_VIEW_H