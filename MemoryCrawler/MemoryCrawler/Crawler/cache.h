//
//  cache.h
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/9.
//  Copyright © 2019 larryhou. All rights reserved.
//

#ifndef cache_h
#define cache_h

#include <stdio.h>
#include <sqlite3.h>
#include <new>
#include "crawler.h"
#include "perf.h"

class CrawlerCache
{
    char __buffer[32*1024];
    TimeSampler<std::nano> __sampler;
    sqlite3 *__database;
    
public:
    CrawlerCache();
    void open(const char *filepath);
    void save(MemorySnapshotCrawler &crawler);
    MemorySnapshotCrawler &read();
    ~CrawlerCache();
    
private:
    void createNativeTypeTable();
    void insert(Array<PackedNativeType> &nativeTypes);
    void createNativeObjectTable();
    void insert(Array<PackedNativeUnityEngineObject> &nativeObjects);
    void createTypeTable();
    void createFieldTable();
    void insert(Array<TypeDescription> &types);
    void createJointTable();
    void insert(InstanceManager<EntityJoint> &joints);
    void createConnectionTable();
    void insert(InstanceManager<EntityConnection> &connections);
    void createObjectTable();
    void insert(InstanceManager<ManagedObject> &objects);
    
    template <typename T>
    void insert(const char * sql, InstanceManager<T> &manager, std::function<void(T &item, sqlite3_stmt *stmt)> kernel);
    
    template <typename T>
    void insert(const char * sql, Array<T> &array, std::function<void(T &item, sqlite3_stmt *stmt)> kernel);
    
    void create(const char *sql);
};

template <typename T>
void CrawlerCache::insert(const char * sql, Array<T> &array, std::function<void(T &item, sqlite3_stmt *stmt)> kernel)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(__database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    for (auto i = 0; i < array.size; i++)
    {
        kernel(array[i], stmt);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(__database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}

template <typename T>
void CrawlerCache::insert(const char * sql, InstanceManager<T> &manager, std::function<void(T &item, sqlite3_stmt *stmt)> kernel)
{
    char *errmsg;
    sqlite3_stmt *stmt;
    
    sqlite3_exec(__database, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_prepare_v2(__database, sql, (int)strlen(sql), &stmt, nullptr);
    
    for (auto i = 0; i < manager.size(); i++)
    {
        kernel(manager[i], stmt);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {}
        sqlite3_reset(stmt);
    }
    
    sqlite3_exec(__database, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    sqlite3_finalize(stmt);
}


#endif /* cache_h */
