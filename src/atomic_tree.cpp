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

bool AtomicPaymentTree::unlock(const std::string& nodeName, int userId) {
    AtomicTreeNode* node = getNode(nodeName);
    if (!node) return false;

    // 1. Verify ownership
    if (node->lockedBy.load(std::memory_order_acquire) != userId) {
        return false;
    }

    // 2. Clear ownership first
    node->lockedBy.store(-1, std::memory_order_release);

    // 3. FIX: Use compare_exchange_strong to prevent spurious failures
    int expected = 1;
    if (!node->lockState.compare_exchange_strong(expected, 0,
            std::memory_order_release, std::memory_order_relaxed)) {
        // Rollback ownership in the highly unlikely event of an actual CAS failure
        node->lockedBy.store(userId, std::memory_order_release);
        metrics.casRetries.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    // 4. Propagate counts
    informAncestorsAtomic(node, -1);
    informDescendantsAtomic(node, -1);
    return true;
}

bool AtomicPaymentTree::upgradeLock(const std::string& nodeName, int userId) {
    AtomicTreeNode* node = getNode(nodeName);
    if (!node) return false;

    // Node must not already be locked
    if (node->lockState.load(std::memory_order_acquire) != 0) {
        return false;
    }

    // No ancestor can be locked
    if (node->ancestorLockedCount.load(std::memory_order_acquire) > 0) {
        return false;
    }

    // BFS to collect locked descendants
    std::vector<AtomicTreeNode*> lockedDescendants;
    std::queue<AtomicTreeNode*> bfs;
    for (auto* child : node->children) {
        bfs.push(child);
    }
    while (!bfs.empty()) {
        AtomicTreeNode* current = bfs.front();
        bfs.pop();
        if (current->lockState.load(std::memory_order_acquire) == 1) {
            if (current->lockedBy.load(std::memory_order_acquire) != userId) {
                return false; 
            }
            lockedDescendants.push_back(current);
        }
        for (auto* child : current->children) {
            bfs.push(child);
        }
    }

    if (lockedDescendants.empty()) {
        return false;
    }

    // Unlock descendants
    for (auto* desc : lockedDescendants) {
        desc->lockState.store(0, std::memory_order_release);
        desc->lockedBy.store(-1, std::memory_order_release);
        informAncestorsAtomic(desc, -1);
        informDescendantsAtomic(desc, -1);
    }

    // FIX: Use compare_exchange_strong here as well
    int expected = 0;
    if (!node->lockState.compare_exchange_strong(expected, 1,
            std::memory_order_release, std::memory_order_relaxed)) {
        return false;
    }

    node->lockedBy.store(userId, std::memory_order_release);
    informAncestorsAtomic(node, 1);
    informDescendantsAtomic(node, 1);
    return true;
}

// Serialize tree state to JSON for the frontend
std::string AtomicPaymentTree::toJson() const {
    if (!root) return "{}";

    std::function<std::string(AtomicTreeNode*)> serialize = [&](AtomicTreeNode* node) -> std::string {
        std::string json = "{\"name\":\"" + node->name + "\",\"isLocked\":" +
            (node->lockState.load(std::memory_order_acquire) == 1 ? "true" : "false") +
            ",\"children\":[";
        for (size_t i = 0; i < node->children.size(); i++) {
            if (i > 0) json += ",";
            json += serialize(node->children[i]);
        }
        json += "]}";
        return json;
    };

    return serialize(root);
}