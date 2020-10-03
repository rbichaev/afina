#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <utility>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {

private:

    void ClearFromEnd(const std::size_t size_of_new);

    // LRU cache node
    using lru_node = struct lru_node {
        const std::string key;
        std::string value;
        std::unique_ptr<lru_node> prev;
        lru_node *next = nullptr;

        lru_node(const std::string &k, const std::string &v) : key(k), value(v) {}
    };

    void MakeFirst(lru_node *current_node);

    // Maximum number of bytes could be stored in this cache.
    // i.e all (keys+values) must be less the _max_size
    std::size_t _max_size;

    std::size_t _current_size = 0;

    // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
    // element that wasn't used for longest time.
    //
    // List owns all nodes
    std::unique_ptr<lru_node> _lru_head;

    // указатель на последний элемент двусвязного списка,
    // чтобы знать, что удалять первым, в случае нехватки памяти
    lru_node *_last_node = nullptr;

    // Index of nodes from list above, allows fast random access to elements by lru_node#key
    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>> _lru_index;

public:
    SimpleLRU(size_t max_size = 1024) : _max_size(max_size) {}

    ~SimpleLRU()
    {
        _lru_index.clear();

        std::unique_ptr<lru_node> node_ptr;
        std::unique_ptr<lru_node> prev_node_ptr;

        node_ptr = std::move(_lru_head);

        for (; node_ptr.get() != nullptr;)
        {
            prev_node_ptr = std::move(node_ptr.get()->prev);
            node_ptr.reset();
            node_ptr = std::move(prev_node_ptr);
        }
    }

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    bool PutIfAbsent(const std::string &key, const std::string &value, std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator &key_iterator);

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    bool Set(const std::string &key, const std::string &value, std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator &key_iterator);

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

    using iterator_type = std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_SIMPLE_LRU_H
