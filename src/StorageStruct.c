#include "StorageStruct.h"
#include "AoiHammer.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static IndexInfo indexStorage[4][0xFFFF + 1];

void searchNode(uint64_t hash, ChainNode *start, ChainNode *end, SearchResult *result, uint8_t resultCount);
void *searchThread(void *argv);
uint8_t initIndex(IndexInfo *index);
uint8_t insertIndex(IndexInfo *index, ImageInfo *image);
void bubbleSort(SearchResult *resultArray, uint8_t resultCount);

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

void bubbleSort(SearchResult *resultArray, uint8_t resultCount)
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

uint8_t startSearch(uint64_t searchHash, uint8_t resultCount, SearchResult *resultStorage)
{
    HashStore hash;
    hash.data = searchHash;
    memset(resultStorage, 0, resultCount * sizeof(SearchResult));
    for (uint8_t hashSection = 0; hashSection < 4; hashSection++)
    {
        IndexInfo *index = &indexStorage[hashSection][hash.section[hashSection]];
        if (index->count)
        {
            SearchResult parallelSearchResult[cpuCount][resultCount];
            for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
            {
                for (uint8_t currentId = 0; currentId < resultCount; currentId++)
                {
                    parallelSearchResult[workerId][currentId].id = 0;
                    parallelSearchResult[workerId][currentId].distance = 64;
                }
            }
            pthread_t threadId[cpuCount];
            for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
            {
                ThreadArgv *argv = calloc(1, sizeof(ThreadArgv));
                argv->index = index;
                argv->workerId = workerId;
                argv->hash = hash.data;
                argv->resultStorage = parallelSearchResult[workerId];
                argv->resultCount = resultCount;
                pthread_create(&threadId[workerId], NULL, searchThread, argv);
            }
            for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
            {
                pthread_join(threadId[workerId], NULL);
            }
            SearchResult resultCollect[cpuCount * resultCount];
            for (uint8_t workerId = 0; workerId < cpuCount; workerId++)
            {
                for (uint8_t currentId = 0; currentId < resultCount; currentId++)
                {
                    resultCollect[workerId * resultCount + currentId] = parallelSearchResult[workerId][currentId];
                }
            }
            bubbleSort(resultCollect, cpuCount * resultCount);
            memcpy(resultStorage, resultCollect, resultCount * sizeof(SearchResult));
            return 1;
        }
    }
    return 0;
}

void *searchThread(void *argv)
{
    ThreadArgv *arg = argv;
    IndexInfo *index = arg->index;
    uint8_t workerId = arg->workerId;
    uint64_t hash = arg->hash;
    SearchResult *resultStorage = arg->resultStorage;
    uint8_t resultCount = arg->resultCount;
    ChainNode *start = index->parallelSearchIndex[workerId];
    ChainNode *end = index->parallelSearchIndex[workerId + 1];
    if (start != NULL)
    {
        searchNode(hash, start, end, resultStorage, resultCount);
    }
    free(argv);
    return ((void *)NULL);
}

void searchNode(uint64_t hash, ChainNode *start, ChainNode *end, SearchResult *resultStorage, uint8_t resultCount)
{
    ChainNode *current = start;
    do
    {
        uint8_t distance = hammingDistance(current->info->hash.data, hash);
        if (distance < resultStorage[resultCount - 1].distance)
        {
            resultStorage[resultCount - 1].distance = distance;
            resultStorage[resultCount - 1].id = current->info->id;
            bubbleSort(resultStorage, resultCount);
        }
        current = current->next;
    } while (current != end);
}
