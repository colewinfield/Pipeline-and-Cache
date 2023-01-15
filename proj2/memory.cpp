#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <cstring>

#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */
#define MAXLINELENGTH 1000

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION 0x1c00000

typedef struct IFIDStruct {
    int instr;
    int pcPlus1;
} IFIDType;

typedef struct IDEXStruct {
    int instr;
    int pcPlus1;
    int readRegA;
    int readRegB;
    int offset;
} IDEXType;

typedef struct EXMEMStruct {
    int instr;
    int branchTarget;
    int aluResult;
    int readRegB;
} EXMEMType;

typedef struct MEMWBStruct {
    int instr;
    int writeData;
} MEMWBType;

typedef struct WBENDStruct {
    int instr;
    int writeData;
} WBENDType;

typedef struct stateStruct {
    int pc;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
    IFIDType IFID;
    IDEXType IDEX;
    EXMEMType EXMEM;
    MEMWBType MEMWB;
    WBENDType WBEND;
    int cycles; /* number of cycles run so far */
} stateType;
void printState(stateType *statePtr);

int field0(int instruction);

int field1(int instruction);

int field2(int instruction);

int opcode(int instruction);

void printInstruction(int instr);

int convertNum(int num);

void initializeState(stateType &state);

void run(stateStruct &state);

void instructionFetchStage(stateStruct &state, stateStruct &newState);

void instructionDecodeStage(stateStruct &state, stateStruct &newState);

void executeStage(stateStruct &state, stateStruct &newState);

void memoryStage(stateStruct &state, stateStruct &newState);

void writeBackStage(stateStruct &state, stateStruct &newState);

int getRegisterAContents(int instruction, stateStruct &state);

int getRegisterBContents(int instruction, stateStruct &state);

int getForwardedRegisterA(stateStruct &state);

int getForwardedRegisterB(stateStruct &state);

int getOffset(int instruction, stateStruct &state);

void checkLoadStall(stateStruct &state, stateStruct &newState);

bool hasBranchTaken(stateStruct &state);

int main(int argc, char *argv[]) {
    char line[MAXLINELENGTH];
    stateType state;
    FILE *filePtr;

    initializeState(state);

    if (argc != 2) {
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
        if (sscanf(line, "%d", state.instrMem + state.numMemory) != 1) {
            printf("error in reading address %d\n", state.numMemory);
            exit(1);
        }
        printf("memory[%d]=%d\n", state.numMemory, state.instrMem[state.numMemory]);

        if (sscanf(line, "%d", state.dataMem + state.numMemory) != 1) {
            printf("error in reading address %d\n", state.numMemory);
            exit(1);
        }
    }

    run(state);
}

void run(stateStruct &state) {

    while (true) {

        printState(&state);

        /* check for halt */
        if (opcode(state.MEMWB.instr) == HALT) {
            printf("machine halted\n");
            printf("total of %d cycles executed\n", state.cycles);
            exit(0);
        }

        stateStruct newState = state;
        newState.cycles++;

        /* --------------------- IF stage --------------------- */

        instructionFetchStage(state, newState);

        /* --------------------- ID stage --------------------- */

        instructionDecodeStage(state, newState);

        /* --------------------- EX stage --------------------- */

        executeStage(state, newState);

        /* --------------------- MEM stage --------------------- */

        memoryStage(state, newState);

        /* --------------------- WB stage --------------------- */

        writeBackStage(state, newState);

        state = newState; /* this is the last statement before end of the loop.
			    It marks the end of the cycle and updates the
			    current state with the values calculated in this
			    cycle */
    }
}

void instructionFetchStage(stateStruct &state, stateStruct &newState) {
    newState.IFID.instr = state.instrMem[state.pc];
    newState.IFID.pcPlus1 = state.pc + 1;
    newState.pc++;
}

void instructionDecodeStage(stateStruct &state, stateStruct &newState) {
    newState.IDEX.instr = state.IFID.instr;
    newState.IDEX.pcPlus1 = state.IFID.pcPlus1;
    newState.IDEX.readRegA = getRegisterAContents(state.IFID.instr, state);
    newState.IDEX.readRegB = getRegisterBContents(state.IFID.instr, state);
    newState.IDEX.offset = getOffset(state.IFID.instr, state);
}

void checkLoadStall(stateStruct &state, stateStruct &newState) {
    int IDEXOpCode = opcode(state.IDEX.instr);
    int IFIDRegA = field0(state.IFID.instr), IFIDRegB = field1(state.IFID.instr);
    int IDEXRegB = field1(state.IDEX.instr);

    if (IDEXOpCode == LW) {
        if (IFIDRegA == IDEXRegB || IFIDRegB == IDEXRegB) {
            newState.IDEX.instr = NOOPINSTRUCTION;
            newState.IFID.instr = state.IFID.instr;
            newState.IFID.pcPlus1--;
            newState.pc--;
        }
    }

}

int getRegisterAContents(int instruction, stateStruct &state) {
    return state.reg[field0(instruction)];
}

int getRegisterBContents(int instruction, stateStruct &state) {
    return state.reg[field1(instruction)];
}

int getOffset(int instruction, stateStruct &state) {
    int offset = field2(instruction);
    return convertNum(offset);
}

void executeStage(stateStruct &state, stateStruct &newState) {
    checkLoadStall(state, newState);

    newState.EXMEM.instr = state.IDEX.instr;
    newState.EXMEM.readRegB = state.IDEX.readRegB;
    int op_code = opcode(state.IDEX.instr);

    newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + state.IDEX.offset;

    int registerA = getForwardedRegisterA(state);
    int registerB = getForwardedRegisterB(state);

    if (op_code == ADD) {
        newState.EXMEM.aluResult = registerA + registerB;
    } else if (op_code == NAND) {
        newState.EXMEM.aluResult = ~(registerA & registerB);
    } else if (op_code == LW || op_code == SW) {
        newState.EXMEM.aluResult = registerA + state.IDEX.offset;
    } else if (op_code == BEQ) {
        if (registerA == registerB) {
            newState.EXMEM.aluResult = 1;
        } else {
            newState.EXMEM.aluResult = 0;
        }
    }
}

int getForwardedRegisterA(stateStruct &state) {
    int registerA = field0(state.IDEX.instr);
    int contentsOfA = state.IDEX.readRegA;

    int opCodeWBEND = opcode(state.WBEND.instr);
    int opCodeMEMWB = opcode(state.MEMWB.instr);
    int opCodeEXMEM = opcode(state.EXMEM.instr);

    if (opCodeWBEND == LW) {
        if (registerA == field1(state.WBEND.instr)) {
            contentsOfA = state.WBEND.writeData;
        }
    } else if (opCodeWBEND == ADD || opCodeWBEND == NAND) {
        if (registerA == field2(state.WBEND.instr)) {
            contentsOfA = state.WBEND.writeData;
        }
    }

    if (opCodeMEMWB == LW) {
        if (registerA == field1(state.MEMWB.instr)) {
            contentsOfA = state.MEMWB.writeData;
        }
    } else if (opCodeMEMWB == ADD || opCodeMEMWB == NAND) {
        if (registerA == field2(state.MEMWB.instr)) {
            contentsOfA = state.MEMWB.writeData;
        }
    }

    if (opCodeEXMEM == ADD || opCodeEXMEM == NAND) {
        if (registerA == field2(state.EXMEM.instr)) {
            contentsOfA = state.EXMEM.aluResult;
        }
    }

    return contentsOfA;
}

int getForwardedRegisterB(stateStruct &state) {
    int registerB = field1(state.IDEX.instr);
    int contentsOfB = state.IDEX.readRegB;

    int opCodeWBEND = opcode(state.WBEND.instr);
    int opCodeMEMWB = opcode(state.MEMWB.instr);
    int opCodeEXMEM = opcode(state.EXMEM.instr);

    if (opCodeWBEND == LW) {
        if (registerB == field1(state.WBEND.instr)) {
            contentsOfB = state.WBEND.writeData;
        }
    } else if (opCodeWBEND == ADD || opCodeWBEND == NAND) {
        if (registerB == field2(state.WBEND.instr)) {
            contentsOfB = state.WBEND.writeData;
        }
    }

    if (opCodeMEMWB == LW) {
        if (registerB == field1(state.MEMWB.instr)) {
            contentsOfB = state.MEMWB.writeData;
        }
    } else if (opCodeMEMWB == ADD || opCodeMEMWB == NAND) {
        if (registerB == field2(state.MEMWB.instr)) {
            contentsOfB = state.MEMWB.writeData;
        }
    }

    if (opCodeEXMEM == ADD || opCodeEXMEM == NAND) {
        if (registerB == field2(state.EXMEM.instr)) {
            contentsOfB = state.EXMEM.aluResult;
        }
    }

    return contentsOfB;
}

void memoryStage(stateStruct &state, stateStruct &newState) {
    newState.MEMWB.instr = state.EXMEM.instr;
    int op_code = opcode(state.EXMEM.instr);

    switch (op_code) {
        case ADD:
        case NAND:
            newState.MEMWB.writeData = state.EXMEM.aluResult;
            break;
        case LW:
            newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
            break;
        case SW:
            newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.readRegB;
            break;
        case BEQ:
            if (hasBranchTaken(state)) {
                newState.pc = state.EXMEM.branchTarget;
                newState.IFID.instr = NOOPINSTRUCTION;
                newState.IDEX.instr = NOOPINSTRUCTION;
                newState.EXMEM.instr = NOOPINSTRUCTION;
            }
        case JALR:
        case HALT:
        case NOOP:
            break;
    }
}

bool hasBranchTaken(stateStruct &state) {
    return state.EXMEM.aluResult == 1;
}

void writeBackStage(stateStruct &state, stateStruct &newState) {
    newState.WBEND.writeData = state.MEMWB.writeData;
    newState.WBEND.instr = state.MEMWB.instr;

    int op_code = opcode(state.MEMWB.instr);
    int writeBack = state.MEMWB.writeData;

    if (op_code == ADD || op_code == NAND) {
        newState.reg[field2(state.MEMWB.instr)] = writeBack;
    } else if (op_code == LW) {
        newState.reg[field1(state.MEMWB.instr)] = writeBack;
    }
}

void printState(stateType *statePtr) {
    int i;
    printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
    printf("\tpc %d\n", statePtr->pc);

    printf("\tdata memory:\n");
    for (i = 0; i < statePtr->numMemory; i++) {
        printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
    }
    printf("\tregisters:\n");
    for (i = 0; i < NUMREGS; i++) {
        printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
    }
    printf("\tIFID:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->IFID.instr);
    printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
    printf("\tIDEX:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->IDEX.instr);
    printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
    printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
    printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
    printf("\t\toffset %d\n", statePtr->IDEX.offset);
    printf("\tEXMEM:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->EXMEM.instr);
    printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
    printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
    printf("\t\treadRegB %d\n", statePtr->EXMEM.readRegB);
    printf("\tMEMWB:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->MEMWB.instr);
    printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
    printf("\tWBEND:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->WBEND.instr);
    printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
}

int field0(int instruction) {
    return ((instruction >> 19) & 0x7);
}

int field1(int instruction) {
    return ((instruction >> 16) & 0x7);
}

int field2(int instruction) {
    return (instruction & 0xFFFF);
}

int opcode(int instruction) {
    return (instruction >> 22);
}

void printInstruction(int instr) {
    char opcodeString[10];
    if (opcode(instr) == ADD) {
        strcpy(opcodeString, "add");
    } else if (opcode(instr) == NAND) {
        strcpy(opcodeString, "nand");
    } else if (opcode(instr) == LW) {
        strcpy(opcodeString, "lw");
    } else if (opcode(instr) == SW) {
        strcpy(opcodeString, "sw");
    } else if (opcode(instr) == BEQ) {
        strcpy(opcodeString, "beq");
    } else if (opcode(instr) == JALR) {
        strcpy(opcodeString, "jalr");
    } else if (opcode(instr) == HALT) {
        strcpy(opcodeString, "halt");
    } else if (opcode(instr) == NOOP) {
        strcpy(opcodeString, "noop");
    } else {
        strcpy(opcodeString, "data");
    }

    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           field2(instr));
}

void initializeState(stateType &state) {
    state.pc = 0;
    state.cycles = 0;

    for (int i = 0; i < NUMREGS; i++) {
        state.reg[i] = 0;
    }

    state.IFID.instr = NOOPINSTRUCTION;
    state.IDEX.instr = NOOPINSTRUCTION;
    state.EXMEM.instr = NOOPINSTRUCTION;
    state.MEMWB.instr = NOOPINSTRUCTION;
    state.WBEND.instr = NOOPINSTRUCTION;
}

int convertNum(int num) {
    /* convert a 16-bit number into a 32-bit Sun integer */

    if (num & (1 << 15)) {
        num -= (1 << 16);
    }

    return (num);
}
