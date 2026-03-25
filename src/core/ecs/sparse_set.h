#ifndef ECS_SPARSE_SET_H
#define ECS_SPARSE_SET_H


#include "types.h"

#include <vector>
#include <limits>
#include <array>

namespace AdvEng
{

    class ISparseSet
    {
    public:
        virtual ~ISparseSet() = default;
        virtual void Reserve(EntityID) = 0;
        virtual void Clear() = 0;
        virtual bool Has(EntityID) = 0;
        virtual void Delete(EntityID) = 0;
        virtual size_t Size() = 0;
        virtual std::vector<EntityID> GetIDList() = 0;
    };

    template <typename T>
    class SparseSet : public ISparseSet
    {

        

    private:
        // v2: paginated sparse vector
        std::vector<SparsePage> m_pages;
        std::vector<T> m_dense;

        // This will allow us to get to the sparse (entity id) while only reading the dense
        std::vector<EntityID> m_denseToSparse;

        //EntityID m_size; // we could use m_dense.size()


    private:
        inline void SetDenseIndex(EntityID id, EntityID index)
        {
            EntityID pageIndex = id / PAGE_SIZE;
            EntityID pageOffset = id % PAGE_SIZE;

            // if we don't have a page allocated for the id
            if (pageIndex >= m_pages.size())
            {
                m_pages.resize(pageIndex + 1);
                // don't forget to set all page values to NULL_ID
                m_pages[pageIndex].fill(NULL_ID);
            }

            SparsePage& page = m_pages[pageIndex];
            page[pageOffset] = index;
            // page[pageOffset] = m_size++;
        }

        inline EntityID GetDenseIndex(EntityID id)
        {
            // v2 uses pagination, otherwise m_sparse[id]
            EntityID pageIndex = id / PAGE_SIZE;
            EntityID pageOffset = id % PAGE_SIZE;

            if (pageIndex < m_pages.size())
            {
                SparsePage& page = m_pages[pageIndex];
                return page[pageOffset];
            }

            return NULL_ID;
        }

    public:
        SparseSet()
        {
            Reserve(2000);
        }

        ~SparseSet() override
        {
            Clear();
        }

        // Main utilities

        // Set the sparse value / Insert into dense
        T* Set(EntityID id, T component)
        {
            EntityID index = GetDenseIndex(id);

            // if we already have a value, overwrite it
            if (index != NULL_ID)
            {
                m_dense[index] = component;
                m_denseToSparse[index] = id;

                return &m_dense[index];
            }

            // if we do not have a value in the sparse, set sparseToDense index and push back the value to the dense array.
            SetDenseIndex(id, m_dense.size());

            m_dense.push_back(component);
            m_denseToSparse.push_back(id);

            return &m_dense.back();
        }

        void Delete(EntityID id) override
        {
            if (m_dense.empty()) return;
            EntityID indexToDelete = GetDenseIndex(id);

            if (indexToDelete == NULL_ID) return;

            // swap sparseToDense indices
            SetDenseIndex(m_denseToSparse.back(), indexToDelete);
            SetDenseIndex(id, NULL_ID);

            // swap to back and delete
            std::swap(m_dense.back(), m_dense[indexToDelete]);
            std::swap(m_denseToSparse.back(), m_denseToSparse[indexToDelete]);
            m_dense.pop_back();
            m_denseToSparse.pop_back();
        }

        T& Get(EntityID id)
        {
            EntityID index = GetDenseIndex(id);
            SDL_assert(index == NULL_ID && "SparseSet::GetRef with invalid entity id");
            return m_dense[index];
        }

        T* GetPtr(EntityID id)
        {
            EntityID index = GetDenseIndex(id);
            return (index == NULL_ID) ? nullptr : &m_dense[index];
        }
              

        const std::vector<T>& Data() const
        {
            return m_dense;
        }

        // For View class implementation
        std::vector<EntityID> GetIDList() override
        {
            return m_denseToSparse;
        }

        // Extra utility methods

        void Reserve(EntityID size) override
        {
            if (size > m_dense.size())
            {
                m_dense.reserve(size);
                m_denseToSparse.reserve(size);
            }
        }

        void Clear() override
        {
            m_pages.clear();
            m_dense.clear();
            m_denseToSparse.clear();
        }

        bool Has(EntityID id) override
        {
            return NULL_ID != GetDenseIndex(id);
        }

        size_t Size() override
        {
            return m_dense.size();
        }

        bool IsEmpty()
        {
            return m_dense.empty();
        }

    };

}

#endif // !ECS_SPARSE_SET_H