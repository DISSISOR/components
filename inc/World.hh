#ifndef World_hh_INCLUDED
#define World_hh_INCLUDED

#include <string>
#include <cstdlib>
#include <utility>
#include <vector>

#include "PodArray.hh"

using EntityIdx = int;
struct EntityHandle {
    EntityIdx offset;
    int generation;
};

struct Entity {
    int free; // free list
    int generation;
};

class Property {
public:
    Property(const std::string& name, size_t elemSize);

    Property(const Property& ot) = delete;
    Property(Property &&ot);

    template<typename T>
    std::span<T> getData() {
        return dataArray.getItems<T>();
    }

    template<typename T>
    std::span<const T> getData() const {
        return dataArray.getItems<T>();
    }

    template<typename T>
    void addForEntity(EntityIdx eIdx, const T& p) {
        const size_t idx = dataArray.getSize();
        dataArray.push(p);
        revIndices.push_back(eIdx);
        auto [pageIdx, offset] = pageAndOffsetFromIdx(eIdx);
        auto& page = createOrGetPage(eIdx);
        if (!page) {
            page.data = new Index[pageSize];
            std::fill(page.data, page.data + pageSize, -1);
        }
        page.data[offset] = idx;
    }

    template<typename T>
    void addForEntity(EntityHandle e, const T& p) {
        addForEntity<T>(e.offset, p);
    }

    void deleteForEntity(EntityHandle e, std::vector<char>* tmpBuffer);
    void deleteForEntity(EntityIdx eIdx, std::vector<char>* tmpBuffer);

    bool hasForEntity(EntityIdx eIdx);
    bool hasForEntity(EntityHandle e);

    template <typename T>
    T* getForEntity(EntityIdx idx) {
        auto [pageIdx, offset] = pageAndOffsetFromIdx(idx);
        if (pageIdx >= indices.size()) return nullptr;
        auto& page = indices[pageIdx];
        Index dataIndex = page.data[offset];
        if (dataIndex < 0) return nullptr;
        return &dataArray.getItems<T>()[dataIndex];
    }

    template <typename T>
    T* getForEntity(EntityHandle e) {
        return getForEntity<T>(e.offset);
    }

private:
using Index = int;
public:
    template <typename T>
    class Iterator {
    public:
        Iterator() = delete;
        Iterator(Property* prop): prop(prop), idx(0) {}
        bool hasNext() const {
            return static_cast<Index>(prop->dataArray.getSize()) > idx;
        }
        void next() {
            idx++;
        }
        std::pair<T*, EntityIdx> get() {
            return { &prop->getData<T>()[idx], prop->revIndices[idx] };
        }

        std::pair<const T*, EntityIdx> get() const  {
            return { &prop->getData<T>()[idx], prop->revIndices[idx] };
        }
    private:
        Property* prop;
        Index idx;
    };
    template <>
    class Iterator<void> {
    public:
        Iterator() = delete;
        Iterator(Property* prop): prop(prop), idx(0) {}
        bool hasNext() const {
            return static_cast<Index>(prop->dataArray.getSize()) > idx;
        }
        void next() {
            idx++;
        }

        EntityIdx get() const  {
            return prop->revIndices[idx];
        }
    private:
        Property* prop;
        Index idx;
    };
    template<typename T>
    friend class Iterator;

    template<typename T>
    Iterator<T> getIterator() {
        return { this };
    }

private:
    std::string name;
    PodArray dataArray;
    static constexpr size_t pageSize = 4096;
    static constexpr std::pair<size_t, size_t> pageAndOffsetFromIdx(size_t index) {
        return { index / pageSize, index % pageSize };
    }
    struct Page {
        Index* data = nullptr;
        operator bool() const;
        Page();
        Page(const Page& ot) = delete;
        ~Page();
        Page(Page&& ot);
    };
    std::vector<Page> indices;
    std::vector<Index> revIndices; // value at propety index is entity id
    Page& createOrGetPage(size_t index);
};

class PropertyHandle {
public:
    PropertyHandle(int offset): offset(offset) {}
    int getOffset() const { return offset; }
private:
    int offset;
};

class World {
public:
    PropertyHandle addProperty(const std::string& name, size_t elemSize);
    EntityHandle newEntity();

    Property& property(PropertyHandle h);
    void deleteEntity(EntityIdx eIdx);

private:
    void returnEntity(EntityIdx eIdx);
    std::vector<Property> properties;
    std::vector<Entity> entities;
    std::vector<char> tmpBuffer;
    int freeEntity = -1;
    PropertyHandle addProperty(Property &&comps);
};

#endif // World_hh_INCLUDED
