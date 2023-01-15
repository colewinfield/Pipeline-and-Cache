#include <stdio.h>
#include <stdlib.h>

// Declaring an enum for easy switch functionality and for re-usability.
enum {
    ADD = 0, NAND = 1, LW = 2, SW = 3, BEQ = 4, JALR = 5, HALT = 6, NOOP = 7
};

#define NUMMEMORY 65536 /* maximum number of words in memory */
#define NUMREGS 8 /* number of machine registers */
#define MAXLINELENGTH 1000
#define REGZERO 0


typedef struct stateStruct {
    int pc;
    int mem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
} stateType;
int convertNum(int num);

void printState(stateType *);
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

void loadWord(stateType *state, int instruction);

void saveWord(stateType *state, int instruction);

void branchEqual(stateType *state, int instruction);

void jumpAndLink(stateType *state, int instruction);

void printSummary(int numOfInstructions);


int main(int argc, char *argv[]) {
    char line[MAXLINELENGTH];
    stateType state;
    FILE *filePtr;

    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    filePtr = fopen(argv[1], "r");
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
        printf("memory[%d]=%d\n", state.numMemory, state.mem[state.numMemory]);
    }

    clearRegisters(&state);

    int halted = 0, numOfInstructions = 0;

    while (!halted) {
        printState(&state);

        int instruction = state.mem[state.pc];
        int opCode = getOpCode(instruction);

        switch (opCode) {
            case ADD:
                add(&state, instruction);
                break;
            case NAND:
                nand(&state, instruction);
                break;
            case LW:
                loadWord(&state, instruction);
                break;
            case SW:
                saveWord(&state, instruction);
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
		printf("\nSystem halted!\n");
                halted = 1;
                break;
            default:
                printf("error: opcode isn't recognized");
                exit(1);
        }

        state.pc++;
        numOfInstructions++;
    }

    printSummary(numOfInstructions);
    printState(&state);

    return (0);
}
void printState(stateType *statePtr) {
    int i;

    printf("\n@@@\nstate:\n");
    printf("\tpc %d\n", statePtr->pc);
    printf("\tmemory:\n");

    for (i = 0; i < statePtr->numMemory; i++) {
        printf("\t\tmem[ %d ] %d\n", i, statePtr->mem[i]);
    }

    printf("\tregisters:\n");

    for (i = 0; i < NUMREGS; i++) {
        printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
    }

    printf("end state\n");
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

void loadWord(stateType *state, int instruction) {
    int regA, regB, offset = getOffset(instruction);
    offset = convertNum(offset);

    regA = getRegA(instruction);
    regB = getRegB(instruction);

    checkRegister(regA);
    checkRegister(regB);
    checkOffset(offset);

    state->reg[regB] = state->mem[offset + state->reg[regA]];
}

// ##########################################################################################
// # If the SW operation code is called for, it will use the by-reference state of the      #
// # computer and save the value of regB into a memory destination of offset + regA.        #
// ##########################################################################################

void saveWord(stateType *state, int instruction) {
    int regA, regB, offset = getOffset(instruction);
    offset = convertNum(offset);

    regA = getRegA(instruction);
    regB = getRegB(instruction);

    checkRegister(regA);
    checkRegister(regB);
    checkOffset(offset);

    state->mem[offset + state->reg[regA]] = state->reg[regB];
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
	printf("%d", state->reg[regA]);
	printf("\n");
	printf("%d", state->reg[regB]);
	printf("\n regA and regB are equal!");
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

// ##########################################################################################
// # Simple print summary that's used at the very end of the simulation.                    #
// ##########################################################################################

void printSummary(int numOfInstructions) {
    printf("\nmachine halted\n");
    printf("total of ");
    printf("%d", numOfInstructions);
    printf(" instructions executed\nfinal state of machine: \n");
}






