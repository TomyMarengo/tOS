#ifdef USE_BUDDY

#include <defs.h>
#include <lib.h>
#include <memoryManager.h>

#define MAX_LEVEL        23
#define MIN_LEVEL        8
#define MIN_MEMORY_SIZE  (9 * 1024 * 1024)
#define HEAP_MEMORY_SIZE (8 * 1024 * 1024)
#define MAX_NODES        65535
#define TRUE             1
#define FALSE            0
#define POSITIVE_DELTA   1
#define NEGATIVE_DELTA   -1

typedef struct {
    unsigned int occupied;
    unsigned int occupiedSubnodes;
} Node;

static void *heapStart;
static Node *nodes;
static size_t totalMemory;
static size_t usedMemory;
static unsigned int memoryChunks;

static inline unsigned int
getLeftChildIndex(unsigned int index) {
    return (index << 1) + 1;
}

static inline unsigned int
getRightChildIndex(unsigned int index) {
    return (index << 1) + 2;
}

static inline unsigned int
getParentIndex(unsigned int index) {
    return ((index + 1) >> 1) - 1;
}

static inline unsigned int
getFirstIndexOfLevel(unsigned int level) {
    return (1 << (MAX_LEVEL - level)) - 1;
}

static int
getFirstFreeIndexOfLevel(unsigned int level) {
    int idx = getFirstIndexOfLevel(level);
    int lastIdx = getFirstIndexOfLevel(level - 1);

    while (idx < lastIdx && (nodes[idx].occupied || nodes[idx].occupiedSubnodes))
        ++idx;

    if (idx == lastIdx)
        return -1;

    return idx;
}

static void
setAsOccupied(int idx, int occupied) {
    if (idx < 0 || idx >= MAX_NODES)
        return;

    nodes[idx].occupied = occupied;
    setAsOccupied(getRightChildIndex(idx), occupied);
    setAsOccupied(getLeftChildIndex(idx), occupied);
}

static void
updateParents(unsigned int idx, int occupiedDelta) {
    nodes[idx].occupiedSubnodes += occupiedDelta;
    while (idx != 0) {
        idx = getParentIndex(idx);
        nodes[idx].occupiedSubnodes += occupiedDelta;
    }
}

static inline void *
getAddress(int index, int level) {
    return heapStart + (index - getFirstIndexOfLevel(level)) * (1 << level);
}

static int
searchNode(int idx, int *level, void *ptr) {
    if (*level < MIN_LEVEL)
        return -1;

    if (!nodes[idx].occupied) {
        (*level)--;
        int rightChildIndex = getRightChildIndex(idx);
        void *address = getAddress(rightChildIndex, *level);
        return searchNode(ptr < address ? getLeftChildIndex(idx) : rightChildIndex, level, ptr);
    }

    return ptr == getAddress(idx, *level) ? idx : -1;
}

static unsigned int
getLevel(size_t size) {
    size--;
    unsigned int level = 0;
    while (size != 0) {
        size >>= 1;
        level++;
    }
    return level;
}

static int
getMaxPosibleLevel(void *ptr) {
    int relativeAddress = ptr - heapStart;
    int level = MAX_LEVEL;
    int powOfTwo = 1 << MAX_LEVEL;
    while ((relativeAddress % powOfTwo) != 0) {
        level--;
        powOfTwo = powOfTwo >> 1;
    }
    return level;
}

static int
getStartSearchingIdx(void *ptr, int maxLevel) {
    int relativeAddress = ptr - heapStart;
    return getFirstIndexOfLevel(maxLevel) + (relativeAddress / (1 << maxLevel));
}

void
initializeMemory(void *memoryStart, size_t memorySize) {
    void *actualStart = (void *) WORD_ALIGN_UP(memoryStart);
    memorySize -= (actualStart - memoryStart);

    if (memorySize < MIN_MEMORY_SIZE)
        return;

    nodes = actualStart;
    size_t nodesMemorySize = sizeof(Node) * MAX_NODES;
    actualStart += nodesMemorySize;
    heapStart = actualStart;

    totalMemory = HEAP_MEMORY_SIZE;
    usedMemory = 0;
    memoryChunks = 0;

    for (int i = 0; i < MAX_NODES; ++i) {
        nodes[i].occupied = 0;
        nodes[i].occupiedSubnodes = 0;
    }
}

void *
malloc(size_t size) {
    unsigned int level;
    if (size > HEAP_MEMORY_SIZE || size == 0 || (level = getLevel(size)) > MAX_LEVEL)
        return NULL;

    if (level < MIN_LEVEL)
        level = MIN_LEVEL;

    int index = getFirstFreeIndexOfLevel(level);

    if (index < 0)
        return NULL;

    updateParents(index, POSITIVE_DELTA);
    setAsOccupied(index, TRUE);

    memoryChunks++;
    usedMemory += (1 << level);

    return getAddress(index, level);
}

int
free(void *ptr) {
    if (ptr == NULL)
        return 0;

    if (ptr < heapStart || ptr >= (heapStart + HEAP_MEMORY_SIZE))
        return -1;

    int level = getMaxPosibleLevel(ptr);
    int index = searchNode(getStartSearchingIdx(ptr, level), &level, ptr);

    if (index < 0)
        return -1;

    updateParents(index, NEGATIVE_DELTA);
    setAsOccupied(index, FALSE);

    memoryChunks--;
    usedMemory -= (1 << level);

    return 0;
}

void *
realloc(void *ptr, size_t size) {
    void *newPtr = malloc(size);

    if (newPtr != NULL) {
        if (ptr >= heapStart)
            memcpy(newPtr, ptr, size);
        free(ptr);
    }

    return newPtr;
}

int
getStateMemory(MemoryState *memoryState) {
    memoryState->total = totalMemory;
    memoryState->used = usedMemory;
    memoryState->type = BUDDY;
    return 0;
}

#endif