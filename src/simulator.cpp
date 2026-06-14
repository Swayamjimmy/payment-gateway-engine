#include "atomic_tree_node.h"
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>

// Build the payment gateway tree hierarchy
void buildPaymentTree(AtomicPaymentTree& tree) {
    tree.addNode("PaymentSystem");
    tree.addNode("Cards", "PaymentSystem");
    tree.addNode("UPI", "PaymentSystem");
    tree.addNode("Wallets", "PaymentSystem");
    tree.addNode("Visa", "Cards");
    tree.addNode("Mastercard", "Cards");
    tree.addNode("RuPay", "Cards");
    tree.addNode("GPay", "UPI");
    tree.addNode("PhonePe", "UPI");
    tree.addNode("BHIM", "UPI");
    tree.addNode("Paytm", "Wallets");
    tree.addNode("AmazonPay", "Wallets");
    tree.addNode("HDFC", "Visa");
    tree.addNode("ICICI", "Visa");
    tree.addNode("SBI", "Mastercard");
    tree.addNode("Axis", "Mastercard");
    tree.addNode("PNB", "RuPay");
    tree.addNode("BOB", "RuPay");
}

// Simulate concurrent transactions for a single thread
void simulateTransactions(AtomicPaymentTree& tree, int threadId, int numTransactions) {
    std::mt19937 rng(threadId * 42);
    std::vector<std::string> gateways = {
        "HDFC", "ICICI", "SBI", "Axis", "PNB", "BOB",
        "GPay", "PhonePe", "BHIM", "Paytm", "AmazonPay"
    };
    std::uniform_int_distribution<int> dist(0, gateways.size() - 1);

    for (int i = 0; i < numTransactions; i++) {
        std::string target = gateways[dist(rng)];
        tree.metrics.totalTransactions.fetch_add(1, std::memory_order_relaxed);

        if (tree.lock(target, threadId)) {
            tree.metrics.successfulLocks.fetch_add(1, std::memory_order_relaxed);
            // Simulate transaction processing time
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            tree.unlock(target, threadId);
        } else {
            tree.metrics.failedLocks.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

// Verify tree invariants after all threads complete
bool verifyTreeInvariants(AtomicPaymentTree& tree) {
    std::vector<std::string> allNodes = {
        "PaymentSystem", "Cards", "UPI", "Wallets",
        "Visa", "Mastercard", "RuPay",
        "GPay", "PhonePe", "BHIM", "Paytm", "AmazonPay",
        "HDFC", "ICICI", "SBI", "Axis", "PNB", "BOB"
    };

    bool valid = true;
    for (const auto& name : allNodes) {
        AtomicTreeNode* node = tree.getNode(name);
        if (!node) continue;

        // Check no orphaned locks remain
        if (node->lockState.load(std::memory_order_acquire) != 0) {
            std::cerr << "INVARIANT VIOLATION: " << name << " still locked\n";
            valid = false;
        }

        // Check ancestor/descendant counts are consistent
        if (node->ancestorLockedCount.load(std::memory_order_acquire) != 0) {
            std::cerr << "INVARIANT VIOLATION: " << name << " ancestorLockedCount != 0\n";
            valid = false;
        }
        if (node->descendantLockedCount.load(std::memory_order_acquire) != 0) {
            std::cerr << "INVARIANT VIOLATION: " << name << " descendantLockedCount != 0\n";
            valid = false;
        }
    }
    return valid;
}

int main() {
    std::cout << "=== Payment Gateway Concurrent Stress Test ===\n\n";

    AtomicPaymentTree tree(3);
    buildPaymentTree(tree);

    int numThreads = 8;
    int txnPerThread = 1000;

    std::cout << "Launching " << numThreads << " threads, "
              << txnPerThread << " transactions each...\n";

    auto start = std::chrono::high_resolution_clock::now();

    // Spawn worker threads
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(simulateTransactions, std::ref(tree), i, txnPerThread);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Print results
    std::cout << "\n=== Results ===\n";
    std::cout << "Duration: " << duration.count() << " ms\n";
    std::cout << "Total transactions: " << tree.metrics.totalTransactions.load() << "\n";
    std::cout << "Successful locks: " << tree.metrics.successfulLocks.load() << "\n";
    std::cout << "Failed locks: " << tree.metrics.failedLocks.load() << "\n";
    std::cout << "CAS retries: " << tree.metrics.casRetries.load() << "\n";
    std::cout << "Throughput: "
              << (tree.metrics.totalTransactions.load() * 1000) / std::max(1LL, (long long)duration.count())
              << " txn/sec\n";

    // Verify tree invariants
    std::cout << "\n=== Correctness Verification ===\n";
    if (verifyTreeInvariants(tree)) {
        std::cout << "All invariants passed! No orphaned locks or inconsistent counts.\n";
    } else {
        std::cerr << "FAILURE: Tree invariants violated!\n";
        return 1;
    }

    return 0;
}