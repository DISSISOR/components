#include "World.hh"
Property::Property(const std::string& name, size_t elemSize): name(name), dataArray(elemSize)  {
}

Property::Property(Property &&ot):
    name(std::move(ot.name))
    , dataArray(std::move(ot.dataArray))
    , indices  (std::move(ot.indices)) {
}

bool Property::hasForEntity(EntityIdx eIdx) {
    auto [pageIdx, offset] = pageAndOffsetFromIdx(eIdx);
    if (pageIdx >= indices.size()) return false;
    auto& page = indices[pageIdx];
    return page.data[offset] >= 0;
}

bool Property::hasForEntity(EntityHandle e) {
    return hasForEntity(e.offset);
}

Property::Page& Property::createOrGetPage(size_t index) {
    auto pageIdx = index / pageSize;
    while ( pageIdx + 1 > indices.size() ) {
        indices.emplace_back();
    }
    return indices[pageIdx];
}


void Property::deleteForEntity(EntityIdx eIdx, std::vector<char>* tmpBuffer) {
    auto [pageIdx, offset] = pageAndOffsetFromIdx(eIdx);
    auto& page = createOrGetPage(eIdx);
    Index idx = page.data[offset];
    page.data[offset] = -1;
    tmpBuffer->resize(dataArray.getElemSize());

    // swap
    auto* tmp = tmpBuffer->data();
    Index lastIdx = dataArray.getSize() - 1;
    memcpy(tmp, dataArray.ptrToNthElem(lastIdx), dataArray.getElemSize());
    memcpy(dataArray.ptrToNthElem(lastIdx), dataArray.ptrToNthElem(idx),  dataArray.getElemSize());
    memcpy(dataArray.ptrToNthElem(idx), tmp, dataArray.getElemSize());

    dataArray.popBack();

    auto [pageIdxLast, offsetLastIdx] = pageAndOffsetFromIdx(revIndices[lastIdx]);
    auto& lastPage = indices[pageIdxLast];
    lastPage.data[offsetLastIdx] = idx;
    std::swap(revIndices[lastIdx], revIndices[idx]);
    revIndices.pop_back();
}

void Property::deleteForEntity(EntityHandle e, std::vector<char>* tmpBuffer) {
    deleteForEntity(e.offset, tmpBuffer);
}

Property::Page::operator bool() const {
    return data != nullptr;
}
Property::Page::Page(): data(nullptr) {}
Property::Page::~Page() {
    delete[] data;
}
Property::Page::Page(Page&& ot):
    data(std::exchange(ot.data, nullptr)) {}

EntityHandle World::newEntity() {
    if (freeEntity < 0)
    {
        entities.push_back({-1, 0});
        return { static_cast<int>(entities.size()) - 1, 0 };
    }
    auto& e = entities[freeEntity];
    EntityHandle ret = { freeEntity, e.generation };
    freeEntity = e.free;
    return ret;
}

void World::deleteEntity(EntityIdx eIdx) {
    for (auto &prop: properties) {
        if (!prop.hasForEntity(eIdx)) continue;
        prop.deleteForEntity(eIdx, &tmpBuffer);
    }
    returnEntity(eIdx);
}

void World::returnEntity(EntityIdx eIdx) {
    auto& e = entities[eIdx];
    e.generation++;
    e.free = freeEntity;
    freeEntity = eIdx;
}

