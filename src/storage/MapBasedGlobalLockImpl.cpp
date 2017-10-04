#include "MapBasedGlobalLockImpl.h"

namespace Afina {
namespace Backend {
    
// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value)
{
    std::hash<std::string> hash_function;
    size_t str_hash = hash_function(key) % 1000;

    if  (storage.size() >= _max_size)
    {
        storage[str_hash] = value;
    }
    else
    {
        storage.pop_back();
        storage[str_hash] = value;
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value)
{
    std::hash<std::string> hash_function;
    size_t str_hash = hash_function(key) % 1000;

    if (!hash_function(storage[str_hash]))
    {
        storage[str_hash] = value;
        return true;
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value)
{
    std::hash<std::string> hash_function;
    size_t str_hash = hash_function(key) % 1000;

    if(hash_function(storage[str_hash]))
    {
        storage[str_hash] = value;
        return true;
    };
return false;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key)
{
    std::hash<std::string> hash_function;
    size_t str_hash = hash_function(key) % 1000;

    if(hash_function(storage[str_hash]))
    {
        std::string val = storage[str_hash];
        storage.erase(std::remove(storage.begin(), storage.end(), val), storage.end());
        return true;
    }
    return false;
    
}

// See MapBasedGlobalLockImpl.h
    bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const
{
    std::hash<std::string> hash_function;
    size_t str_hash = hash_function(key) % 1000;

    if(std::find(storage.begin(), storage.end(), value) != storage.end())
    {
        value = storage[str_hash];
        return true;
    }
    else
        return false;
    
}

} // namespace Backend
} // namespace Afina
