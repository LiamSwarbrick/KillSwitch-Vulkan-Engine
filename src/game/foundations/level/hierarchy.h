#include <vector>

// NOT FINISHED

template <typename IDType>
class HierarchyNode {
public:
    IDType id;

    HierarchyNode* parent;
    std::vector<HierarchyNode*> children;

private:
    inline void RemoveChild(size_t index)
    {
        // swap to back
        std::swap(children[index], children.back());
        
        children.pop_back();
    }

public:
    HierarchyNode* AddChild(HierarchyNode* child) {
        child->parent = this;

        children.push_back(child);
    }

    void RemoveChild(HierarchyNode* child) {
        bool found = false;
        for (size_t i = 0; (i < children.size()) && !found; i++)
        {
            if (child->id == children[i]->id)
            {
                found = true;
                RemoveChild(i);
            }
        }

        SDL_assert(found);
    }
};

template <typename IDType>
class Hierarchy {

private:
    using HNode = HierarchyNode<IDType>;

    std::vector<HNode*> hierarchy;
    std::vector<HNode> allNodes;

public:

    void AddNode(IDType id) {

    }

    void AddNode(IDType id, IDType parent) {

    }

    void DeleteNode(IDType id) {

    }
    
};