#include "StorageStruct.h"
#include "AoiHammer.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static IndexInfo indexHashTable[4][0xFFFF + 1];
static IndexInfo fullChain;
static uint32_t maxId = 0;
static uint8_t *searchFilter;

void *searchThread(void *argv);

uint8_t initIndex(IndexInfo *index)
{
    ChainNode **parallelSearchIndex = calloc(cpuCount + 1, sizeof(ChainNode *));
    if (parallelSearchIndex == NULL)
    {
        return 0;
    }
    index->parallelSearchIndex = parallelSearchIndex;
    return 1;
}

uint8_t insertIndex(IndexInfo *index, ImageInfo *image)
{
    if (index->parallelSearchIndex == NULL)
    {
        uint8_t status = initIndex(index);
        if (!status)
        {
            return 0;
        }
    }
    ChainNode *newNode = calloc(1, sizeof(ChainNode));
    if (newNode == NULL)
    {
        return 0;
    }
    newNode->info = image;
    if (index->end != NULL)
    {
        index->end->next = newNode;
    }
    index->end = newNode;
    index->count++;
    for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
    {
        if (index->parallelSearchIndex[workerId] == NULL)
        {
            index->parallelSearchIndex[workerId] = newNode;
            break;
        }
        if (index->count != cpuCount && index->count % cpuCount == 0)
        {
            ChainNode *nextIndex = index->parallelSearchIndex[workerId];
            for (uint8_t offset = 0; offset < workerId; offset++)
            {
                nextIndex = nextIndex->next;
            }
            index->parallelSearchIndex[workerId] = nextIndex;
        }
    }
    return 1;
}

uint8_t insertData(uint32_t id, uint64_t hash)
{
    ImageInfo *im = calloc(1, sizeof(ImageInfo));
    if (im == NULL)
    {
        return 0;
    }
    if (id > maxId)
    {
        maxId = id;
    }
    im->id = id;
    im->hash.data = hash;
    for (uint8_t hashSection = 0; hashSection < 4; hashSection++)
    {
        IndexInfo *index = &indexHashTable[hashSection][im->hash.section[hashSection]];
        if (!insertIndex(index, im))
        {
            return 0;
        }
    }
    if (!insertIndex(&fullChain, im))
    {
        return 0;
    }
    return 1;
}

inline void insertSearchResult(SearchResultStorage *storage, uint32_t *id, uint8_t *distance)
{
    if (storage->realMem == storage->count)
    {
        SearchResult *newPtr = realloc(storage->storage, (storage->count + 32) * sizeof(SearchResult));
        if (newPtr == NULL)
        {
            return;
        }
        storage->storage = newPtr;
        storage->realMem += 32;
    }
    storage->storage[storage->count].id = *id;
    storage->storage[storage->count].distance = *distance;
    storage->count++;
    return;
}

inline uint8_t hammingDistance(uint64_t hashA, uint64_t hashB)
{
    uint8_t distance = 0;
    uint64_t diff = hashA ^ hashB;
    while (diff)
    {
        distance++;
        diff = (diff - 1) & diff;
    }
    return distance;
}

void launchWorker(IndexInfo *index, HashStore hash, uint8_t maxDistance, SearchResultStorage **resultStorage)
{
    pthread_t threadId[cpuCount];

    for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
    {
        ThreadArgv *argv = calloc(1, sizeof(ThreadArgv));
        argv->index = index;
        argv->workerId = workerId;
        argv->hash = hash.data;
        argv->maxDistance = maxDistance;
        argv->resultStorage = resultStorage[workerId];
        pthread_create(&threadId[workerId], NULL, searchThread, argv);
    }
    for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
    {
        pthread_join(threadId[workerId], NULL);
    }
}

void mergeDistanceHashStorage(uint8_t maxDistance, SearchResultStorage *dst, SearchResultStorage *src)
{
    for (uint8_t currentDistanceIdx = 0; currentDistanceIdx < maxDistance; currentDistanceIdx++)
    {
        SearchResultStorage *currentSrc = &src[currentDistanceIdx];
        SearchResultStorage *currentDst = &dst[currentDistanceIdx];
        if (currentSrc->count)
        {
            SearchResult *tmp = realloc(currentDst->storage, (currentDst->count + currentSrc->count) * sizeof(SearchResult));
            if (tmp != NULL)
            {
                currentDst->storage = tmp;
                memcpy(currentDst->storage + currentDst->count, currentSrc->storage, currentSrc->count * sizeof(SearchResult));
                currentDst->count += currentSrc->count;
            }
        }
        free(currentSrc->storage);
    }
}

uint32_t searchIndex(IndexInfo *index, HashStore hash, uint8_t maxDistance, SearchResultStorage **collect)
{
    if (index->count)
    {
        SearchResultStorage *parallelSearchResult[cpuCount];
        for (uint8_t currentIdx = 0; currentIdx < cpuCount; currentIdx++)
        {
            parallelSearchResult[currentIdx] = calloc(maxDistance, sizeof(SearchResultStorage));
            if (parallelSearchResult[currentIdx] == NULL)
            {
                return 0;
            }
        }
        SearchResultStorage *resultCollect = calloc(maxDistance, sizeof(SearchResultStorage));
        if (resultCollect == NULL)
        {
            return 0;
        }
        launchWorker(index, hash, maxDistance, parallelSearchResult);
        for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
        {
            mergeDistanceHashStorage(maxDistance, resultCollect, parallelSearchResult[workerId]);
            free(parallelSearchResult[workerId]);
        }
        *collect = (SearchResultStorage *)resultCollect;
        return 1;
    }
    return 0;
}

void collectSearchResult(uint8_t resultCount, uint8_t maxDistance, SearchResultStorage *resultCollect, SearchResult *resultList)
{
    uint8_t currentCount = 0;
    for (uint8_t currentDistance = 0; currentDistance < maxDistance; currentDistance++)
    {
        uint8_t needResultCount = resultCount - currentCount;
        if (needResultCount)
        {
            uint8_t writeResultCount = resultCollect[currentDistance].count > needResultCount ? needResultCount : resultCollect[currentDistance].count;
            memcpy(resultList + currentCount, resultCollect[currentDistance].storage, writeResultCount * sizeof(SearchResult));
            currentCount += writeResultCount;
        }
        free(resultCollect[currentDistance].storage);
    }
    free(resultCollect);
}

void startFullSearch(uint64_t searchHash, uint8_t maxDistance, uint8_t resultCount, SearchResult *resultStorage)
{
    HashStore hash;
    hash.data = searchHash;
    memset(resultStorage, 0, resultCount * sizeof(SearchResult));
    SearchResultStorage *resultCollect;
    searchIndex(&fullChain, hash, maxDistance, &resultCollect);
    collectSearchResult(resultCount, maxDistance, resultCollect, resultStorage);
}

void startFastSearch(uint64_t searchHash, uint8_t maxDistance, uint8_t resultCount, SearchResult *resultStorage)
{
    searchFilter = calloc(maxId, sizeof(uint8_t));
    HashStore hash;
    hash.data = searchHash;
    memset(resultStorage, 0, resultCount * sizeof(SearchResult));
    SearchResultStorage *fullResultCollect = calloc(maxDistance, sizeof(SearchResultStorage));
    for (uint8_t hashSection = 0; hashSection < 4; hashSection++)
    {
        IndexInfo *index = &indexHashTable[hashSection][hash.section[hashSection]];
        SearchResultStorage *resultCollect;
        if (searchIndex(index, hash, maxDistance, &resultCollect))
        {
            mergeDistanceHashStorage(maxDistance, fullResultCollect, resultCollect);
        }
    }
    collectSearchResult(resultCount, maxDistance, fullResultCollect, resultStorage);
    free(searchFilter);
    searchFilter = NULL;
}

void *searchThread(void *argv)
{
    ThreadArgv *arg = argv;
    IndexInfo *index = arg->index;
    uint8_t workerId = arg->workerId;
    uint64_t hash = arg->hash;
    uint8_t maxDistance = arg->maxDistance;
    SearchResultStorage *resultStorage = arg->resultStorage;
    ChainNode *start = index->parallelSearchIndex[workerId];
    ChainNode *end = index->parallelSearchIndex[workerId + 1];
    if (start != NULL)
    {
        ChainNode *current = start;
        if (searchFilter == NULL)
        {
            do
            {
                uint8_t distance = hammingDistance(current->info->hash.data, hash);
                if (distance < maxDistance)
                {
                    insertSearchResult(&resultStorage[distance], &current->info->id, &distance);
                }
                current = current->next;
            } while (current != end);
        }
        else
        {
            do
            {
                if (!searchFilter[current->info->id])
                {
                    uint8_t distance = hammingDistance(current->info->hash.data, hash);
                    if (distance < maxDistance)
                    {
                        insertSearchResult(&resultStorage[distance], &current->info->id, &distance);
                    }
                    searchFilter[current->info->id] = 1;
                }
                current = current->next;
            } while (current != end);
        }
    }
    free(argv);
    return ((void *)NULL);
}
