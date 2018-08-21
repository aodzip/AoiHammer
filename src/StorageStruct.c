#include "StorageStruct.h"
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static IndexInfo indexStorage[4][0xFFFF];
static SearchResult *parallelSearchResult;

void insertData(uint64_t id, uint64_t phash)
{
    ImageInfo *im = calloc(1, sizeof(ImageInfo));
    im->id = id;
    im->phash.hash = phash;
    for (uint8_t i = 0; i < 4; i++)
    {
        IndexInfo *index = &indexStorage[i][im->phash.section[i]];
        insertIndex(index, im);
    }
}

uint8_t initIndex(IndexInfo *index)
{
    uint8_t cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
    index->parralSearchWorker = cpuCount;
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
    newNode->info = image;
    if (index->end != NULL)
    {
        index->end->next = newNode;
    }
    index->end = newNode;
    index->count++;
    for (uint8_t i = 0; i < index->parralSearchWorker; i++)
    {
        if (index->parallelSearchIndex[i] == NULL)
        {
            index->parallelSearchIndex[i] = newNode;
            break;
        }
        if (index->count != index->parralSearchWorker && index->count % index->parralSearchWorker == 0)
        {
            ChainNode *nextIndex = index->parallelSearchIndex[i];
            for (uint8_t offset = 0; offset < i; offset++)
            {
                nextIndex = nextIndex->next;
            }
            index->parallelSearchIndex[i] = nextIndex;
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

uint64_t searchNode(ChainNode *start, ChainNode *end, uint64_t hash, uint8_t *distance)
{
    uint64_t result = 0;
    uint8_t currentDistance = 64;
    ChainNode *current = start;
    do
    {
        uint8_t distance = hammingDistance(current->info->phash.hash, hash);
        if (distance < currentDistance)
        {
            currentDistance = distance;
            result = current->info->id;
        }
        current = current->next;
    } while (current != end);
    *distance = currentDistance;
    return result;
}

uint64_t startSearch(pHashStore hash)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        IndexInfo *indexList = indexStorage[i];
        if (indexList[hash.section[i]].count > 0)
        {
            IndexInfo *index = &indexList[hash.section[i]];
            launchWorker(index, hash.hash);
            uint64_t result;
            uint8_t currentDistance = 64;
            for (uint8_t i = 0; i < index->parralSearchWorker; i++)
            {
                SearchResult rs = parallelSearchResult[i];
                if (rs.distance < currentDistance)
                {
                    result = rs.id;
                }
            }
            free(parallelSearchResult);
            return result;
        }
    }
    return 0;
}

void launchWorker(IndexInfo *index, uint64_t hash)
{
    parallelSearchResult = calloc(index->parralSearchWorker, sizeof(SearchResult));
    pthread_t threadId[index->parralSearchWorker];
    for (uint8_t i = 0; i < index->parralSearchWorker; i++)
    {
        ThreadArgv *argv = calloc(1, sizeof(ThreadArgv));
        argv->index = index;
        argv->workerId = i;
        argv->hash = hash;
        pthread_create(&threadId[i], NULL, searchThread, argv);
    }
    for (uint8_t i = 0; i < index->parralSearchWorker; i++)
    {
        pthread_join(threadId[i], NULL);
    }
}

void *searchThread(void *argv)
{
    ThreadArgv *arg = argv;
    IndexInfo *index = arg->index;
    uint8_t workerId = arg->workerId;
    uint64_t hash = arg->hash;
    uint8_t distance;
    ChainNode *start = index->parallelSearchIndex[workerId];
    ChainNode *end = index->parallelSearchIndex[workerId + 1];
    if (start == NULL)
    {
        parallelSearchResult[workerId].id = 0;
        parallelSearchResult[workerId].distance = 64;
    }
    else
    {
        uint64_t result = searchNode(start, end, hash, &distance);
        parallelSearchResult[workerId].id = result;
        parallelSearchResult[workerId].distance = distance;
    }
    free(argv);
    return ((void *)NULL);
}