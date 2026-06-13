#include "atomic_tree_node.h"
#include <queue>
#include <functional>

// Constructor initializes an empty tree with max children per node
AtomicPaymentTree::AtomicPaymentTree(int childrenPerNode) : root(nullptr), m(childrenPerNode) {}

// Destructor recursively frees all nodes
AtomicPaymentTree::~AtomicPaymentTree() {
    deleteTree(root);
}

void AtomicPaymentTree::deleteTree(AtomicTreeNode* node) {
    if (!node) return;
    for (auto* child : node->children) {
        deleteTree(child);
    }
    delete node;
}

// Add a node to the tree (called during setup, not thread-safe)
void AtomicPaymentTree::addNode(const std::string& name, const std::string& parentName) {
    if (parentName.empty()) {
        root = new AtomicTreeNode(name);
        nodeMap[name] = root;
        return;
    }
    auto it = nodeMap.find(parentName);
    if (it == nodeMap.end()) return;
    AtomicTreeNode* parent = it->second;
    AtomicTreeNode* node = new AtomicTreeNode(name, parent);
    parent->children.push_back(node);
    nodeMap[name] = node;
}

// O(1) lookup by name
AtomicTreeNode* AtomicPaymentTree::getNode(const std::string& name) {
    auto it = nodeMap.find(name);
    return (it != nodeMap.end()) ? it->second : nullptr;
}

// Propagate lock count changes up to all ancestors using fetch_add
void AtomicPaymentTree::informAncestorsAtomic(AtomicTreeNode* node, int delta) {
    AtomicTreeNode* current = node->parent;
    while (current) {
        current->descendantLockedCount.fetch_add(delta, std::memory_order_release);
        current = current->parent;
    }
}

// Propagate lock count changes down to all descendants using fetch_add
void AtomicPaymentTree::informDescendantsAtomic(AtomicTreeNode* node, int delta) {
    for (auto* child : node->children) {
        child->ancestorLockedCount.fetch_add(delta, std::memory_order_release);
        informDescendantsAtomic(child, delta);
    }
}