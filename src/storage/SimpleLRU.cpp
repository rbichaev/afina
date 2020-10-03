#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

// удаляем последние элементы списка, пока не влезет новый
void SimpleLRU::ClearFromEnd(const std::size_t size_of_new)
{
    while (size_of_new > _max_size - _current_size)
    {
        if (_last_node->next)
        {
            _lru_index.erase(_last_node->key);
            _current_size -= _last_node->key.size() + _last_node->value.size();
            lru_node *tmp_ptr = _last_node->next;
            tmp_ptr->prev.reset();
            _last_node = tmp_ptr;
        }
        else
        {
            _lru_index.erase(_last_node->key);
            _current_size -= _last_node->key.size() + _last_node->value.size();
            _lru_head.reset();
            _last_node = nullptr;
        }
    }
}

// переставляем элемент в начало списка
void SimpleLRU::MakeFirst(lru_node *current_node)
{
    // "удаляем" его из середины списка
    // если это головная вершина, то ничего не переставляем
    if (_lru_head.get() != current_node)
    {
        // у нынешней головной структуры нет next - нужно его поставить
        if (current_node->prev)
        {
            _lru_head->next = current_node->prev->next;
        }
        else
        {
            _lru_head->next = _last_node;
        }

        // (если это не головная структура, то у нее есть next)
        // если у следующего за рассматриваемым нет следующего, то
        // им станет рассматриваемый
        if (!(current_node->next)->next)
        {
            (current_node->next)->next = ((current_node->next)->prev).get();
        }

        // нельзя терять прошлый элемент
        if (current_node->prev)
        {
            current_node->prev->next = current_node->next;
        }
        // если нет прошлого, то этот сейчас последний, и
        // тогда последним станет следующий за ним, а он есть
        // так как мы в этом if'е => делаем его последним
        else
        {
            _last_node = current_node->next;
        }

        // создаем временный умный указатель для осуществления перестановки
        std::unique_ptr<lru_node> tmp_ptr;
        tmp_ptr = std::move(current_node->prev);

        // если это не головная вершина, то у нее есть next
        current_node->prev = std::move(_lru_head);
        _lru_head = std::move(current_node->next->prev);
        current_node->next->prev = std::move(tmp_ptr);

        // у головной вершины не может быть следующей
        current_node->next = nullptr;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value)
{
    // размер нового элемента
    std::size_t size_of_new = key.size() + value.size();

    // влезет вообще или нет
    if (size_of_new > _max_size)
    {
        return false;
    }

    bool result = false;

    auto key_iterator = _lru_index.find(key);

    // вызываем PutIfAbsent или Set в зависимости от того,
    // есть ли нужный ключ в двусвязном списке
    if (key_iterator == _lru_index.end())
    {
        result = PutIfAbsent(key, value, key_iterator);
    }
    else
    {
        result = Set(key, value, key_iterator);
    }

    return result;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value)
{
    // размер нового элемента
    std::size_t size_of_new = key.size() + value.size();

    // влезет вообще или нет
    if (size_of_new > _max_size)
    {
        return false;
    }

    // влезет или нет с учетом уже имеющихся
    if (size_of_new > _max_size - _current_size)
    {
        this->ClearFromEnd(size_of_new);
    }

    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator key_iterator;

    key_iterator = _lru_index.find(key);

    // если ключ уже есть, возвращаем false
    if (key_iterator != _lru_index.end())
    {
        return false;
    }

    // создаем новую структуру-вершину и инициализируем ее
    std::unique_ptr<lru_node> new_node_ptr(new lru_node(key, value));

    // если уже есть другие элементы:
    if (_lru_head)
    {
        _lru_head->next = new_node_ptr.get();
        new_node_ptr->prev = std::move(_lru_head);
    }
    // других элементов нет - этот первый:
    else
    {
        _last_node = new_node_ptr.get();
    }

    // добавляем пару (ключ, структура) в дерево
    _lru_index.insert({new_node_ptr->key, *new_node_ptr});

    _lru_head = std::move(new_node_ptr);

    _current_size += key.size() + value.size();

    return true;
}

// перегрузка прошлого метода с передачей итератора
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value, std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator &key_iterator)
{
    // размер нового элемента
    std::size_t size_of_new = key.size() + value.size();

    // влезет вообще или нет
    if (size_of_new > _max_size)
    {
        return false;
    }

    // влезет или нет с учетом уже имеющихся
    if (size_of_new > _max_size - _current_size)
    {
        this->ClearFromEnd(size_of_new);
    }

    // если ключ уже есть, возвращаем false
    if (key_iterator != _lru_index.end())
    {
        return false;
    }

    // создаем новую структуру-вершину и инициализируем ее
    std::unique_ptr<lru_node> new_node_ptr(new lru_node(key, value));

    // если уже есть другие элементы:
    if (_lru_head)
    {
        _lru_head->next = new_node_ptr.get();
        new_node_ptr->prev = std::move(_lru_head);
    }
    // других элементов нет - этот первый:
    else
    {
        _last_node = new_node_ptr.get();
    }

    // добавляем пару (ключ, структура) в дерево
    _lru_index.insert({new_node_ptr->key, *new_node_ptr});

    _lru_head = std::move(new_node_ptr);

    _current_size += key.size() + value.size();

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value)
{
    // размер нового элемента
    std::size_t size_of_new = key.size() + value.size();

    // влезет вообще или нет
    if (size_of_new > _max_size)
    {
        return false;
    }

    std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator key_iterator;

    key_iterator = _lru_index.find(key);

    // если ключа нет, возвращаем false
    if (key_iterator == _lru_index.end())
    {
        return false;
    }

    // сначала ставим элемент в начало, а потом освобождаем место:

    lru_node *current_node = &(key_iterator->second).get();

    // переносим элемент в начало двусвязного списка:
    if (_lru_head.get() != current_node)
    {
        this->MakeFirst(current_node);
    }

    // сразу убираем размер имеющегося value, чтобы не делать лишних удалений
    _current_size -= current_node->value.size();

    if (size_of_new > _max_size - _current_size)
    {
        this->ClearFromEnd(size_of_new);
    }

    current_node->value = value;
    _current_size += value.size();

    return true;
}

// перегрузка прошлого метода с передачей итератора
bool SimpleLRU::Set(const std::string &key, const std::string &value, std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>::iterator &key_iterator)
{
    // размер нового элемента
    std::size_t size_of_new = key.size() + value.size();

    // влезет вообще или нет
    if (size_of_new > _max_size)
    {
        return false;
    }

    // если ключа нет, возвращаем false
    if (key_iterator == _lru_index.end())
    {
        return false;
    }

    // сначала ставим элемент в начало, а потом освобождаем место:

    lru_node *current_node = &(key_iterator->second).get();

    // переносим элемент в начало двусвязного списка:
    if (_lru_head.get() != current_node)
    {
        this->MakeFirst(current_node);
    }

    // сразу убираем размер имеющегося value, чтобы не делать лишних удалений
    _current_size -= current_node->value.size();

    if (size_of_new > _max_size - _current_size)
    {
        this->ClearFromEnd(size_of_new);
    }

    current_node->value = value;
    _current_size += value.size();

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key)
{
    auto key_iterator = _lru_index.find(key);
    std::unique_ptr<lru_node> current_node_ptr;

    // если ключа нет, возвращаем false
    if (key_iterator == _lru_index.end())
    {
        return false;
    }

    lru_node *current_node = &(key_iterator->second).get();

    // чтобы не потерять unique_ptr на удаляемую структуру
    // в ходе перестановок указателей, временно сохраним его
    if (current_node->next)
    {
        current_node_ptr = std::move(current_node->next->prev);
    }
    else
    {
        current_node_ptr = std::move(_lru_head);
    }

    // "удаляем" из списка - отдельно рассматриваем наличие
    // прошлого и следующего элемента в списке:

    // STEP 1 - наличие прошлого:
    if (current_node->prev)
    {
        current_node->prev->next = current_node->next;
    }
    // если это последний, то ставим указатель на следующий элемент в качестве
    // последнего (не важно nullptr это или нет)
    else
    {
        _last_node = current_node->next;
    }

    // STEP 2 - наличие следующего:
    if (current_node->next)
    {
        if (current_node->prev)
        {
            current_node->next->prev = std::move(current_node->prev);
        }
    }
    else
    {
        _lru_head = std::move(current_node->prev);
    }

    _current_size -= key.size() + current_node->value.size();

    // удаляем из дерева
    _lru_index.erase(key);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value)
{
    auto key_iterator = _lru_index.find(key);

    // если ключа нет, возвращаем false
    if (key_iterator == _lru_index.end())
    {
        return false;
    }

    lru_node *current_node = &(key_iterator->second).get();
    value = current_node->value;

    // переносим элемент в начало двусвязного списка:
    if (_lru_head.get() != current_node)
    {
        this->MakeFirst(current_node);
    }

    return true;
}

} // namespace Backend
} // namespace Afina
