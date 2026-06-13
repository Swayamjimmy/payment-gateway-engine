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

class PaymentTree {
public:
    PaymentTree(int childrenPerNode);
    ~PaymentTree();

    void addNode(const std::string& name, const std::string& parentName = "");
    bool lock(const std::string& nodeName, int userId);
    bool unlock(const std::string& nodeName, int userId);
    bool upgradeLock(const std::string& nodeName, int userId);
    TreeNode* getNode(const std::string& name);
    std::string toJson() const;

private:
    TreeNode* root;
    int m;
    std::unordered_map<std::string, TreeNode*> nodeMap;

    void informAncestors(TreeNode* node, int delta);
    void informDescendants(TreeNode* node, int delta);
    void collectLockedDescendants(TreeNode* node, int userId, std::vector<TreeNode*>& result, bool& valid);
    void deleteTree(TreeNode* node);
};

#endif  