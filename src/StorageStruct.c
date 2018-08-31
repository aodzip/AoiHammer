#include "StorageStruct.h"
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static IndexInfo indexStorage[4][0xFFFF];
static SearchResult *parallelSearchResult;

uint8_t insertData(uint64_t id, uint64_t phash)
{
    ImageInfo *im = calloc(1, sizeof(ImageInfo));
    im->id = id;
    im->phash.hash = phash;
    for (uint8_t hashSection = 0; hashSection < 4; hashSection++)
    {
        IndexInfo *index = &indexStorage[hashSection][im->phash.section[hashSection]];
        if (!insertIndex(index, im))
        {
            return 0;
        }
    }
    return 1;
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
    for (uint8_t workerId = 0; workerId < index->parralSearchWorker; workerId++)
    {
        if (index->parallelSearchIndex[workerId] == NULL)
        {
            index->parallelSearchIndex[workerId] = newNode;
            break;
        }
        if (index->count != index->parralSearchWorker && index->count % index->parralSearchWorker == 0)
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
    for (uint8_t hashSection = 0; hashSection < 4; hashSection++)
    {
        IndexInfo *index = &indexStorage[hashSection][hash.section[hashSection]];
        if (index->count > 0)
        {
            launchWorker(index, hash.hash);
            uint64_t result;
            uint8_t currentDistance = 64;
            for (uint8_t workerId = 0; workerId < index->parralSearchWorker; workerId++)
            {
                SearchResult partResult = parallelSearchResult[workerId];
                if (partResult.distance < currentDistance)
                {
                    result = partResult.id;
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
    for (uint8_t workerId = 0; workerId < index->parralSearchWorker; workerId++)
    {
        ThreadArgv *argv = calloc(1, sizeof(ThreadArgv));
        argv->index = index;
        argv->workerId = workerId;
        argv->hash = hash;
        pthread_create(&threadId[workerId], NULL, searchThread, argv);
    }
    for (uint8_t workerId = 0; workerId < index->parralSearchWorker; workerId++)
    {
        pthread_join(threadId[workerId], NULL);
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
    parallelSearchResult[workerId].id = 0;
    parallelSearchResult[workerId].distance = 64;
    if (start != NULL)
    {
        uint64_t result = searchNode(start, end, hash, &distance);
        parallelSearchResult[workerId].id = result;
        parallelSearchResult[workerId].distance = distance;
    }
    free(argv);
    return ((void *)NULL);
}
