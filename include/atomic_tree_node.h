#ifndef ATOMIC_TREE_NODE_H
#define ATOMIC_TREE_NODE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>

// Lock-free tree node using atomic integers for all mutable state
struct AtomicTreeNode {
    std::string name;
    AtomicTreeNode* parent;
    std::vector<AtomicTreeNode*> children;
    std::atomic<int> lockState;            // 0 = unlocked, 1 = locked
    std::atomic<int> lockedBy;             // userId who holds the lock (-1 = none)
    std::atomic<int> ancestorLockedCount;  // how many ancestors are locked
    std::atomic<int> descendantLockedCount; // how many descendants are locked

    AtomicTreeNode(const std::string& nodeName, AtomicTreeNode* parentNode = nullptr)
        : name(nodeName), parent(parentNode),
          lockState(0), lockedBy(-1),
          ancestorLockedCount(0), descendantLockedCount(0) {}
};

// Lock-free payment tree with CAS-based operations
class AtomicPaymentTree {
public:
    AtomicPaymentTree(int childrenPerNode);
    ~AtomicPaymentTree();

    void addNode(const std::string& name, const std::string& parentName = "");
    bool lock(const std::string& nodeName, int userId);
    bool unlock(const std::string& nodeName, int userId);
    bool upgradeLock(const std::string& nodeName, int userId);
    AtomicTreeNode* getNode(const std::string& name);
    std::string toJson() const;

    // Performance metrics tracked atomically
    struct Metrics {
        std::atomic<long long> totalTransactions{0};
        std::atomic<long long> successfulLocks{0};
        std::atomic<long long> failedLocks{0};
        std::atomic<long long> casRetries{0};
    };

    Metrics metrics;

private:
    AtomicTreeNode* root;
    int m;
    std::unordered_map<std::string, AtomicTreeNode*> nodeMap;

    void informAncestorsAtomic(AtomicTreeNode* node, int delta);
    void informDescendantsAtomic(AtomicTreeNode* node, int delta);
    void deleteTree(AtomicTreeNode* node);
};

#endif