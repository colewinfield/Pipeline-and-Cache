#include <stdio.h>
#include <stdlib.h>
#include <cmath>

// Declaring an enum for easy switch functionality and for re-usability.
enum actionType {
    cacheToProcessor, processorToCache, memoryToCache, cacheToMemory, cacheToNowhere
};

enum {
    ADD = 0, NAND = 1, LW = 2, SW = 3, BEQ = 4, JALR = 5, HALT = 6, NOOP = 7
};

#define NUMMEMORY 65536 /* maximum number of words in memory */
#define NUMREGS 8 /* number of machine registers */
#define MAXLINELENGTH 1000
#define REGZERO 0
#define MAXNUMOFBLOCKS 256

typedef struct stateStruct {
    int pc;
    int mem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
} stateType;

typedef struct blockStruct {
    int lines[256];
    bool isValid;
    bool isDirty;
    int tag;
    int setIndex;
    int blockIndex;
    int LRU;
} blockStruct;

typedef struct cacheStruct {
    blockStruct blocks[256];
    int numOfSets;
    int blocksPerSet;
    int blockSize;
    int blockBits;
    int setBits;
} cacheStruct;

int convertNum(int num);

void printAction(int address, int size, enum actionType type);

int getBits(int word, int from, int to);

void clearRegisters(stateType *state);

void checkRegister(int reg);

void checkOffset(int offset);

int getOpCode(int word);

int getRegA(int word);

int getRegB(int word);

int getDestination(int word);

int getOffset(int word);

void add(stateType *state, int instruction);

void nand(stateType *state, int instruction);

void loadWord(stateType *state, int instruction, cacheStruct &cache);

void saveWord(stateType *state, int instruction, cacheStruct &cache);

void branchEqual(stateType *state, int instruction);

void jumpAndLink(stateType *state, int instruction);

void initializeCacheBlocks(cacheStruct &cache);

void setNumberOfBits(cacheStruct &cache);

int getBlockOffset(cacheStruct &cache, int address);

int getSetOffset(cacheStruct &cache, int address);

int getTag(cacheStruct &cache, int address);

void loadCacheFromMemory(cacheStruct &cache, stateStruct &state, int address);

int findBestBlock(cacheStruct &cache, int setOffset, int tag, int address, stateStruct &state);

bool isCacheHit(cacheStruct &cache, int address);

void saveToCache(cacheStruct &cache, int address, int data);

int getLoadWordFromCache(cacheStruct &cache, int address);

void sendDirtyCacheToMemory(blockStruct &block, stateStruct &state, int address, int blockSize);

int getOldAddress(blockStruct &block, cacheStruct &cache);

void updateLRU(cacheStruct &cache, int address);

int main(int argc, char *argv[]) {

    char line[MAXLINELENGTH];
    stateType state;
    FILE *filePtr;
    cacheStruct cache;

    cache.blockSize = atoi(argv[2]);
    cache.numOfSets = atoi(argv[3]);
    cache.blocksPerSet = atoi(argv[4]);

    if (argc != 5) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    filePtr = fopen(argv[1],"r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", argv[1]);
        perror("fopen");
        exit(1);
    }

    /* read in the entire machine-code file into memory */
    for (state.numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL;
         state.numMemory++) {
        if (sscanf(line, "%d", state.mem + state.numMemory) != 1) {
            printf("error in reading address %d\n", state.numMemory);
            exit(1);
        }
    }

    clearRegisters(&state);
    initializeCacheBlocks(cache);

    int halted = 0, numOfInstructions = 0;

    while (!halted) {

        int instruction = state.mem[state.pc];
        int opCode = getOpCode(instruction);
        int instructionAddress = state.pc;

        int minus = instructionAddress % cache.blockSize;
        int printAddress = instructionAddress - minus;

        if (!isCacheHit(cache, instructionAddress)) {
            loadCacheFromMemory(cache, state, instructionAddress);
            printAction(printAddress, cache.blockSize, memoryToCache);
        }

        printAction(instructionAddress, 1, cacheToProcessor);
        updateLRU(cache, instructionAddress);

        switch (opCode) {
            case ADD:
                add(&state, instruction);
                break;
            case NAND:
                nand(&state, instruction);
                break;
            case LW:
                loadWord(&state, instruction, cache);
                break;
            case SW:
                saveWord(&state, instruction, cache);
                break;
            case BEQ:
                branchEqual(&state, instruction);
                break;
            case JALR:
                jumpAndLink(&state, instruction);
                break;
            case NOOP:
                break;
            case HALT:
                halted = 1;
                break;
            default:
                printf("error: opcode isn't recognized");
                exit(1);
        }

        state.pc++;
        numOfInstructions++;

    }


    return (0);
}


void printAction(int address, int size, enum actionType type) {
    printf("@@@ transferring word [%d-%d] ", address, address + size - 1);
    if (type == cacheToProcessor) {
        printf("from the cache to the processor\n");
    } else if (type == processorToCache) {
        printf("from the processor to the cache\n");
    } else if (type == memoryToCache) {
        printf("from the memory to the cache\n");
    } else if (type == cacheToMemory) {
        printf("from the cache to the memory\n");
    } else if (type == cacheToNowhere) {
        printf("from the cache to nowhere\n");
    }
}

int convertNum(int num) {
    /* convert a 16-bit number into a 32-bit Sun integer */

    if (num & (1 << 15)) {
        num -= (1 << 16);
    }

    return (num);
}
// ##########################################################################################
// # getBits: Used to extract each number and data from the instruction. Word is the 32-bit
// # instruction that's sent in to the function. From is the first part of the bits that    #
// # are needed for isolation. Argument "to" is the inclusive ending bit position.          #
// # For instance:                                                                          #
// # Operation codes are in bits 24, 23, and 22. From = 22, to = 24.                        #
// # Register A bits are in 21 to 19. From 21 to 19.                                        #
// ##########################################################################################


int getBits(int word, int from, int to) {
    int mask = ((1 << (to - from + 1)) - 1) << from;
    return (word & mask) >> from;
}

// ##########################################################################################
// # Simply clears the registers before using them. Without doing so, they'll collect garb -#
// # -age information that's leftover. So, each position in the array is initialized to 0.  #
// ##########################################################################################

void clearRegisters(stateType *state) {
    state->pc = 0;
    int i = 0;

    for (i; i < NUMREGS; i++) {
        state->reg[i] = 0;
    }
}

// ##########################################################################################
// # Make sure that the offset does not exceed 16 bits. If it did, an error is thrown and   #
// # the program halts with an exit-code.                                                   #
// ##########################################################################################

void checkOffset(int offset) {
    if (offset > NUMMEMORY) {
        printf("\nout of memory\n");
        exit(1);
    }
}

// ##########################################################################################
// # Checks if the register is within the limits of 0 to 7 (it's only an 8-bit registry)    #
// ##########################################################################################

void checkRegister(int reg) {
    if (reg < REGZERO || reg > (NUMREGS - 1)) {
        printf("\nunreachable registry state\n");
        exit(1);
    }
}

// ##########################################################################################
// # The following get* functions grab their respective bit from the instruction that's     #
// # passed to getBits( ). It will isolate each section of the instruction and return the   #
// # appropriate integer value for regA, regB, the opcode, the destination register, and    #
// # the offset.                                                                            #
// ##########################################################################################


int getOpCode(int word) { return getBits(word, 22, 24); }

int getRegA(int word) { return getBits(word, 19, 21); }

int getRegB(int word) { return getBits(word, 16, 18); }

int getDestination(int word) { return getBits(word, 0, 2); }

int getOffset(int word) { return getBits(word, 0, 15); }

// ##########################################################################################
// # If the add operation code is called for, it will use the by-reference state of the     #
// # computer and save the values from regA + regB into the destination register.           #
// ##########################################################################################

void add(stateType *state, int instruction) {
    int regA, regB, destination;
    regA = getRegA(instruction);
    regB = getRegB(instruction);
    destination = getDestination(instruction);

    checkRegister(regA);
    checkRegister(regB);

    state->reg[destination] = state->reg[regA] + state->reg[regB];
}

// ##########################################################################################
// # If the nand operation code is called for, it will use the by-reference state of the    #
// # computer and save the value from ~(regA & regB) into the destination register.         #
// ##########################################################################################

void nand(stateType *state, int instruction) {
    int regA, regB, destination;
    regA = getRegA(instruction);
    regB = getRegB(instruction);
    destination = getDestination(instruction);

    checkRegister(regA);
    checkRegister(regB);

    state->reg[destination] = ~(state->reg[regA] & state->reg[regB]);
}

// ##########################################################################################
// # If the LW operation code is called for, it will use the by-reference state of the      #
// # computer and load the value from the offset plus regA into regB.                       #
// ##########################################################################################

void loadWord(stateType *state, int instruction, cacheStruct &cache) {
    int regA, regB, offset = getOffset(instruction);
    offset = convertNum(offset);

    regA = getRegA(instruction);
    regB = getRegB(instruction);

    checkRegister(regA);
    checkRegister(regB);
    checkOffset(offset);


    int address = offset + state->reg[regA];

    int minus = address % cache.blockSize;
    int printAddress = address - minus;

    if (isCacheHit(cache, address)) {
        updateLRU(cache, address);
        printAction(address, 1, cacheToProcessor);
        state->reg[regB] = getLoadWordFromCache(cache, address);
    } else {
        loadCacheFromMemory(cache, *state, address);
        printAction(printAddress, cache.blockSize, memoryToCache);
        state->reg[regB] = getLoadWordFromCache(cache, address);
        printAction(address, 1, cacheToProcessor);
    }

    state->reg[regB] = state->mem[offset + state->reg[regA]];
}

int getLoadWordFromCache(cacheStruct &cache, int address) {
    int tag = getTag(cache, address);
    int blockOffset = getBlockOffset(cache, address);

    for (int i = 0; i < (cache.blocksPerSet * cache.numOfSets); i++) {
        if (cache.blocks[i].tag == tag) {
            return cache.blocks[i].lines[blockOffset];
        }
    }

    return 0;
}

// ##########################################################################################
// # If the SW operation code is called for, it will use the by-reference state of the      #
// # computer and save the value of regB into a memory destination of offset + regA.        #
// ##########################################################################################

void saveWord(stateType *state, int instruction, cacheStruct &cache) {
    int regA, regB, offset = getOffset(instruction);

    offset = convertNum(offset);

    regA = getRegA(instruction);
    regB = getRegB(instruction);

    checkRegister(regA);
    checkRegister(regB);
    checkOffset(offset);


    int address = offset + state->reg[regA];

    int minus = address % cache.blockSize;
    int printAddress = address - minus;

    if (!isCacheHit(cache, address)) {
        loadCacheFromMemory(cache, *state, address);
        printAction(printAddress, cache.blockSize, memoryToCache);
        saveToCache(cache, address, state->reg[regB]);
        printAction(address, 1, processorToCache);
    } else {
        updateLRU(cache, address);
        saveToCache(cache, address, state->reg[regB]);
        printAction(address, 1, processorToCache);
    }

}

void saveToCache(cacheStruct &cache, int address, int data) {
    int tag = getTag(cache, address);
    int blockOffset = getBlockOffset(cache, address);
    int setOffset = getSetOffset(cache, address);

    for (int i = 0; i < (cache.numOfSets * cache.blocksPerSet); i++) {
        if (cache.blocks[i].tag == tag && cache.blocks[i].setIndex == setOffset) {
            cache.blocks[i].lines[blockOffset] = data;
            cache.blocks[i].isDirty = true;
        }
    }
}

// ##########################################################################################
// # If the add operation code is called for, it will use the by-reference state of the     #
// # computer and compare the values in regA and regB. If they're equal, it'll branch into  #
// # the PC of PC + offset (plus 1, but that's incremented regardless in the while).        #
// ##########################################################################################

void branchEqual(stateType *state, int instruction) {
    int regA, regB, offset = getOffset(instruction);
    offset = convertNum(offset);

    regA = getRegA(instruction);
    regB = getRegB(instruction);

    checkRegister(regA);
    checkRegister(regB);
    checkOffset(offset);

    if (state->reg[regA] == state->reg[regB]) {
        state->pc += offset;
    }
}

// ##########################################################################################
// # If the JALR operation code is called for, it will use the by-reference state of the    #
// # computer and save the current PC + 1 into regB, then the PC will be replaced with the  #
// # count in regA.                                                                         #
// ##########################################################################################

void jumpAndLink(stateType *state, int instruction) {
    int regA, regB;

    regA = getRegA(instruction);
    regB = getRegB(instruction);

    checkRegister(regA);
    checkRegister(regB);

    state->reg[regB] = state->pc + 1;
    state->pc = state->reg[regA] - 1;
}

//// ########################################################################################################
//// #                                CACHE FUNCTIONS: Initialize the Blocks                                #
//// ########################################################################################################

void initializeCacheBlocks(cacheStruct &cache) {
    setNumberOfBits(cache);

    for (int i = 0; i < MAXNUMOFBLOCKS; i++) {
        cache.blocks[i].tag = -1;
        cache.blocks[i].isValid = false;
        cache.blocks[i].isDirty = false;
        cache.blocks[i].LRU = 0;
        cache.blocks[i].setIndex = -1;
    }

    // partition the cache into sets and block-indices per set
    // using integer division to combine all of the blocks (numSets * blocksPer) into categories
    for (int i = 0; i < (cache.numOfSets * cache.blocksPerSet); i++) {
        cache.blocks[i].setIndex = (i / cache.blocksPerSet);
        cache.blocks[i].blockIndex = (i % cache.blocksPerSet);
    }

}

void setNumberOfBits(cacheStruct &cache) {
    int numOfBlockBits = log2(cache.blockSize);
    int numOfSetBits = log2(cache.numOfSets);

    cache.blockBits = numOfBlockBits;
    cache.setBits = numOfSetBits;
}

//// ########################################################################################################
//// #                       CACHE FUNCTIONS: Get the tag, set offset, and block offset                     #
//// ########################################################################################################

int getBlockOffset(cacheStruct &cache, int address) {
    return getBits(address, 0, cache.blockBits - 1);
}

int getSetOffset(cacheStruct &cache, int address) {
    return getBits(address, cache.blockBits, cache.blockBits + cache.setBits - 1);
}

int getTag(cacheStruct &cache, int address) {
    return getBits(address, cache.setBits + cache.blockBits, 30);
}

//// ########################################################################################################
//// #                       CACHE FUNCTIONS: Load the dang data                                            #
//// ########################################################################################################

void loadCacheFromMemory(cacheStruct &cache, stateStruct &state, int address) {
    int setOffset = getSetOffset(cache, address);
    int tag = getTag(cache, address);
    int bestBlockIndex = findBestBlock(cache, setOffset, tag, address, state);
    int lineIndex = 0;

    for (int i = 0; i < (cache.blocksPerSet * cache.numOfSets); i++) {
        if (cache.blocks[i].setIndex == setOffset && cache.blocks[i].blockIndex == bestBlockIndex) {
            for (int j = address; j < (address + cache.blockSize); j++) {
                cache.blocks[i].lines[lineIndex] = state.mem[j];
                lineIndex++;
            }
        }
    }

}

int findBestBlock(cacheStruct &cache, int setOffset, int tag, int address, stateStruct &state) {
    int highestLRU = 0;

    for (int i = 0; i < cache.blocksPerSet * cache.numOfSets; i++) {
        if (cache.blocks[i].setIndex == setOffset) {
            cache.blocks[i].LRU++;
            if (cache.blocks[i].LRU > highestLRU) {
                highestLRU = cache.blocks[i].LRU;
            }
        }
    }

    for (int i = 0; i < cache.blocksPerSet * cache.numOfSets; i++) {
        if (cache.blocks[i].setIndex == setOffset) {
            if (cache.blocks[i].isValid == false) {
                cache.blocks[i].isValid = true;
                cache.blocks[i].tag = tag;
                cache.blocks[i].LRU = 0;
                return cache.blocks[i].blockIndex;
            } else if (cache.blocks[i].LRU >= highestLRU) {
                int oldAddress = getOldAddress(cache.blocks[i], cache);

                if (cache.blocks[i].isDirty) {
                    sendDirtyCacheToMemory(cache.blocks[i], state, oldAddress, cache.blockSize);
                    printAction(oldAddress, cache.blockSize, cacheToMemory);
                } else {
                    printAction(oldAddress, cache.blockSize, cacheToNowhere);
                }

                cache.blocks[i].isValid = true;
                cache.blocks[i].tag = tag;
                cache.blocks[i].LRU = 0;
                return cache.blocks[i].blockIndex;
            }
        }
    }

    return -1;
}

int getOldAddress(blockStruct &block, cacheStruct &cache) {
    return (block.tag << (cache.setBits + cache.blockBits)) + (block.setIndex << cache.blockBits);
}

void sendDirtyCacheToMemory(blockStruct &block, stateStruct &state, int address, int blockSize) {
    block.isDirty = false;

    int minus = address % blockSize;
    int memAddress = address - minus;

    for (int i = 0; i < blockSize; i++) {
        state.mem[memAddress] = block.lines[i];
        memAddress++;
    }
}

bool isCacheHit(cacheStruct &cache, int address) {
    int tag = getTag(cache, address);
    int setOffset = getSetOffset(cache, address);

    for (int i = 0; i < (cache.blocksPerSet * cache.numOfSets); i++) {
        if (cache.blocks[i].tag == tag && cache.blocks[i].setIndex == setOffset) {
            return true;
        }
    }

    return false;
}

void updateLRU(cacheStruct &cache, int address) {
    int tag = getTag(cache, address);
    int setOffset = getSetOffset(cache, address);

    for (int i = 0; i < (cache.blocksPerSet * cache.numOfSets); i++) {
        if (cache.blocks[i].tag == tag && cache.blocks[i].setIndex == setOffset) {
            cache.blocks[i].LRU = 0;
        } else {
            cache.blocks[i].LRU++;
        }
    }
}
