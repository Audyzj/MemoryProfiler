//
//  main.cpp
//  MemoryCrawler
//
//  Created by larryhou on 2019/4/2.
//  Copyright © 2019 larryhou. All rights reserved.
//

#include <string>
#include <thread>
#include <istream>
#include <vector>
#include <ratio>
#include "Crawler/snapshot.h"
#include "Crawler/stream.h"
#include "Crawler/perf.h"
#include "Crawler/serialize.h"
#include "Crawler/crawler.h"
#include "Crawler/cache.h"
//#include "Crawler/args.h"

using std::cout;
using std::endl;
using std::vector;

void readCachedSnapshot(const char *uuid);

bool strbeg(const char *str, const char *cmp)
{
    auto size = strlen(cmp);
    for (auto i = 0; i < size; i++)
    {
        if (str[i] != cmp[i]){return false;}
    }
    if (strlen(str) > size)
    {
        return str[size] == ' ';
    }
    return true;
}

void readCommandOptions(const char *command, std::function<void(std::vector<const char *> &)> callback)
{
    std::vector<const char *> options;
    auto charCount = strlen(command);
    char *item = new char[64];
    auto iter = 0;
    for (auto i = 0; i < charCount; i++)
    {
        if (command[i] == ' ')
        {
            if (iter > 0)
            {
                auto size = strlen(item);
                auto option = new char[size];
                strcpy(option, item);
                options.push_back(option);
                memset(item, 0, size);
                iter = 0;
            }
        }
        else
        {
            item[iter++] = command[i];
        }
    }
    
    if (strlen(item) > 0) { options.push_back(item); }
    
    callback(options);
    
    for (auto i = 0; i < options.size(); i++) { delete [] options[i]; }
}

#include <unistd.h>
#include <memory>
void crawlSnapshot(const char * filepath)
{
    TimeSampler<std::nano> sampler;
    
    sampler.begin("testStream");
    sampler.begin("read_snapshot");
    MemorySnapshotReader reader;
    auto &snapshot = reader.read(filepath);
    sampler.end();
    
    sampler.begin("craw_snapshot");
    MemorySnapshotCrawler crawler(snapshot);
    crawler.crawl();
    sampler.end();
    sampler.end();
    sampler.summary();
    
    SnapshotCrawlerCache cache;
    cache.save(crawler);
    
    while (true)
    {
        std::cout << "/> ";
        
        std::string input;
        while (!getline(std::cin, input))
        {
            std::cin.clear();
            usleep(100000);
        }
        
        const char *command = input.c_str();
        if (strbeg(command, "read"))
        {
            readCachedSnapshot(crawler.snapshot.uuid->c_str());
        }
        else if (strbeg(command, "ref"))
        {
            
        }
        else if (strbeg(command, "uref"))
        {
            
        }
        else if (strbeg(command, "leak"))
        {
            readCommandOptions(command, [](std::vector<const char *> &options)
                               {
                                   if (options.size() == 1){return;}
                                   const char *subcommand = options[1];
                                   if (0 == strcmp(subcommand, "array"))
                                   {
                                       Array<PackedNativeUnityEngineObject> array(1000000);
                                       printf("[Array] size=%d\n", array.size);
                                   }
                                   else if (0 == strcmp(subcommand, "im"))
                                   {
                                       InstanceManager<PackedNativeUnityEngineObject> manager(10000);
                                       for (auto i = 0; i < 1000000; i++) { manager.add(); }
                                       printf("[IM] size=%d\n", manager.size());
                                   }
                                   else if (0 == strcmp(subcommand, "vector"))
                                   {
                                       std::vector<PackedNativeUnityEngineObject> vector(1000000);
                                       printf("[Vector] size=%d\n", (int)vector.size());
                                   }
                                   else if (0 == strcmp(subcommand, "carray"))
                                   {
                                       char array[64*1024];
                                       printf("[CArray] size=%d\n", (int)sizeof(array));
                                   }
                                   else if (0 == strcmp(subcommand, "t1"))
                                   {
                                       std::vector<const Connection *> vector;
                                       for (auto i = 0; i < 1000; i++)
                                       {
                                           vector.push_back(new Connection[1000]);
                                       }
                                       
                                       for (auto iter = vector.begin(); iter != vector.end(); iter++)
                                       {
                                           delete [] *iter;
                                       }
                                       
                                       printf("total_memory=%d\n", (int)sizeof(char) * 1000000);
                                   }
                                   else if (0 == strcmp(subcommand, "t2"))
                                   {
                                       for (auto i = 0; i < 1000; i++)
                                       {
                                           std::allocator<PackedNativeUnityEngineObject> allocator;
                                           auto *ptr = allocator.allocate(1000);
                                           allocator.deallocate(ptr, 1000);
                                       }
                                       
                                       
                                   }
                               });
        }
        else
        {
            printf("not supported command [%s]\n", command);
        }
    }
}

void readCachedSnapshot(const char *uuid)
{
    SnapshotCrawlerCache cache;
    MemorySnapshotCrawler &serializedCrawler = cache.read(uuid);
    cout << serializedCrawler.managedObjects.size() << endl;
    delete &serializedCrawler;
}

int main(int argc, const char * argv[])
{
//    ArgumentManager manager;
//    manager.addOption(new ArgumentOption("--file-snapshot-path", "-f", "内存快照路径", true, true));
//    manager.addOption(new ArgumentOption("--comp-snapshot-path", "-c", "参与比对的内存快照路径", true));
//    manager.addOption(new ArgumentOption("--debug", "-d", "开启调试模式", false));
//    manager.addOption(new ArgumentOption("--interactive", "-i", "开启交互模式", false));
//    manager.help();
//    manager.parseArguments(argc - 1, &argv[1]);
    
    cout << "argc=" << argc << endl;
    for (auto i = 0; i < argc; i++)
    {
        cout << "argv[" << i << "]=" << argv[i] << endl;
    }
    
    if (argc > 1)
    {
        crawlSnapshot(argv[1]);
    }
    
    return 0;
}
