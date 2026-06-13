#ifndef TREE_NODE_H
#define TREE_NODE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>

struct TreeNode {
    std::string name;
    TreeNode* parent;
    std::vector<TreeNode*> children;
    bool isLocked;
    int lockedBy;
    int ancestorLockedCount;
    int descendantLockedCount;

    TreeNode(const std::string& nodeName, TreeNode* parentNode = nullptr)
        : name(nodeName), parent(parentNode), isLocked(false),
          lockedBy(-1), ancestorLockedCount(0), descendantLockedCount(0) {}
};