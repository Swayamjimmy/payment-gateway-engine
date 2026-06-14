#include "atomic_tree_node.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

// Build a small test tree
void buildTestTree(AtomicPaymentTree& tree) {
    tree.addNode("Root");
    tree.addNode("A", "Root");
    tree.addNode("B", "Root");
    tree.addNode("C", "A");
    tree.addNode("D", "A");
    tree.addNode("E", "B");
}

// Each thread tries to lock a leaf node, hold it briefly, then unlock
void workerThread(AtomicPaymentTree& tree, int threadId, int iterations) {
    std::vector<std::string> targets = {"C", "D", "E"};
    std::string target = targets[threadId % targets.size()];

    for (int i = 0; i < iterations; i++) {
        if (tree.lock(target, threadId)) {
            // Verify we actually own it
            AtomicTreeNode* node = tree.getNode(target);
            assert(node->lockedBy.load() == threadId);
            assert(node->lockState.load() == 1);

            // Simulate brief work
            std::this_thread::yield();

            // Unlock and verify
            bool unlocked = tree.unlock(target, threadId);
            assert(unlocked);
        }
    }
}

int main() {
    
    AtomicPaymentTree tree(3);
    buildTestTree(tree);

    std::cout << "=== Concurrent Correctness Test ===\n";

    // Test 1: Basic lock exclusivity
    assert(tree.lock("C", 1) == true);
    assert(tree.lock("C", 2) == false);  // Already locked
    assert(tree.lock("A", 3) == false);  // Descendant locked
    assert(tree.unlock("C", 1) == true);
    std::cout << "[PASS] Basic lock exclusivity\n";

    // Test 2: Ancestor/descendant conflict
    assert(tree.lock("A", 1) == true);
    assert(tree.lock("C", 2) == false);  // Ancestor locked
    assert(tree.lock("D", 2) == false);  // Ancestor locked
    assert(tree.lock("B", 2) == true);   // Sibling is fine
    assert(tree.unlock("A", 1) == true);
    assert(tree.unlock("B", 2) == true);
    std::cout << "[PASS] Ancestor/descendant conflict\n";

    // Test 3: UpgradeLock
    assert(tree.lock("C", 1) == true);
    assert(tree.lock("D", 1) == true);
    assert(tree.upgradeLock("A", 1) == true);  // Consolidate C+D into A
    assert(tree.getNode("C")->lockState.load() == 0);  // C now unlocked
    assert(tree.getNode("D")->lockState.load() == 0);  // D now unlocked
    assert(tree.getNode("A")->lockState.load() == 1);  // A now locked
    assert(tree.unlock("A", 1) == true);
    std::cout << "[PASS] UpgradeLock\n";

    // Test 4: Multi-threaded stress test
    int numThreads = 8;
    int iterationsPerThread = 10000;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(workerThread, std::ref(tree), i, iterationsPerThread);
    }
    for (auto& t : threads) {
        t.join();
    }

    // Verify invariants after all threads complete
    AtomicTreeNode* root = tree.getNode("Root");
    assert(root->descendantLockedCount.load() == 0);
    AtomicTreeNode* a = tree.getNode("A");
    assert(a->descendantLockedCount.load() == 0);
    assert(a->ancestorLockedCount.load() == 0);

    std::cout << "[PASS] Multi-threaded stress test (" << numThreads
              << " threads x " << iterationsPerThread << " iterations)\n";
    std::cout << "CAS retries: " << tree.metrics.casRetries.load() << "\n";
    std::cout << "\n=== All tests passed! ===\n";

    return 0;
}