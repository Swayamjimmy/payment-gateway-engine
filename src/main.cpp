#include "atomic_tree_node.h"
#include "httplib.h"
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>

// Build the full payment gateway hierarchy
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

// Each worker thread runs this to simulate random payment transactions
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

int main() {
    AtomicPaymentTree tree(3);
    buildPaymentTree(tree);

    std::cout << "=== Payment Gateway Orchestration Engine ===\n";
    std::cout << "Starting HTTP server on http://localhost:8080\n\n";

    httplib::Server svr;

    // Serve static frontend files from the frontend/ directory
    svr.set_mount_point("/", "./frontend");

    // GET /api/tree - returns current tree state as JSON
    svr.Get("/api/tree", [&tree](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(tree.toJson(), "application/json");
    });

    // GET /api/metrics - returns throughput and contention stats
    svr.Get("/api/metrics", [&tree](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        std::string json = "{\"totalTransactions\":" +
            std::to_string(tree.metrics.totalTransactions.load()) +
            ",\"successfulLocks\":" +
            std::to_string(tree.metrics.successfulLocks.load()) +
            ",\"failedLocks\":" +
            std::to_string(tree.metrics.failedLocks.load()) +
            ",\"casRetries\":" +
            std::to_string(tree.metrics.casRetries.load()) + "}";
        res.set_content(json, "application/json");
    });

    // POST /api/simulate - run a concurrent simulation with 8 threads
    svr.Post("/api/simulate", [&tree](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        int numThreads = 8;
        int txnPerThread = 100;
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++) {
            threads.emplace_back(simulateTransactions, std::ref(tree), i, txnPerThread);
        }
        for (auto& t : threads) {
            t.join();
        }

        res.set_content("{\"status\":\"complete\",\"threads\":" +
            std::to_string(numThreads) + ",\"txnPerThread\":" +
            std::to_string(txnPerThread) + "}", "application/json");
    });

    svr.listen("0.0.0.0", 8080);
    return 0;
}