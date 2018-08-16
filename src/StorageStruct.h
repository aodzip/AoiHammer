#pragma once
#include <inttypes.h>
typedef union pHashStore pHashStore;
typedef struct ImageInfo ImageInfo;
typedef struct ChainNode ChainNode;
typedef struct IndexInfo IndexInfo;

typedef union pHashStore {
    uint64_t hash;
    uint16_t section[4];
} pHashStore;

typedef struct ImageInfo
{
    uint64_t id;
    pHashStore phash;
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
    uint8_t parralSearchWorker;
    ChainNode **parallelSearchIndex;
} IndexInfo;

static IndexInfo indexA[0xFFFF];
static IndexInfo indexB[0xFFFF];
static IndexInfo indexC[0xFFFF];
static IndexInfo indexD[0xFFFF];

ImageInfo *createImageInfo(uint64_t id, uint64_t phash);
uint8_t initIndex(IndexInfo *index);
uint8_t insertIndex(IndexInfo *index, ImageInfo *image);
uint64_t searchNode(ChainNode *start, ChainNode *end, uint64_t hash, uint8_t *distance);
