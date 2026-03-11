#include "PodArray.hh"

#include <utility>

PodArray::PodArray(size_t elemSize):
    size(0), cap(0), elemSize(elemSize) {}


PodArray::PodArray(const PodArray &ot): size(ot.size), cap(ot.size), elemSize(ot.elemSize) {
    data = new char[cap];
    memcpy(data, ot.data, size * elemSize);
};

PodArray& PodArray::operator=(const PodArray& ot) {
    auto tmp = ot;
    swap(tmp);
    return *this;
};

PodArray::PodArray(PodArray &&ot):
      data	  (std::exchange(ot.data, nullptr))
    , size	  (std::exchange(ot.size, 0))
    , cap 	  (std::exchange(ot.cap,  0))
    , elemSize(std::exchange(ot.elemSize, 0) ) {
};

PodArray& PodArray::operator=(PodArray &&ot) {
    auto tmp = std::move(ot);
    swap(tmp);
    return *this;
};

PodArray::~PodArray() {
    delete[] data;
}

void PodArray::swap(PodArray& ot) noexcept {
    using std::swap;
    swap(data, ot.data);
    swap(size, ot.size);
    swap(cap, ot.cap);
    swap(elemSize, ot.elemSize);
}

void PodArray::push(const char *item) {
    [[ unlikely ]]
    if (size == cap) {
        size_t newCap = cap * 2;
        if (newCap == 0) {
            newCap = 1;
        }
        char* newData = new char[newCap * elemSize];
        std::memcpy(newData, data, cap * elemSize);
        delete[] data;
        data = newData;
        cap = newCap;
    }
    const size_t offset = elemSize * (size++);
    const auto itemPtr = item;
    std::memcpy(data + offset, itemPtr, elemSize);
}

void PodArray::popBack() {
    if (size > 0) {
        size--;
    }
}

size_t PodArray::getSize() const {
    return size;
}

size_t PodArray::getCap() const {
    return cap;
}

size_t PodArray::getElemSize() const {
    return elemSize;
}

