#include "tree_node.h"
#include <iostream>
#include <cassert>

void testBasicLock() {
    PaymentTree tree(3);
    tree.addNode("PaymentSystem");
    tree.addNode("Cards", "PaymentSystem");
    tree.addNode("Visa", "Cards");
    tree.addNode("HDFC", "Visa");

    // Lock a leaf node
    assert(tree.lock("HDFC", 1) == true);
    // Cannot lock same node again
    assert(tree.lock("HDFC", 2) == false);
    // Cannot lock an ancestor while descendant is locked
    assert(tree.lock("Visa", 2) == false);
    assert(tree.lock("Cards", 2) == false);

    std::cout << "[PASS] testBasicLock" << std::endl;
}

void testAncestorBlocking() {
    PaymentTree tree(3);
    tree.addNode("PaymentSystem");
    tree.addNode("Cards", "PaymentSystem");
    tree.addNode("Visa", "Cards");
    tree.addNode("HDFC", "Visa");

    // Lock an ancestor
    assert(tree.lock("Cards", 1) == true);
    // Cannot lock any descendant
    assert(tree.lock("Visa", 2) == false);
    assert(tree.lock("HDFC", 2) == false);

    std::cout << "[PASS] testAncestorBlocking" << std::endl;
}

void testUnlockOwnership() {
    PaymentTree tree(3);
    tree.addNode("PaymentSystem");
    tree.addNode("Cards", "PaymentSystem");
    tree.addNode("Visa", "Cards");

    assert(tree.lock("Visa", 1) == true);
    // Wrong user cannot unlock
    assert(tree.unlock("Visa", 2) == false);
    // Correct user can unlock
    assert(tree.unlock("Visa", 1) == true);
    // After unlock, another user can lock
    assert(tree.lock("Visa", 2) == true);

    std::cout << "[PASS] testUnlockOwnership" << std::endl;
}

void testUpgradeLock() {
    PaymentTree tree(3);
    tree.addNode("PaymentSystem");
    tree.addNode("Cards", "PaymentSystem");
    tree.addNode("Visa", "Cards");
    tree.addNode("HDFC", "Visa");
    tree.addNode("ICICI", "Visa");

    // Lock two descendants with same user
    assert(tree.lock("HDFC", 1) == true);
    assert(tree.lock("ICICI", 1) == true);
    // Upgrade to parent - should succeed
    assert(tree.upgradeLock("Visa", 1) == true);
    // Verify Visa is now locked
    assert(tree.getNode("Visa")->isLocked == true);
    // Verify descendants are unlocked
    assert(tree.getNode("HDFC")->isLocked == false);
    assert(tree.getNode("ICICI")->isLocked == false);

    std::cout << "[PASS] testUpgradeLock" << std::endl;
}

void testUpgradeLockMixedUsers() {
    PaymentTree tree(3);
    tree.addNode("PaymentSystem");
    tree.addNode("Cards", "PaymentSystem");
    tree.addNode("Visa", "Cards");
    tree.addNode("HDFC", "Visa");
    tree.addNode("ICICI", "Visa");

    // Lock descendants with different users
    assert(tree.lock("HDFC", 1) == true);
    assert(tree.lock("ICICI", 2) == true);
    // Upgrade should fail - mixed users
    assert(tree.upgradeLock("Visa", 1) == false);

    std::cout << "[PASS] testUpgradeLockMixedUsers" << std::endl;
}

void testUpgradeLockNoDescendants() {
    PaymentTree tree(3);
    tree.addNode("PaymentSystem");
    tree.addNode("Cards", "PaymentSystem");
    tree.addNode("Visa", "Cards");

    // No descendants are locked - upgrade should fail
    assert(tree.upgradeLock("Cards", 1) == false);

    std::cout << "[PASS] testUpgradeLockNoDescendants" << std::endl;
}

int main() {
    std::cout << "=== Payment Tree Unit Tests ===" << std::endl;
    testBasicLock();
    testAncestorBlocking();
    testUnlockOwnership();
    testUpgradeLock();
    testUpgradeLockMixedUsers();
    testUpgradeLockNoDescendants();
    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}