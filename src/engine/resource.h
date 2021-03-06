#ifndef GAMEFRIENDS_RESOURCE_H
#define GAMEFRIENDS_RESOURCE_H

#include "filesystem.h"
#include "foundation/exception.h"
#include "foundation/prerequest.h"
#include <memory>
#include <unordered_map>
#include <utility>
#include <string>

GF_NAMESPACE_BEGIN

class ResourceException : public FileException
{
public:
    explicit ResourceException(const std::string& msg)
        : FileException(msg) {}
};

class CodecException : public ResourceException
{
public:
    explicit CodecException(const std::string& msg)
        : ResourceException(msg) {}
};

class MeshLoadException : public ResourceException
{
public:
    explicit MeshLoadException(const std::string& msg)
        : ResourceException(msg) {}
};

class TextureLoadException : public ResourceException
{
public:
    explicit TextureLoadException(const std::string& msg)
        : ResourceException(msg) {}
};

class MaterialLoadException : public ResourceException
{
public:
    explicit MaterialLoadException(const std::string& msg)
        : ResourceException(msg) {}
};

class ShadeModelLoadException : public ResourceException
{
public:
    explicit ShadeModelLoadException(const std::string& msg)
        : ResourceException(msg) {}
};

/*
class SoundLoadException : public ResourceException
{
public:
    explicit SoundLoadException(const std::string& msg)
        : ResourceException(msg) {}
};
*/

class Resource
{
private:
    EnginePath path_;
    std::string osPath_;
    bool ready_;

public:
    explicit Resource(const EnginePath& path);
    virtual ~Resource() = default;

    EnginePath path() const;
    std::string osPath() const;

    bool ready() const;
    void load();
    void unload();

private:
    virtual bool loadImpl() = 0;
    virtual void unloadImpl() = 0;
};

template <class T>
class ResourceInterface
{
    template <class U> friend class ResourceInterface;

private:
    std::weak_ptr<T> ptr_;
    std::shared_ptr<T> inplace_;

public:
    static ResourceInterface create(const std::weak_ptr<T>& p)
    {
        ResourceInterface ptr;
        ptr.ptr_ = p;
        return ptr;
    }

    static ResourceInterface inplace(const std::shared_ptr<T>& p)
    {
        ResourceInterface ptr;
        ptr.ptr_ = p;
        ptr.inplace_ = p;
        return ptr;
    }

    ResourceInterface() = default;

    ResourceInterface(std::nullptr_t)
        : ResourceInterface()
    {
    }

    template <class U>
    ResourceInterface(const ResourceInterface<U>& that)
        : ptr_(that.ptr_)
        , inplace_(that.inplace_)
    {
    }

    template <class U>
    ResourceInterface(ResourceInterface<U>&& that)
        : ptr_(std::move(that.ptr_))
        , inplace_(std::move(that.inplace_))
    {
    }

    template <class U>
    ResourceInterface& operator=(const ResourceInterface<U>& that)
    {
        ptr_ = that.ptr_;
        inplace_ = that.inplace_;
        return *this;
    }

    template <class U>
    ResourceInterface& operator=(ResourceInterface<U>&& that)
    {
        ptr_ = std::move(that.ptr_);
        inplace_ = std::move(that.inplace_);
        return *this;
    }

    T* get() const
    {
        if (auto p = ptr_.lock())
        {
            return p.get();
        }
        return nullptr;
    }

    T& operator*() const
    {
        check(get());
        return *get();
    }

    T* operator->() const
    {
        return get();
    }

    operator bool() const
    {
        return !!get();
    }

    bool useable() const
    {
        return get() && get()->ready();
    }
};

class ResourceManager
{
private:
    std::unordered_map<EnginePath, std::shared_ptr<Resource>> resourceMap_;

public:
    void startup();
    void shutdown();

    template <class T, class... Args>
    ResourceInterface<T> create(const EnginePath& path, Args&&... args)
    {
        const auto it = resourceMap_.find(path);
        if (it != std::cend(resourceMap_))
        {
            nullptr;
        }

        const auto r = std::make_shared<T>(path, std::forward<Args>(args)...);
        resourceMap_.emplace(path, r);
        return ResourceInterface<T>::create(r);
    }

    template <class T>
    ResourceInterface<T> get(const EnginePath& path)
    {
        const auto it = resourceMap_.find(path);
        if (it == std::cend(resourceMap_))
        {
            return nullptr;
        }
        return ResourceInterface<T>::create(std::dynamic_pointer_cast<T>(it->second));
    }

    template <class T, class... Args>
    ResourceInterface<T> obtain(const EnginePath& path, Args&&... args)
    {
        const auto p = get<T>(path);
        if (!p)
        {
            return create<T>(path, std::forward<Args>(args)...);
        }
        return p;
    }

    void destroy(const EnginePath& path);
    void clear();
};

extern ResourceManager resourceManager;

GF_NAMESPACE_END

#endif