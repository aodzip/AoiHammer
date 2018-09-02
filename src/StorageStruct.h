#pragma once
#include <inttypes.h>
typedef union HashStore HashStore;
typedef struct ImageInfo ImageInfo;
typedef struct ChainNode ChainNode;
typedef struct IndexInfo IndexInfo;
typedef struct SearchResult SearchResult;

typedef union HashStore {
    uint64_t data;
    uint16_t section[4];
} HashStore;

typedef struct ImageInfo
{
    uint32_t id;
    HashStore hash;
} ImageInfo;

typedef struct ChainNode
{
    ImageInfo *info;
    ChainNode *next;
} ChainNode;

typedef struct IndexInfo
{
    uint64_t count;
    ChainNode *end;
    ChainNode **parallelSearchIndex;
} IndexInfo;

typedef struct ThreadArgv
{
    IndexInfo *index;
    uint8_t workerId;
    uint64_t hash;
    SearchResult *resultStorage;
    uint8_t resultCount;
} ThreadArgv;

typedef struct SearchResult
{
    uint32_t id;
    uint8_t distance;
} SearchResult;

uint8_t insertData(uint32_t id, uint64_t phash);
uint8_t startSearch(uint64_t searchHash, uint8_t resultCount, SearchResult *resultStorage);
