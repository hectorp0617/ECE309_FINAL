// include/util/lru_cache.h
//
// YOU IMPLEMENT (this whole file; it is a template). Spec §4.2.
//
// A fixed-capacity cache that evicts the Least Recently Used entry.
// Both get() and put() must be O(1) (expected), so it is built from two
// structures that point at the same nodes:
//
//   std::unordered_map<Key, Node*>   finds a key's node in O(1)
//   a doubly linked list of Nodes    keeps recency order:
//        head_ = most recently used ... tail_ = least recently used
//
// Rules for this file:
//   * Write the doubly linked list yourself (Node with prev/next pointers,
//     new/delete inside this class only). Do NOT use std::list, std::deque
//     or std::forward_list (don't even #include their headers), or a
//     std::vector you shift around (spec §7.3).
//   * std::unordered_map is allowed (and expected) for the key -> Node* index.
//   * No leaks, no double frees: ASan runs on every test.

#pragma once
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruCache {
public:
    struct Stats {
        std::size_t hits = 0;        // get() found the key
        std::size_t misses = 0;      // get() did not find the key
        std::size_t evictions = 0;   // put() removed the LRU entry to make room
    };

    // A cache holding at most `capacity` entries. Capacity 0 is legal: such a
    // cache never stores anything (every get() is a miss).
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

    // Frees every node.
    ~LruCache() { clear(); }

    // Copying a cache of raw-pointer nodes is not needed; forbid it.
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    // Hit: marks the entry most recently used, counts a hit, and returns a
    //      pointer to its value (valid until the next put/erase/clear).
    // Miss: counts a miss and returns nullptr.
    const Value* get(const Key& key) {
        auto it = map_.find(key);

        // If the key is not found, increment misses and return nullptr
        if (it == map_.end()) {
            stats_.misses++;
            return nullptr;

        // Clue: if the key is found, increment hits, move the node to the front of the list, and return a pointer to its value
        } else {
            stats_.hits++;
            move_to_front(it->second);
            return &(it->second->value);
        }

        (void)key;
        return nullptr;
    }

    // Key present: replaces its value and marks it most recently used (no
    //   eviction, no hit/miss counted).
    // Key absent: if the cache is full, evicts the least recently used entry
    //   (counting one eviction), then inserts the new entry as most recently
    //   used. With capacity 0, does nothing.
    void put(const Key& key, Value value) {

        if (capacity_ == 0) {
            return; // Do nothing if capacity is 0
        }

        auto it = map_.find(key);
        if (it != map_.end()) {
            // Key is present, update the value and move to front
            it->second->value = std::move(value);
            move_to_front(it->second);
        } else {
            // Key is absent, check if we need to evict
            if (map_.size() >= capacity_) {
                // Evict the least recently used entry (tail_)

                if (tail_) {
                    Node* victim = tail_;
                    unlink(victim);
                    map_.erase(victim->key);
                    delete victim;
                    stats_.evictions++;
                }
            }

            // Insert the new entry as most recently used
            Node* new_node = new Node{key, std::move(value), nullptr, nullptr};
            push_front(new_node);
            map_[key] = new_node;

        }
    }

    // True if the key is cached. Does NOT change recency or stats.
    bool contains(const Key& key) const {
        // TODO
        // Clue: this function is const, so map_[key] does not even compile
        // ("discards qualifiers"). Use map_.find(key) != map_.end() or
        // map_.count(key)

        auto it = map_.find(key);
        return it != map_.end();
    }

    // Removes the key if present (not an eviction). Returns whether it was.
    bool erase(const Key& key) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            Node* node_to_remove = it->second;
            unlink(node_to_remove);
            map_.erase(it);
            delete node_to_remove;
            return true;
        }
        return false;
    }

    // Removes every entry. Stats are kept.
    void clear() {
        // TODO: delete every node, empty the map, reset head_/tail_.
        // Clue: save n->next in a local BEFORE  delete n;  a loop like
        //   for (Node* n = head_; n; n = n->next) delete n;
        // reads n->next from freed memory.

        Node* current = head_;
        while (current) {
            Node* next_node = current->next;
            delete current;
            current = next_node;
        }
        head_ = nullptr;
        tail_ = nullptr;
        map_.clear();
    }

    std::size_t size() const noexcept { return map_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    Stats       stats() const noexcept { return stats_; }

    // Keys from most to least recently used, by walking the list from head_.
    // Used by the tests to check your recency order.
    std::vector<Key> keys_mru_to_lru() const {
        std::vector<Key> out;
        for (Node* n = head_; n != nullptr; n = n->next) out.push_back(n->key);
        return out;
    }

private:
    struct Node {
        Key   key;
        Value value;
        Node* prev;
        Node* next;
    };

    // ===== YOUR HELPERS AND MEMBERS =====
    // Suggested helpers: unlink(Node*), push_front(Node*), move_to_front(Node*).
    // Clue for unlink(n): four cases. If n has a prev, prev->next skips n,
    // otherwise n was head_ and head_ moves to n->next. Likewise, if n has
    // a next, next->prev skips n, otherwise n was tail_ and tail_ moves to
    // n->prev. Unlinking the only node leaves head_ == tail_ == nullptr.
    // Spec Appendix B lists every pointer write.


    // Unlinks n from the list, but does not delete it
    // Updates head_ and tail_ if needed. Does not change the map
    void unlink(Node* n) {
        if (n->prev) {
            n->prev->next = n->next;
        } else {
            head_ = n->next;
        }
        if (n->next) {
            n->next->prev = n->prev;
        } else {
            tail_ = n->prev;
        }
    }

    // Moves n to the front of the list (most recently used)
    void move_to_front(Node* n) {
        unlink(n);
        push_front(n);
    }

    // Pushes n to the front of the list (most recently used)
    // Does not change the map
    void push_front(Node* n) {
        n->prev = nullptr;
        n->next = head_;
        if (head_) {
            head_->prev = n;
        }
        head_ = n;
        if (!tail_) {
            tail_ = n;
        }
    }

    std::unordered_map<Key, Node*, Hash> map_;
    Node*       head_ = nullptr;
    Node*       tail_ = nullptr;
    std::size_t capacity_;
    Stats       stats_;
};
