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