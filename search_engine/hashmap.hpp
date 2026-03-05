#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include "vector.hpp"
#include <string>
#include <stdexcept>

inline size_t string_hash(const std::string& str) {
    size_t hash = 0;
    const size_t prime = 31;
    for (char c : str) {
        hash = hash * prime + static_cast<size_t>(c);
    }
    return hash;
}

template<typename K, typename V>
struct HashMapNode {
    K key;
    V val;
    bool occupied;

    HashMapNode() : occupied(false) {}
    HashMapNode(const K& k, const V& v) : key(k), val(v), occupied(true) {}
};

template<typename K, typename V>
class HashMap {
private:
    Vector<HashMapNode<K, V>> buckets_;
    size_t size_;
    size_t capacity_;
    static constexpr double LOAD_FACTOR_THRESHOLD = 0.75; 

    size_t hash_index(const K& key) const {
        static_assert(std::is_same<K, std::string>::value, "Key type must be std::string for hash function");
        return string_hash(key) % capacity_;
    }

    size_t find_index(const K& key, bool for_insertion = false) const {
        size_t index = hash_index(key);
        size_t orig_idx = index;

        do {
            if (!buckets_[index].occupied) {
                if (for_insertion) {
                    return index;
                } else {
                    return capacity_;
                }
            }
            if (buckets_[index].key == key) {
                return index;
            }

            index = (index + 1) % capacity_;
        } while (index != orig_idx);

        return capacity_;
    }

    void insert_rehash(K&& key, V&& val) {
        size_t index = hash_index(key);
        while (buckets_[index].occupied) {
            index = (index + 1) % capacity_;
        }

        buckets_[index] = HashMapNode<K, V>(std::move(key), std::move(val));
        size_++;
    }

    void rehash() {
        Vector<HashMapNode<K, V>> old_buckets = std::move(buckets_);
        size_t old_capacity = capacity_;
        capacity_ *= 2;
        size_t old_size = size_;
        size_= 0;

        buckets_ = Vector<HashMapNode<K, V>>(capacity_);
        for (size_t i = 0; i < capacity_; ++i) {
            buckets_.push_back(HashMapNode<K, V>());
        }

        for (size_t i = 0; i < old_capacity; ++i) {
            if(old_buckets[i].occupied) {
                insert_rehash(std::move(old_buckets[i].key), std::move(old_buckets[i].val));
            }
        }

         if (size_ != old_size) {
                throw std::runtime_error("Rehash failed: size mismatch.");
        }
    }
public:

    explicit HashMap(size_t init_capacity = 16) : size_(0), capacity_(init_capacity) {
        buckets_ = Vector<HashMapNode<K, V>>(capacity_);
        for (size_t i = 0; i < capacity_; ++i) {
            buckets_.push_back(HashMapNode<K, V>());
        }
    }

    ~HashMap() = default;

    V& operator[](const K& key) {
        if (static_cast<double>(size_) / capacity_ >= LOAD_FACTOR_THRESHOLD) {
            rehash();
        }

        size_t index = find_index(key, true);
        if (index >= capacity_) {
            throw std::runtime_error("Hash table is full after rehashing!");
        }

        if (!buckets_[index].occupied) {
            buckets_[index] = HashMapNode<K, V>(key, V{});
            size_++;
        }

        return buckets_[index].val;
    }

    bool contains(const K& key) const {
        size_t index = find_index(key);
        return index < capacity_ && buckets_[index].occupied;
    }
    
    const V* find(const K& key) const {
        size_t index = find_index(key);
        if (index < capacity_ && buckets_[index].occupied) {
            return &buckets_[index].val;
        }
        return nullptr;
    }

    V* find(const K& key) {
        size_t index = find_index(key);
        if (index < capacity_ && buckets_[index].occupied) {
            return &buckets_[index].val;
        }
        return nullptr;
    }

    size_t size() const {
        return size_;
    }

    size_t capacity() const {
        return capacity_;
    }

    void get_all_pairs(Vector<K>& keys, Vector<V>& vals) const {
        keys.clear();
        vals.clear();
        for (size_t i = 0; i < capacity_; ++i) {
            if (buckets_[i].occupied) {
                keys.push_back(buckets_[i].key);
                vals.push_back(buckets_[i].val);
            }
        }
    }
};

#endif