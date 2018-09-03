#include "StorageStruct.h"
#include "AoiHammer.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static IndexInfo indexStorage[4][0xFFFF + 1];
static IndexInfo fullChain;

void searchNode(uint64_t hash, ChainNode *start, ChainNode *end, SearchResultStorage *result);
void *searchThread(void *argv);
uint8_t initIndex(IndexInfo *index);
uint8_t insertIndex(IndexInfo *index, ImageInfo *image);
void bubbleSort(SearchResult *resultArray, uint32_t resultCount);

uint8_t insertData(uint32_t id, uint64_t hash)
{
    ImageInfo *im = calloc(1, sizeof(ImageInfo));
    if (im == NULL)
    {
        return 0;
    }
    im->id = id;
    im->hash.data = hash;
    for (uint8_t hashSection = 0; hashSection < 4; hashSection++)
    {
        IndexInfo *index = &indexStorage[hashSection][im->hash.section[hashSection]];
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

uint8_t insertSearchResult(SearchResultStorage *storage, SearchResult data)
{
    SearchResult *newPtr = realloc(storage->storage, (storage->count + 1) * sizeof(SearchResult));
    if (newPtr == NULL)
    {
        return 0;
    }
    storage->storage = newPtr;
    storage->storage[storage->count] = data;
    storage->count++;
    return 1;
}

uint8_t hammingDistance(uint64_t hashA, uint64_t hashB)
{
    int distance = 0;
    while (hashA != hashB)
    {
        distance += (hashA & 1) ^ (hashB & 1);
        hashA >>= 1;
        hashB >>= 1;
    }
    return distance;
}

void bubbleSort(SearchResult *resultArray, uint32_t resultCount)
{
    for (uint8_t i = 0; i < resultCount - 1; i++)
    {
        for (uint8_t j = 0; j < resultCount - i - 1; j++)
        {
            if (resultArray[j].distance > resultArray[j + 1].distance)
            {
                SearchResult tmp = resultArray[j + 1];
                resultArray[j + 1] = resultArray[j];
                resultArray[j] = tmp;
            }
        }
    }
}

void launchWorker(IndexInfo *index, HashStore hash, SearchResultStorage *resultStorage)
{
    pthread_t threadId[cpuCount];
    for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
    {
        ThreadArgv *argv = calloc(1, sizeof(ThreadArgv));
        argv->index = index;
        argv->workerId = workerId;
        argv->hash = hash.data;
        argv->resultStorage = &resultStorage[workerId];
        pthread_create(&threadId[workerId], NULL, searchThread, argv);
    }
    for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
    {
        pthread_join(threadId[workerId], NULL);
    }
}

uint32_t searchIndex(IndexInfo *index, HashStore hash, SearchResult **collect)
{
    if (index->count)
    {
        SearchResultStorage parallelSearchResult[cpuCount];
        memset(parallelSearchResult, 0, cpuCount * sizeof(SearchResultStorage));
        launchWorker(index, hash, parallelSearchResult);
        uint32_t searchResultCount = 0;
        for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
        {
            searchResultCount += parallelSearchResult[workerId].count;
        }
        SearchResult *resultCollect = calloc(searchResultCount, sizeof(SearchResult));
        uint32_t currentCollectIndex = 0;
        for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
        {
            SearchResultStorage storage = parallelSearchResult[workerId];
            memcpy(resultCollect + currentCollectIndex, storage.storage, storage.count * sizeof(SearchResult));
            free(storage.storage);
            currentCollectIndex += storage.count;
        }
        *collect = (SearchResult *)resultCollect;
        return searchResultCount;
    }
    return 0;
}

uint8_t startFullSearch(uint64_t searchHash, uint8_t resultCount, SearchResult *resultStorage)
{
    HashStore hash;
    hash.data = searchHash;
    memset(resultStorage, 0, resultCount * sizeof(SearchResult));
    SearchResult *resultCollect;
    uint32_t searchResultCount = searchIndex(&fullChain, hash, &resultCollect);
    bubbleSort(resultCollect, searchResultCount);
    memcpy(resultStorage, resultCollect, (searchResultCount > resultCount ? resultCount : searchResultCount) * sizeof(SearchResult));
    free(resultCollect);
    return searchResultCount;
}

uint8_t startFastSearch(uint64_t searchHash, uint8_t resultCount, SearchResult *resultStorage)
{
    HashStore hash;
    hash.data = searchHash;
    memset(resultStorage, 0, resultCount * sizeof(SearchResult));
    for (uint8_t hashSection = 0; hashSection < 4; hashSection++)
    {
        IndexInfo *index = &indexStorage[hashSection][hash.section[hashSection]];
        SearchResult *resultCollect;
        uint32_t searchResultCount = searchIndex(index, hash, &resultCollect);
        bubbleSort(resultCollect, searchResultCount);
        memcpy(resultStorage, resultCollect, (searchResultCount > resultCount ? resultCount : searchResultCount) * sizeof(SearchResult));
        free(resultCollect);
        return 1;
    }
    return 0;
}

void *searchThread(void *argv)
{
    ThreadArgv *arg = argv;
    IndexInfo *index = arg->index;
    uint8_t workerId = arg->workerId;
    uint64_t hash = arg->hash;
    SearchResultStorage *resultStorage = arg->resultStorage;
    ChainNode *start = index->parallelSearchIndex[workerId];
    ChainNode *end = index->parallelSearchIndex[workerId + 1];
    if (start != NULL)
    {
        searchNode(hash, start, end, resultStorage);
    }
    free(argv);
    return ((void *)NULL);
}

void searchNode(uint64_t hash, ChainNode *start, ChainNode *end, SearchResultStorage *resultStorage)
{
    ChainNode *current = start;
    do
    {
        uint8_t distance = hammingDistance(current->info->hash.data, hash);
        if (distance <= 10)
        {
            SearchResult result;
            result.distance = distance;
            result.id = current->info->id;
            insertSearchResult(resultStorage, result);
        }
        current = current->next;
    } while (current != end);
}
