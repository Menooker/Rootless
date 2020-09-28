#pragma once
#include <unordered_map>
#include <mutex>
#include <memory>

struct entry_base
{
    virtual ~entry_base() = default;
};

struct concurrent_hash_map_owned
{
    std::mutex lock;
    std::unordered_map<void *, std::unique_ptr<entry_base>> map;
    void set(void *k, std::unique_ptr<entry_base> v)
    {
        std::lock_guard<std::mutex> guard(lock);
        map.insert(std::make_pair(k, std::move(v)));
    }
    entry_base *get(void *k)
    {
        std::lock_guard<std::mutex> guard(lock);
        auto itr = map.find(k);
        if (itr != map.end())
        {
            return itr->second.get();
        }
        return nullptr;
    }

    void remove(void *k)
    {
        std::lock_guard<std::mutex> guard(lock);
        map.erase(k);
    }
};

struct concurrent_hash_map
{
    std::mutex lock;
    std::unordered_map<void *, void *> map;
    void set(void *k, void *v)
    {
        std::lock_guard<std::mutex> guard(lock);
        map.insert(std::make_pair(k, std::move(v)));
    }
    void *get(void *k)
    {
        std::lock_guard<std::mutex> guard(lock);
        auto itr = map.find(k);
        if (itr != map.end())
        {
            return itr->second;
        }
        return nullptr;
    }
};