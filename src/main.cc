#include <string>
#include <vector>
#include <span>
#include <cstring>
#include <utility>
#include <iostream>

#include <SDL3/SDL.h>

constexpr int keyCountMax = 512;
int keyCount = 0;
bool prevKeysState[keyCountMax];
const bool *keysState;

bool isKeyPressed(SDL_Scancode code) {
    return keysState[code];
}

bool isKeyJustPressed(SDL_Scancode code) {
    return keysState[code] && !prevKeysState[code];
}

class PodArray {
public:
    PodArray(size_t elemSize):
        size(0), cap(0), elemSize(elemSize) {}
    PodArray(const PodArray &ot): size(ot.size), cap(ot.size), elemSize(ot.elemSize) {
        data = new char[cap];
        memcpy(data, ot.data, size * elemSize);
    };
    PodArray& operator=(const PodArray& ot) {
        auto tmp = ot;
        swap(tmp);
        return *this;
    };
    PodArray(PodArray &&ot):
          data(std::exchange(ot.data, nullptr))
        , size(std::exchange(ot.size, 0))
        , cap (std::exchange(ot.cap,  0))
        , elemSize(std::exchange(ot.elemSize, 0) ){
    };
    PodArray& operator=(PodArray &&ot) {
        auto tmp = std::move(ot);
        swap(tmp);
        return *this;
    };

    ~PodArray() {
        delete[] data;
    }

    void swap(PodArray& ot) noexcept {
        using std::swap;
        swap(data, ot.data);
        swap(size, ot.size);
        swap(cap, ot.cap);
        swap(elemSize, ot.elemSize);
    }

    template <typename T>
    void push(const T& item) {
        [[ unlikely ]]
        if (size == cap) {
            size_t newCap = cap * 2;
            if (newCap == 0) {
                newCap = 1;
            }
            T* newData = new T[newCap];
            std::memcpy(newData, data, cap * elemSize);
            delete[] data;
            data = reinterpret_cast<char*>(newData);
            cap = newCap;
        }
        const size_t offset = sizeof(T) * (size++);
        const auto itemPtr = reinterpret_cast<const char*>(&item);
        std::memcpy(data + offset, itemPtr, sizeof(T));
    }

    size_t getSize() const {
        return size;
    }

    size_t getCap() const {
        return cap;
    }

    template<typename T>
    std::span<const T> getItems() const {
        return { reinterpret_cast<const T*>(data), size };
    }

    template<typename T>
    std::span<T> getItems() {
        return { reinterpret_cast<T*>(data), size };
    }

    void popBack() {
        if (size > 0) {
            size--;
        }
    }

private:
    char* data = nullptr;
    size_t size;
    size_t cap;

    size_t elemSize;
};

struct EntityHandle {
    int offset;
    int generation;
};

struct Entity {
    int free; // free list
    int generation;
};

class Property {
public:
    Property(const std::string& name, size_t elemSize): name(name), dataArray(elemSize)  {
    }

    Property(const Property& ot) = delete;
    Property(Property &&ot):
        name(std::move(ot.name))
        , dataArray(std::move(ot.dataArray))
        , indices  (std::move(ot.indices)) {
    }

    template<typename T>
    std::span<T> getData() {
        return dataArray.getItems<T>();
    }

    template<typename T>
    std::span<const T> getData() const {
        return dataArray.getItems<T>();
    }

    template<typename T>
    void addForEntity(EntityHandle e, const T& p) {
        const size_t idx = dataArray.getSize();
        dataArray.push(p);
        revIndices.push_back(e.offset);
        auto [pageIdx, offset] = pageAndOffsetFromIdx(e.offset);
        auto& page = createOrGetPage(e.offset);
        if (!page) {
            page.data = new Index[pageSize];
            std::fill(page.data, page.data + pageSize, -1);
        }
        page.data[offset] = idx;
    }

    template<typename T>
    void deleteForEntity(EntityHandle e) {
        auto [pageIdx, offset] = pageAndOffsetFromIdx(e.offset);
        auto& page = createOrGetPage(e.offset);
        Index idx = page.data[offset];
        page.data[offset] = -1;
        auto span = dataArray.getItems<T>();
        Index lastIdx = dataArray.getSize() - 1;
        std::swap(span[lastIdx], span[idx]);
        dataArray.popBack();

        auto [pageIdxLast, offsetLastIdx] = pageAndOffsetFromIdx(revIndices[lastIdx]);
        auto& lastPage = indices[pageIdxLast];
        lastPage.data[offsetLastIdx] = idx;
        std::swap(revIndices[lastIdx], revIndices[idx]);
        revIndices.pop_back();
    }

    template<typename T>
    bool hasForEntity(EntityHandle e) {
        auto [pageIdx, offset] = pageAndOffsetFromIdx(e.offset);
        if (pageIdx >= indices.size()) return false;
        auto& page = indices[pageIdx];
        return page.data[offset] >= 0;
    }

    template <typename T>
    T* getForEntity(EntityHandle e) {
        auto [pageIdx, offset] = pageAndOffsetFromIdx(e.offset);
        if (pageIdx >= indices.size()) return nullptr;
        auto& page = indices[pageIdx];
        Index idx = page.data[offset];
        if (idx < 0) return nullptr;
        return &dataArray.getItems<T>()[idx];
    }
private:
    std::string name;
    PodArray dataArray;
    using Index = int;
    static constexpr size_t pageSize = 4096;
    static constexpr std::pair<size_t, size_t> pageAndOffsetFromIdx(size_t index) {
        return { index / pageSize, index % pageSize };
    }
    struct Page {
        Index* data = nullptr;
        operator bool() const {
            return data != nullptr;
        }
        Page(): data(nullptr) {}
        Page(const Page& ot) = delete;
        ~Page() {
            delete[] data;
        }
        Page(Page&& ot):
            data(std::exchange(ot.data, nullptr)) {}
    };
    std::vector<Page> indices;
    std::vector<Index> revIndices; // value at propety index is entity id
    Page& createOrGetPage(size_t index) {
        auto pageIdx = index / pageSize;
        while ( pageIdx + 1 > indices.size() ) {
            indices.emplace_back();
        }
        return indices[pageIdx];
    }
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
    PropertyHandle addProperty(const std::string& name, size_t elemSize) {
        return addProperty(Property(name, elemSize));
    }
    EntityHandle newEntity() {
        if (freeEntity < 0)
        {
            entities.push_back({-1, 0});
            return { static_cast<int>(entities.size()) - 1, 0 };
        }
        auto& e = entities[freeEntity];
        e.generation++;
        EntityHandle ret = { freeEntity, e.generation };
        freeEntity = e.free;
        return ret;
    }

    void returnEntity(EntityHandle h) {
        auto& e = entities[h.offset];
        e.free = freeEntity;
        freeEntity = h.offset;
    }

    Property& property(PropertyHandle h) {
        return properties[h.getOffset()];
    }

private:
    std::vector<Property> properties;
    std::vector<Entity> entities;
    int freeEntity = -1;
    PropertyHandle addProperty(Property &&comps) {
        properties.emplace_back(std::move(comps));
        return { static_cast<int>(properties.size()) - 1  };
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    World world;
    auto pos = world.addProperty("pos", sizeof(SDL_FPoint));
    auto color = world.addProperty("color", sizeof(SDL_Color));
    auto e1 = world.newEntity();
    world.property(pos).addForEntity(e1, SDL_FPoint{0, 0});
    world.property(color).addForEntity(e1, SDL_Color{255, 0, 0, 255});

    auto e2 = world.newEntity();
    world.property(pos).addForEntity(e2, SDL_FPoint{100, 100});
    world.property(color).addForEntity(e2, SDL_Color{255, 255, 0, 255});

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_init: %s", SDL_GetError());
        return 1;
    }
    SDL_Window* win = nullptr;
    SDL_Renderer* rend = nullptr;
    if (!SDL_CreateWindowAndRenderer(
        "Hellope", 800, 600, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE, &win, &rend)) {
        SDL_Log("SDL_CreateWindowAndRenderer: %s", SDL_GetError());
        return 1;
    }

    keysState = SDL_GetKeyboardState(&keyCount);
    if (!keysState) {
        SDL_Log("SDL_GeyKeyboardState: %s", SDL_GetError());
        return 1;
    }
    if (keyCount > keyCountMax)
    {
        SDL_Log("Too many keys");
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event ev = {};
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = false;
            }
        }

        keysState = SDL_GetKeyboardState(&keyCount);
        if (isKeyJustPressed(SDL_SCANCODE_Q)) {
            running = false;
        }

        auto positions = world.property(pos).getData<SDL_FPoint>();
        for (const auto& position: positions) {
            (void)position;
        }

        std::memcpy(prevKeysState, keysState, keyCount);

        SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
        SDL_RenderClear(rend);
        SDL_RenderPresent(rend);
    }
}

