#include "resource.h"

GF_NAMESPACE_BEGIN

Resource::Resource(const std::string& path)
    : path_(path)
    , ready_(false)
{
}

std::string Resource::path() const
{
    return path_;
}

bool Resource::ready() const
{
    return ready_;
}

void Resource::load()
{
    if (!ready_)
    {
        loadImpl();
        ready_ = true;
    }
}

void Resource::unload()
{
    if (ready_)
    {
        unloadImpl();
        ready_ = false;
    }
}

void ResourceTable::destroy(const std::string& path)
{
    const auto it = resourceMap_.find(path);
    if (it != std::cend(resourceMap_))
    {
        it->second->unload();
        resourceMap_.erase(it);
    }
}

void ResourceTable::clear()
{
    while (!resourceMap_.empty())
    {
        destroy(std::cbegin(resourceMap_)->first);
    }
}

ResourceTable resourceTable;

GF_NAMESPACE_END