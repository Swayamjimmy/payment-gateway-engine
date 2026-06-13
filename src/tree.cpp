#include "tree_node.h"

// Constructor: store the branching factor (max children per node)
PaymentTree::PaymentTree(int childrenPerNode) : root(nullptr), m(childrenPerNode) {}

// Destructor: recursively free all heap-allocated nodes
PaymentTree::~PaymentTree() {
    deleteTree(root);
}

// Recursively delete a node and all its descendants
void PaymentTree::deleteTree(TreeNode* node) {
    if (!node) return;
    for (TreeNode* child : node->children) {
        deleteTree(child);
    }
    delete node;
}

// Add a new node to the tree under the specified parent
void PaymentTree::addNode(const std::string& name, const std::string& parentName) {
    if (parentName.empty()) {
        // No parent means this is the root node
        root = new TreeNode(name);
        nodeMap[name] = root;
        return;
    }

    // Find the parent node and attach the new child
    TreeNode* parent = getNode(parentName);
    if (!parent) return;

    TreeNode* newNode = new TreeNode(name, parent);
    parent->children.push_back(newNode);
    nodeMap[name] = newNode;
}

// Look up any node by name in O(1) using the hash map
TreeNode* PaymentTree::getNode(const std::string& name) {
    auto it = nodeMap.find(name);
    if (it != nodeMap.end()) {
        return it->second;
    }
    return nullptr;
}

std::string PaymentTree::toJson() const {
    return "{}";
}

void PaymentTree::informAncestors(TreeNode* node, int delta) {
    // Walk up the parent chain, adjusting each ancestor's
    // descendantLockedCount so they know a descendant changed state
    TreeNode* current = node->parent;
    while (current != nullptr) {
        current->descendantLockedCount += delta;
        current = current->parent;
    }
}

void PaymentTree::informDescendants(TreeNode* node, int delta) {
    // Recursively visit all children, adjusting their
    // ancestorLockedCount so they know an ancestor changed state
    for (TreeNode* child : node->children) {
        child->ancestorLockedCount += delta;
        informDescendants(child, delta);
    }
}

bool PaymentTree::lock(const std::string& nodeName, int userId) {
    TreeNode* node = getNode(nodeName);
    if (node == nullptr) return false;

    // Check 1: node must not already be locked
    if (node->isLocked) return false;

    // Check 2: no ancestor can be locked (O(1) check using counter)
    if (node->ancestorLockedCount > 0) return false;

    // Check 3: no descendant can be locked (O(1) check using counter)
    if (node->descendantLockedCount > 0) return false;

    // All checks passed - set the lock state
    node->isLocked = true;
    node->lockedBy = userId;

    // Propagate metadata: tell ancestors and descendants about this lock
    informAncestors(node, 1);
    informDescendants(node, 1);

    return true;
}

bool PaymentTree::unlock(const std::string& nodeName, int userId) {
    TreeNode* node = getNode(nodeName);
    if (node == nullptr) return false;

    // Only the user who locked it can unlock it
    if (!node->isLocked) return false;
    if (node->lockedBy != userId) return false;

    // Clear the lock state
    node->isLocked = false;
    node->lockedBy = -1;

    // Reverse the metadata propagation
    informAncestors(node, -1);
    informDescendants(node, -1);

    return true;
}

void PaymentTree::collectLockedDescendants(TreeNode* node, int userId,
    std::vector<TreeNode*>& result, bool& valid) {
    // Recursively find all locked descendants and verify they
    // all belong to the same userId
    for (TreeNode* child : node->children) {
        if (child->isLocked) {
            if (child->lockedBy != userId) {
                valid = false;
                return;
            }
            result.push_back(child);
        }
        collectLockedDescendants(child, userId, result, valid);
        if (!valid) return;
    }
}

bool PaymentTree::upgradeLock(const std::string& nodeName, int userId) {
    TreeNode* node = getNode(nodeName);
    if (node == nullptr) return false;

    // Target node must not already be locked
    if (node->isLocked) return false;

    // No ancestor can be locked
    if (node->ancestorLockedCount > 0) return false;

    // Must have at least one locked descendant to upgrade from
    if (node->descendantLockedCount == 0) return false;

    // Collect all locked descendants and verify they belong to this user
    std::vector<TreeNode*> lockedDescendants;
    bool valid = true;
    collectLockedDescendants(node, userId, lockedDescendants, valid);

    if (!valid) return false;

    // Unlock all descendants (consolidating into the parent)
    for (TreeNode* desc : lockedDescendants) {
        desc->isLocked = false;
        desc->lockedBy = -1;
        informAncestors(desc, -1);
        informDescendants(desc, -1);
    }

    // Lock the target node
    node->isLocked = true;
    node->lockedBy = userId;
    informAncestors(node, 1);
    informDescendants(node, 1);

    return true;
}