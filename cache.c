
#include <stdio.h>
#include <math.h>

#define MAX_CACHE_SIZE 256
#define MAX_BLOCK_SIZE 256

extern int mem_access(int addr, int write_flag, int write_data);
extern int get_num_mem_accesses();

enum actionType
{
    cacheToProcessor,
    processorToCache,
    memoryToCache,
    cacheToMemory,
    cacheToNowhere
};

typedef struct blockStruct
{
    int data[MAX_BLOCK_SIZE];
    int valid; // ADDED VARIABLE
    int addy; // ADDED VARIABLE
    int dirty;
    int lruLabel;
    int set;
    int tag;
} blockStruct;

typedef struct cacheStruct
{
    blockStruct blocks[MAX_CACHE_SIZE];
    int blockSize;
    int numSets;
    int blocksPerSet;
} cacheStruct;

/* Global Cache variable */
cacheStruct cache;

void printAction(int, int, enum actionType);
void printCache();
void printStats();

/*
 * Set up the cache with given command line parameters. This is 
 * called once in main(). You must implement this function.
 */
void cache_init(int blockSize, int numSets, int blocksPerSet){
    cache.blockSize = blockSize;
    cache.numSets = numSets;
    cache.blocksPerSet = blocksPerSet;
    return;
}


// each set has own LRU
// only update ones below 
void updateLRU(int curr, int set) {
    for (int block = 0; block < cache.blocksPerSet; ++block) {
        if (cache.blocks[set * cache.blocksPerSet + block].lruLabel <= curr) {
            cache.blocks[set * cache.blocksPerSet + block].lruLabel++;
        }
    }
}


// Add new block into cache
void addBlock(int tag, int set, int addr, int startaddy, int block) {
    cache.blocks[set * cache.blocksPerSet + block].valid = 1;
    cache.blocks[set * cache.blocksPerSet + block].tag = tag;
    cache.blocks[set * cache.blocksPerSet + block].set = set;
    cache.blocks[set * cache.blocksPerSet + block].addy = addr;
    cache.blocks[set * cache.blocksPerSet + block].dirty = 0;
    updateLRU(cache.blocksPerSet - 1, set);
    cache.blocks[set * cache.blocksPerSet + block].lruLabel = 0;
    // Fill data from mem
    for (int i = 0; i < cache.blockSize; ++i) {
        cache.blocks[set * cache.blocksPerSet + block].data[i] = mem_access(startaddy + i, 0, 0);
        // printf(" %d ", mem_access(startaddy + i, 0, 0));
    }
}


int cache_access(int addr, int write_flag, int write_data) {
    // printStats();

    int blockOffsetBits = log2(cache.blockSize);
    int setIndexBits = log2(cache.numSets);
    
    int tag = addr >> (blockOffsetBits + setIndexBits);

    // set index:
    int mask = 0;
    for (int i = 0; i < setIndexBits; ++i) {
        mask *= 2;
        mask++;
    }
    int set = (addr >> blockOffsetBits) & mask;

    // block offset
    mask = 0;
    for (int i = 0; i < blockOffsetBits; ++i) {
        mask *= 2;
        mask++;
    }
    int offset = (addr & mask);

    // Check for a cache hit:
    int startaddy = addr - (addr % cache.blockSize);
    int maybe = addr & (cache.blockSize - 1); // MAYBE
    int hit = 0;
    for (int block = 0; block < cache.blocksPerSet; ++block) {
        if (cache.blocks[set * cache.blocksPerSet + block].tag == tag && cache.blocks[set * cache.blocksPerSet + block].valid == 1) {
            hit = 1;
            if (!write_flag) { // if fetch/lw
                printAction(addr, 1, cacheToProcessor);
                updateLRU(cache.blocks[set * cache.blocksPerSet + block].lruLabel, set);
                cache.blocks[set * cache.blocksPerSet + block].lruLabel = 0;
                // printf(" %d ", cache.blocks[set * cache.blocksPerSet + block].data[addr]);
                return cache.blocks[set * cache.blocksPerSet + block].data[maybe];
            }
            else { // if sw
                printAction(addr, 1, processorToCache);
                cache.blocks[set * cache.blocksPerSet + block].data[offset] = write_data;
                cache.blocks[set * cache.blocksPerSet + block].dirty = 1;
                updateLRU(cache.blocks[set * cache.blocksPerSet + block].lruLabel, set);
                cache.blocks[set * cache.blocksPerSet + block].lruLabel = 0;
                return 0;
            }
        }
    }

    // If a miss:
    if (hit == 0) {
        int full = 1;
        for (int block = 0; block < cache.blocksPerSet; ++block) {
            if (cache.blocks[set * cache.blocksPerSet + block].valid == 0) { // cache has empty spot
                full = 0;
                printAction(startaddy, cache.blockSize, memoryToCache);
                addBlock(tag, set, addr, startaddy, block);
                
                if (!write_flag) { // if fetch/lw
                    printAction(addr, 1, cacheToProcessor);
                    // printf(" %d ", cache.blocks[set * cache.blocksPerSet + block].data[addr]);
                    return cache.blocks[set * cache.blocksPerSet + block].data[maybe];
                }
                else { // if sw
                    printAction(addr, 1, processorToCache);
                    cache.blocks[set * cache.blocksPerSet + block].data[offset] = write_data;
                    cache.blocks[set * cache.blocksPerSet + block].dirty = 1;
                    return 0;
                }
            }
        }



        // if cache is full and need to evict:
        if (full == 1) {

            for (int block = 0; block < cache.blocksPerSet; ++block) {

                if (cache.blocks[set * cache.blocksPerSet + block].lruLabel == (cache.blocksPerSet - 1)) { // evict this block
                    int evictStart = cache.blocks[set * cache.blocksPerSet + block].addy - (cache.blocks[set * cache.blocksPerSet + block].addy % cache.blockSize);
                    if (!cache.blocks[set * cache.blocksPerSet + block].dirty) { // if CLEAN
                        printAction(evictStart, cache.blockSize, cacheToNowhere); // evict from cache

                        printAction(startaddy, cache.blockSize, memoryToCache); // add new block from memory to cache
                        addBlock(tag, set, addr, startaddy, block);
                    }

                    else { // if DIRTY
                        printAction(evictStart, cache.blockSize, cacheToMemory); // evict from cache and write back to memory
                        for (int i = 0; i < cache.blockSize; ++i) {
                            mem_access(evictStart + i, 1, cache.blocks[set * cache.blocksPerSet + block].data[i]);
                        }

                        printAction(startaddy, cache.blockSize, memoryToCache); // add new block from memory to cache
                        addBlock(tag, set, addr, startaddy, block);
                    }

                    if (!write_flag) { // if fetch/lw
                        printAction(addr, 1, cacheToProcessor);
                        // printf(" %d ", cache.blocks[set * cache.blocksPerSet + block].data[addr]);
                        return cache.blocks[set * cache.blocksPerSet + block].data[maybe];
                    }
                    else { // if sw
                        printAction(addr, 1, processorToCache);
                        cache.blocks[set * cache.blocksPerSet + block].data[offset] = write_data;
                        cache.blocks[set * cache.blocksPerSet + block].dirty = 1;
                        return 0;
                    }
                }
            }
        }
    }
    return 0;
}

void printStats(){
    printf("\ncache:\n");
    for (int set = 0; set < cache.numSets; ++set) {
        printf("\tset %i:\n", set);
        for (int block = 0; block < cache.blocksPerSet; ++block) {
            printf("\t\t[ %i ]: {", block);
            for (int index = 0; index < cache.blockSize; ++index) {
                printf(" %i", cache.blocks[set * cache.blocksPerSet + block].data[index]);
            }
            /*
            printf(" Dirty:%i ", cache.blocks[set * cache.blocksPerSet + block].dirty);
            printf("LRU:%i ", cache.blocks[set * cache.blocksPerSet + block].lruLabel);
            printf("Tag:%i ", cache.blocks[set * cache.blocksPerSet + block].tag);
            printf("Valid:%i ", cache.blocks[set * cache.blocksPerSet + block].valid);
            */
            printf(" }\n");
            
        }
    }
    printf("end cache\n");
}

/*
 * Log the specifics of each cache action.
 *
 * address is the starting word address of the range of data being transferred.
 * size is the size of the range of data being transferred.
 * type specifies the source and destination of the data being transferred.
 *  -    cacheToProcessor: reading data from the cache to the processor    // lw
 *  -    processorToCache: writing data from the processor to the cache    // SW
 *  -    memoryToCache: reading data from the memory to the cache          // LW / SW
 *  -    cacheToMemory: evicting cache data and writing it to the memory   // SW / LW
 *  -    cacheToNowhere: evicting cache data and throwing it away          // SW / LW
 */
void printAction(int address, int size, enum actionType type)
{
    printf("$$$ transferring word [%d-%d] ", address, address + size - 1);

    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    }
    else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    }
    else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    }
    else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    }
    else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
}


void printCache()
{
    printf("\ncache:\n");
    for (int set = 0; set < cache.numSets; ++set) {
        printf("\tset %i:\n", set);
        for (int block = 0; block < cache.blocksPerSet; ++block) {
            printf("\t\t[ %i ]: {", block);
            for (int index = 0; index < cache.blockSize; ++index) {
                printf(" %i", cache.blocks[set * cache.blocksPerSet + block].data[index]);
            }
            printf(" }\n");
        }
    }
    printf("end cache\n");
}