#include "StorageStruct.h"
#include <stdlib.h>
#include <unistd.h>

uint8_t initIndex(IndexInfo *index)
{
    uint8_t cpuCount = sysconf(_SC_NPROCESSORS_ONLN);
    index->parralSearchWorker = cpuCount;
    ChainNode **parallelSearchIndex = calloc(cpuCount, sizeof(ChainNode *));
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
    distance = currentDistance;
    return result;
}
