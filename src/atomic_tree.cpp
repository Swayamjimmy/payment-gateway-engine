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

// Lock-free lock using CAS retry loop
bool AtomicPaymentTree::lock(const std::string& nodeName, int userId) {
    AtomicTreeNode* node = getNode(nodeName);
    if (!node) return false;

    // Check if any ancestor is locked (atomic read with acquire semantics)
    if (node->ancestorLockedCount.load(std::memory_order_acquire) > 0) {
        return false;
    }

    // Check if any descendant is locked
    if (node->descendantLockedCount.load(std::memory_order_acquire) > 0) {
        return false;
    }

    // CAS loop: atomically set lockState from 0 (unlocked) to 1 (locked)
    int expected = 0;
    while (!node->lockState.compare_exchange_weak(expected, 1,
            std::memory_order_release, std::memory_order_relaxed)) {
        if (expected != 0) {
            // Node is already locked by someone else
            return false;
        }
        // Spurious failure - reset expected and retry
        metrics.casRetries.fetch_add(1, std::memory_order_relaxed);
        expected = 0;
    }

    // Re-validate after CAS success (another thread may have locked an ancestor/descendant)
    if (node->ancestorLockedCount.load(std::memory_order_acquire) > 0 ||
        node->descendantLockedCount.load(std::memory_order_acquire) > 0) {
        // Rollback: release the lock we just acquired
        node->lockState.store(0, std::memory_order_release);
        return false;
    }

    // Lock acquired successfully - record owner and propagate counts
    node->lockedBy.store(userId, std::memory_order_release);
    informAncestorsAtomic(node, 1);
    informDescendantsAtomic(node, 1);
    return true;
}

// Lock-free unlock: CAS clears lock only if the userId matches
bool AtomicPaymentTree::unlock(const std::string& nodeName, int userId) {
    AtomicTreeNode* node = getNode(nodeName);
    if (!node) return false;

    // Verify the node is locked by this user (atomic read)
    if (node->lockedBy.load(std::memory_order_acquire) != userId) {
        return false;
    }

    // CAS to clear lock state from 1 to 0
    int expected = 1;
    if (!node->lockState.compare_exchange_weak(expected, 0,
            std::memory_order_release, std::memory_order_relaxed)) {
        metrics.casRetries.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // Clear ownership and propagate count decrements
    node->lockedBy.store(-1, std::memory_order_release);
    informAncestorsAtomic(node, -1);
    informDescendantsAtomic(node, -1);
    return true;
}