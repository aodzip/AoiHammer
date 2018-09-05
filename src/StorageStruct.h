#pragma once
#include <inttypes.h>
typedef union HashStore HashStore;
typedef struct ImageInfo ImageInfo;
typedef struct ChainNode ChainNode;
typedef struct IndexInfo IndexInfo;
typedef struct SearchResult SearchResult;
typedef struct SearchResultStorage SearchResultStorage;

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
    uint8_t maxDistance;
    SearchResultStorage *resultStorage;
} ThreadArgv;

typedef struct SearchResultStorage
{
    uint32_t count;
    uint32_t realMem;
    SearchResult *storage;
} SearchResultStorage;

typedef struct SearchResult
{
    uint32_t id;
    uint8_t distance;
} SearchResult;

uint8_t insertData(uint32_t id, uint64_t phash);
void startFastSearch(uint64_t searchHash, uint8_t maxDistance, uint8_t resultCount, SearchResult *resultStorage);
void startFullSearch(uint64_t searchHash, uint8_t maxDistance, uint8_t resultCount, SearchResult *resultStorage);
