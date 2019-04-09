//
//  crawler.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/5.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include "crawler.h"

void MemorySnapshotCrawler::crawl()
{
    __sampler.begin("MemorySnapshotCrawler::crawl");
    initManagedTypes();
    crawlGCHandles();
    crawlStatic();
    __sampler.end();
}

using std::ifstream;
void MemorySnapshotCrawler::debug()
{
#if USE_ADDRESS_MIRROR
    ifstream fs;
    fs.open("/Users/larryhou/Documents/MemoryProfiler/address.bin", ifstream::in | ifstream::binary);
    
    int32_t size = 0;
    char *ptr = (char *)&size;
    fs.read(ptr, 4);
    
    __mirror = new address_t[size];
    fs.read((char *)__mirror, size * sizeof(address_t));
    fs.close();
#endif
}

void MemorySnapshotCrawler::initManagedTypes()
{
    __sampler.begin("initManagedTypes");
    Array<TypeDescription> &typeDescriptions = *__snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        TypeDescription &type = typeDescriptions[i];
        type.isUnityEngineObjectType = isSubclassOfManagedType(type, __snapshot.managedTypeIndex.unityengine_Object);
    }
    __sampler.end();
}

bool MemorySnapshotCrawler::isSubclassOfManagedType(TypeDescription &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    TypeDescription *iter = &type;
    while (iter->baseOrElementTypeIndex != -1)
    {
        iter = &__snapshot.typeDescriptions->items[iter->baseOrElementTypeIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

bool MemorySnapshotCrawler::isSubclassOfNativeType(PackedNativeType &type, int32_t baseTypeIndex)
{
    if (type.typeIndex == baseTypeIndex) { return true; }
    if (type.typeIndex < 0 || baseTypeIndex < 0) { return false; }
    
    PackedNativeType *iter = &type;
    while (iter->nativeBaseTypeArrayIndex != -1)
    {
        iter = &__snapshot.nativeTypes->items[iter->nativeBaseTypeArrayIndex];
        if (iter->typeIndex == baseTypeIndex) { return true; }
    }
    
    return false;
}

void MemorySnapshotCrawler::tryAcceptConnection(EntityConnection &ec)
{
    std::lock_guard<std::mutex> g(__mutex);
    
    if (ec.from >= 0)
    {
        ManagedObject &fromObject = managedObjects[ec.from];
        auto iter = __connectionVisit.find(fromObject.address);
        if (iter != __connectionVisit.end())
        {
            ManagedObject &toObject = managedObjects[ec.to];
            if (iter->second == toObject.address) { return; }
            __connectionVisit.insert(pair<address_t, address_t>(fromObject.address, toObject.address));
        }
    }
    
    if (ec.fromKind != ConnectionKind::None && ec.from >= 0)
    {
        auto key = getIndexKey(ec.fromKind, ec.from);
        auto iter = fromConnections.find(key);
        if (iter == fromConnections.end())
        {
            auto entity = fromConnections.insert(pair<int32_t, vector<int32_t> *>(key, new vector<int32_t>));
            iter = entity.first;
        }
        iter->second->push_back(ec.connectionArrayIndex);
    }
    
    if (ec.toKind != ConnectionKind::None && ec.to >= 0)
    {
        auto key = getIndexKey(ec.toKind, ec.to);
        auto iter = toConnections.find(key);
        if (iter == toConnections.end())
        {
            auto entity = toConnections.insert(pair<int32_t, vector<int32_t> *>(key, new vector<int32_t>));
            iter = entity.first;
        }
        iter->second->push_back(ec.connectionArrayIndex);
    }
}

int32_t MemorySnapshotCrawler::getIndexKey(ConnectionKind kind, int32_t index)
{
    return ((int32_t)kind << 28) + index;
}

int64_t MemorySnapshotCrawler::getConnectionKey(EntityConnection &ec)
{
    return (int64_t)getIndexKey(ec.fromKind, ec.from) << 32 | getIndexKey(ec.toKind, ec.to);
}

int32_t MemorySnapshotCrawler::findTypeAtTypeInfoAddress(address_t address)
{
    if (address == 0) {return -1;}
    __mutex.lock();
    if (__typeAddressMap.size() == 0)
    {
        Array<TypeDescription> &typeDescriptions = *__snapshot.typeDescriptions;
        for (auto i = 0; i < typeDescriptions.size; i++)
        {
            auto &type = typeDescriptions[i];
            __typeAddressMap.insert(pair<address_t, int32_t>(type.typeInfoAddress, type.typeIndex));
        }
    }
    __mutex.unlock();
    auto iter = __typeAddressMap.find(address);
    return iter != __typeAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findTypeOfAddress(address_t address)
{
    auto typeIndex = findTypeAtTypeInfoAddress(address);
    if (typeIndex != -1) {return typeIndex;}
    auto typePtr = __memoryReader->readPointer(address);
    if (typePtr == 0) {return -1;}
    auto vtablePtr = __memoryReader->readPointer(typePtr);
    if (vtablePtr != 0)
    {
        return findTypeAtTypeInfoAddress(vtablePtr);
    }
    return findTypeAtTypeInfoAddress(typePtr);
}

int32_t MemorySnapshotCrawler::findManagedObjectOfNativeObject(address_t address)
{
    if (address == 0){return -1;}
    __mutex.lock();
    if (__managedNativeAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            if (mo.nativeObjectIndex >= 0)
            {
                auto &no = __snapshot.nativeObjects->items[mo.nativeObjectIndex];
                __managedNativeAddressMap.insert(pair<address_t, int32_t>(no.nativeObjectAddress, mo.managedObjectIndex));
            }
        }
    }
    __mutex.unlock();
    
    auto iter = __managedNativeAddressMap.find(address);
    return iter != __managedNativeAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findManagedObjectAtAddress(address_t address)
{
    if (address == 0){return -1;}
    __mutex.lock();
    if (__managedObjectAddressMap.size() == 0)
    {
        for (auto i = 0; i < managedObjects.size(); i++)
        {
            auto &mo = managedObjects[i];
            __managedObjectAddressMap.insert(pair<address_t, int32_t>(mo.address, mo.managedObjectIndex));
        }
    }
    __mutex.unlock();
    
    auto iter = __managedObjectAddressMap.find(address);
    return iter != __managedObjectAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findNativeObjectAtAddress(address_t address)
{
    if (address == 0){return -1;}
    __mutex.lock();
    if (__nativeObjectAddressMap.size() == 0)
    {
        auto &nativeObjects = *__snapshot.nativeObjects;
        for (auto i = 0; i < nativeObjects.size; i++)
        {
            auto &no = nativeObjects[i];
            __nativeObjectAddressMap.insert(pair<address_t, int32_t>(no.nativeObjectAddress, no.nativeObjectArrayIndex));
        }
    }
    __mutex.unlock();
    
    auto iter = __nativeObjectAddressMap.find(address);
    return iter != __nativeObjectAddressMap.end() ? iter->second : -1;
}

int32_t MemorySnapshotCrawler::findGCHandleWithTargetAddress(address_t address)
{
    if (address == 0){return -1;}
    __mutex.lock();
    if (__gcHandleAddressMap.size() == 0)
    {
        auto &gcHandles = *__snapshot.gcHandles;
        for (auto i = 0; i < gcHandles.size; i++)
        {
            auto &no = gcHandles[i];
            __gcHandleAddressMap.insert(pair<address_t, int32_t>(no.target, no.gcHandleArrayIndex));
        }
    }
    __mutex.unlock();
    
    auto iter = __gcHandleAddressMap.find(address);
    return iter != __gcHandleAddressMap.end() ? iter->second : -1;
}

void MemorySnapshotCrawler::tryConnectWithNativeObject(ManagedObject &mo)
{
    if (mo.nativeObjectIndex >= 0){return;}
    
    auto &type = __snapshot.typeDescriptions->items[mo.typeIndex];
    mo.isValueType = type.isValueType;
    if (mo.isValueType || type.isArray || !type.isUnityEngineObjectType) {return;}
    
    auto nativeAddress = __memoryReader->readPointer(mo.address + __snapshot.cached_ptr->offset);
    if (nativeAddress == 0){return;}
    
    auto nativeObjectIndex = findNativeObjectAtAddress(nativeAddress);
    if (nativeObjectIndex == -1){return;}
    
    // connect managed/native objects
    auto &no = __snapshot.nativeObjects->items[nativeObjectIndex];
    mo.nativeObjectIndex = nativeObjectIndex;
    mo.nativeSize = no.size;
    no.managedObjectArrayIndex = mo.managedObjectIndex;
    
    // connect managed/native types
    auto &nativeType = __snapshot.nativeTypes->items[no.nativeTypeArrayIndex];
    type.nativeTypeArrayIndex = nativeType.typeIndex;
    nativeType.managedTypeArrayIndex = type.typeIndex;
}

inline void MemorySnapshotCrawler::setObjectSize(ManagedObject &mo, TypeDescription &type, HeapMemoryReader &memoryReader)
{
    if (mo.size != 0){return;}
    mo.size = memoryReader.readObjectSize(mo.address, type);
}

ManagedObject &MemorySnapshotCrawler::createManagedObject()
{
    __mutex.lock();
    auto &mo = managedObjects.add();
    mo.managedObjectIndex = managedObjects.size() - 1;
    __mutex.unlock();
    return mo;
}

EntityJoint &MemorySnapshotCrawler::createJoint()
{
    __mutex.lock();
    auto &ej = joints.add();
    ej.jointArrayIndex = joints.size() - 1;
    __mutex.unlock();
    return ej;
}

EntityJoint &MemorySnapshotCrawler::cloneJoint(EntityJoint &joint)
{
    __mutex.lock();
    auto &ej = joints.clone(joint);
    ej.jointArrayIndex = joints.size() - 1;
    __mutex.unlock();
    return ej;
}

EntityConnection &MemorySnapshotCrawler::createConnection()
{
    __mutex.lock();
    auto &ec = connections.add();
    ec.connectionArrayIndex = connections.size() - 1;
    __mutex.unlock();
    return ec;
}

bool MemorySnapshotCrawler::isCrawlable(TypeDescription &type)
{
    return !type.isValueType || type.size > 8; // vm->pointerSize
}

void MemorySnapshotCrawler::crawlManagedArrayAddress(address_t address, TypeDescription &type, HeapMemoryReader &memoryReader, EntityJoint &joint, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return;}
    
    auto &elementType = __snapshot.typeDescriptions->items[type.baseOrElementTypeIndex];
    if (!isCrawlable(elementType)) {return;}
    
    address_t elementAddress = 0;
    auto elementCount = memoryReader.readArrayLength(address, type);
    for (auto i = 0; i < elementCount; i++)
    {
        if (elementType.isValueType)
        {
            elementAddress = address + __vm->arrayHeaderSize + i *elementType.size - __vm->objectHeaderSize;
        }
        else
        {
            auto ptrAddress = address + __vm->arrayHeaderSize + i * __vm->pointerSize;
            elementAddress = memoryReader.readPointer(ptrAddress);
        }
        
        auto &elementJoint = cloneJoint(joint);
        
        // set element info
        elementJoint.elementArrayIndex = i;
        
        crawlManagedEntryAddress(elementAddress, &elementType, memoryReader, elementJoint, false, depth + 1);
    }
}

void MemorySnapshotCrawler::crawlManagedEntryAddress(address_t address, TypeDescription *type, HeapMemoryReader &memoryReader, EntityJoint &joint, bool isActualType, int32_t depth)
{
    auto isStaticCrawling = memoryReader.isStatic();
    if (address < 0 || (!isStaticCrawling && address == 0)){return;}
    if (depth >= 512) {return;}
    
    int32_t typeIndex = -1;
    if (type != nullptr && type->isValueType)
    {
        typeIndex = type->typeIndex;
    }
    else
    {
        if (isActualType)
        {
            typeIndex = type->typeIndex;
        }
        else
        {
            typeIndex = findTypeOfAddress(address);
            if (typeIndex == -1 && type != nullptr) {typeIndex = type->typeIndex;}
        }
    }
    if (typeIndex == -1){return;}
    
    auto &entryType = __snapshot.typeDescriptions->items[typeIndex];
    
    ManagedObject *mo;
    __mutex.lock();
    auto iter = __crawlingVisit.find(address);
    bool isCrawlled = iter != __crawlingVisit.end();
    __mutex.unlock();
    if (entryType.isValueType || !isCrawlled)
    {
        mo = &createManagedObject();
        mo->jointArrayIndex = joint.jointArrayIndex;
        mo->gcHandleIndex = joint.gcHandleIndex;
        mo->typeIndex = typeIndex;
        mo->address = address;
#if USE_ADDRESS_MIRROR
        assert(address == __mirror[mo->managedObjectIndex]);
#endif
        tryConnectWithNativeObject(*mo);
        setObjectSize(*mo, entryType, memoryReader);
    }
    else
    {
        auto managedObjectIndex = iter->second;
        mo = &managedObjects[managedObjectIndex];
    }
    
    assert(mo->managedObjectIndex >= 0);
    
    auto &ec = createConnection();
    if (joint.gcHandleIndex >= 0)
    {
        ec.fromKind = ConnectionKind::gcHandle;
        ec.from = -1;
    }
    else if (joint.isStatic)
    {
        ec.fromKind = ConnectionKind::Static;
        ec.from = -1;
    }
    else
    {
        ec.fromKind = ConnectionKind::Managed;
        ec.from = joint.hookObjectIndex;
    }
    
    ec.jointArrayIndex = joint.jointArrayIndex;
    ec.toKind = ConnectionKind::Managed;
    ec.to = mo->managedObjectIndex;
    tryAcceptConnection(ec);
    
    if (!entryType.isValueType)
    {
        __mutex.lock();
        iter = __crawlingVisit.find(address);
        if (iter != __crawlingVisit.end())
        {
            __mutex.unlock();
            return;
        }
        __crawlingVisit.insert(pair<address_t, int32_t>(address, mo->managedObjectIndex));
        __mutex.unlock();
    }
    
    if (entryType.isArray)
    {
        crawlManagedArrayAddress(address, entryType, memoryReader, joint, depth + 1);
        return;
    }
    
    auto iterType = &entryType;
    while (iterType != nullptr)
    {
        for (auto i = 0; i < iterType->fields->size; i++)
        {
            auto &field = iterType->fields->items[i];
            if (field.isStatic){continue;}
            
            auto *fieldType = &__snapshot.typeDescriptions->items[field.typeIndex];
            if (!isCrawlable(*fieldType)){continue;}
            
            address_t fieldAddress = 0;
            if (fieldType->isValueType)
            {
                fieldAddress = address + field.offset - __vm->objectHeaderSize;
            }
            else
            {
                address_t ptrAddress = 0;
                if (memoryReader.isStatic())
                {
                    ptrAddress = address + field.offset - __vm->objectHeaderSize;
                }
                else
                {
                    ptrAddress = address + field.offset;
                }
                fieldAddress = memoryReader.readPointer(ptrAddress);
                auto fieldTypeIndex = findTypeOfAddress(fieldAddress);
                if (fieldTypeIndex != -1)
                {
                    fieldType = &__snapshot.typeDescriptions->items[fieldTypeIndex];
                }
            }
            
            auto *reader = &memoryReader;
            if (!fieldType->isValueType)
            {
                reader = this->__memoryReader;
            }
            
            auto &ej = createJoint();
            
            // set field hook info
            ej.hookObjectAddress = address;
            ej.hookObjectIndex = mo->managedObjectIndex;
            ej.hookTypeIndex = entryType.typeIndex;
            
            // set field info
            ej.fieldAddress = fieldAddress;
            ej.fieldTypeIndex = field.typeIndex;
            ej.fieldSlotIndex = field.fieldSlotIndex;
            ej.fieldOffset = field.offset;
            
            crawlManagedEntryAddress(fieldAddress, fieldType, *reader, ej, true, depth + 1);
        }
        
        if (iterType->baseOrElementTypeIndex == -1)
        {
            iterType = nullptr;
        }
        else
        {
            iterType = &__snapshot.typeDescriptions->items[iterType->baseOrElementTypeIndex];
        }
    }
}

void MemorySnapshotCrawler::crawlGCHandles()
{
    __sampler.begin("crawlGCHandles");
    auto &gcHandles = *__snapshot.gcHandles;
    for (auto i = 0; i < gcHandles.size; i++)
    {
        auto &item = gcHandles[i];
        
        auto &joint = joints.add();
        joint.jointArrayIndex = joints.size() - 1;
        
        // set gcHandle info
        joint.gcHandleIndex = item.gcHandleArrayIndex;
        
        crawlManagedEntryAddress(item.target, nullptr, *__memoryReader, joint, false, 0);
    }
    __sampler.end();
}

void MemorySnapshotCrawler::crawlStatic()
{
    __sampler.begin("crawlStatic");
    auto &typeDescriptions = *__snapshot.typeDescriptions;
    for (auto i = 0; i < typeDescriptions.size; i++)
    {
        auto &type = typeDescriptions[i];
        if (type.staticFieldBytes == nullptr || type.staticFieldBytes->size == 0){continue;}
        for (auto n = 0; n < type.fields->size; n++)
        {
            auto &field = type.fields->items[n];
            if (!field.isStatic){continue;}
            
            __staticMemoryReader->load(*type.staticFieldBytes);
            
            HeapMemoryReader *reader;
            address_t fieldAddress = 0;
            auto *fieldType = &__snapshot.typeDescriptions->items[field.typeIndex];
            if (fieldType->isValueType)
            {
                fieldAddress = field.offset - __vm->objectHeaderSize;
                reader = __staticMemoryReader;
            }
            else
            {
                fieldAddress = __staticMemoryReader->readPointer(field.offset);
                reader = __memoryReader;
            }
            
            auto &joint = createJoint();
            
            // set static field info
            joint.hookTypeIndex = type.typeIndex;
            joint.fieldSlotIndex = field.fieldSlotIndex;
            joint.fieldTypeIndex = field.typeIndex;
            joint.isStatic = true;
            
            crawlManagedEntryAddress(fieldAddress, fieldType, *reader, joint, false, 0);
        }
    }
    __sampler.end();
}

MemorySnapshotCrawler::~MemorySnapshotCrawler()
{
    __sampler.summary();
    
    delete __mirror;
    delete __memoryReader;
    delete __staticMemoryReader;
    for (auto iter = toConnections.begin(); iter != toConnections.end(); ++iter)
    {
        delete iter->second;
    }
    
    for (auto iter = fromConnections.begin(); iter != fromConnections.end(); ++iter)
    {
        delete iter->second;
    }
}
