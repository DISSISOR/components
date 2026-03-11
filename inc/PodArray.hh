#ifndef PodArray_hh_INCLUDED
#define PodArray_hh_INCLUDED

#include <cstdlib>
#include <cstring>
#include <span>

class PodArray {
public:
    PodArray(size_t elemSize);
    PodArray(const PodArray &ot);
    PodArray& operator=(const PodArray& ot);
    PodArray(PodArray &&ot);
    PodArray& operator=(PodArray &&ot);

    ~PodArray();

    void swap(PodArray& ot) noexcept;

    void push(const char *item);

    template <typename T>
    void push(const T& item) {
        push(reinterpret_cast<const char*>(&item));
    }

    size_t getSize() const;
    size_t getCap() const;
    size_t getElemSize() const;
    char* getData() {
        return data;
    }

    template<typename T>
    std::span<const T> getItems() const {
        return { reinterpret_cast<const T*>(data), size };
    }

    template<typename T>
    std::span<T> getItems() {
        return { reinterpret_cast<T*>(data), size };
    }

    char* ptrToNthElem(size_t n) {
        return data + n * elemSize;
    }

    void popBack();

private:
    char* data = nullptr;
    size_t size;
    size_t cap;

    size_t elemSize;
};

#endif // PodArray_hh_INCLUDED
