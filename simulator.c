
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Machine Definitions
#define NUMMEMORY 65536 // maximum number of data words in memory
#define NUMREGS 8 // number of machine registers

#define ADD 0
#define NOR 1
#define LW 2
#define SW 3
#define BEQ 4
#define JALR 5 // will not implemented for Project 3
#define HALT 6
#define NOOP 7

#define NOOPINSTRUCTION (NOOP << 22)

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
    int opcode;
    int dest;
} IDEXType;

typedef struct EXMEMStruct {
	int instr;
	int branchTarget;
    int eq;
	int aluResult;
	int readRegB;
    int opcode;
    int dest;
} EXMEMType;

typedef struct MEMWBStruct {
	int instr;
	int writeData;
    int opcode;
    int dest;
} MEMWBType;

typedef struct WBENDStruct {
	int instr;
	int writeData;
    int dest;
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
	int cycles; // number of cycles run so far
} stateType;

static inline int opcode(int instruction) {
    return instruction>>22;
}

static inline int field0(int instruction) {
    return (instruction>>19) & 0x7;
}

static inline int field1(int instruction) {
    return (instruction>>16) & 0x7;
}

static inline int field2(int instruction) {
    return instruction & 0xFFFF;
}

// convert a 16-bit number into a 32-bit Linux integer
static inline int convertNum(int num) {
    return num - ( (num & (1<<15)) ? 1<<16 : 0 );
}

void printState(stateType*);
void printInstruction(int);
void readMachineCode(stateType*, char*);

int main(int argc, char *argv[]) {
    stateType state, newState;

    if (argc != 2) {
        printf("error: usage: %s <machine-code file>\n", argv[0]);
        exit(1);
    }

    readMachineCode(&state, argv[1]);

    // initalize PC and all regs to zero, and all pipeline regs to noop
    state.pc = 0;
    state.cycles = 0;
    state.IFID.instr = 0x1c00000;
    state.IDEX.instr = 0x1c00000;
    state.EXMEM.instr = 0x1c00000;
    state.MEMWB.instr = 0x1c00000;
    state.WBEND.instr = 0x1c00000;

    while (opcode(state.MEMWB.instr) != HALT) {
        printState(&state);

        newState = state;
        newState.cycles++;

        /* ---------------------- IF stage --------------------- */
        newState.IFID.instr = state.instrMem[state.pc];
        newState.pc = state.pc + 1;
        newState.IFID.pcPlus1 = state.pc + 1;

        /* ---------------------- ID stage --------------------- */
        newState.IDEX.instr = state.IFID.instr;
        newState.IDEX.opcode = opcode(state.IFID.instr);
        int IDA = field0(state.IFID.instr);
        int IDB = field1(state.IFID.instr);
        int IDC = field2(state.IFID.instr);
        newState.IDEX.readRegA = state.reg[IDA];
        newState.IDEX.readRegB = state.reg[IDB];
        newState.IDEX.offset = convertNum(IDC);
        newState.IDEX.pcPlus1 = state.IFID.pcPlus1;

        // CHECK FOR STALLING HAZARDS:
        if (state.IDEX.opcode == LW) {
            if (newState.IDEX.opcode == ADD || newState.IDEX.opcode == NOR || newState.IDEX.opcode == BEQ || newState.IDEX.opcode == SW) {
                if (state.IDEX.dest == IDA || state.IDEX.dest == IDB) {
                    newState.IFID.instr = state.IFID.instr;
                    newState.IDEX.instr = 0x1c00000;
                    newState.pc--;
                    newState.IFID.pcPlus1--;
                    newState.IDEX.opcode = NOOP;
                }
            }
            else if (newState.IDEX.opcode == LW) {
                if (state.IDEX.dest == IDA) {
                    newState.IFID.instr = state.IFID.instr;
                    newState.IDEX.instr = 0x1c00000;
                    newState.pc--;
                    newState.IFID.pcPlus1--;
                    newState.IDEX.opcode = NOOP;
                }
            }
        }

        if (newState.IDEX.opcode == ADD || newState.IDEX.opcode == NOR) {
            newState.IDEX.dest = (convertNum(IDC) & 0b111);
        }
        else if (newState.IDEX.opcode == LW) {
            newState.IDEX.dest = (convertNum(IDB) & 0b111);
        }
        else {
            newState.IDEX.dest = -1;
        }
        

        /* ---------------------- EX stage --------------------- */
        newState.EXMEM.instr = state.IDEX.instr;
        newState.EXMEM.dest = state.IDEX.dest;
        // CHECK FOR HAZARDS THAT DO NOT INVOLVE STALLS:
        int regA = state.IDEX.readRegA;
        int regB = state.IDEX.readRegB;
        int fieldA = convertNum(field0(state.IDEX.instr));
        int fieldB = convertNum(field1(state.IDEX.instr));
        int offset = convertNum(field2(state.IDEX.instr));

        if (state.EXMEM.dest == fieldA) {
            if (state.EXMEM.opcode == ADD || state.EXMEM.opcode == NOR || state.EXMEM.opcode == LW) {
                regA = state.EXMEM.aluResult;
            }
        }
        else if (state.MEMWB.dest == fieldA) {
            if (state.MEMWB.opcode == ADD || state.MEMWB.opcode == NOR || state.MEMWB.opcode == LW) {
                regA = state.MEMWB.writeData;
            }
        }
        else if (state.WBEND.dest == fieldA) {
            if (opcode(state.WBEND.instr) == ADD || opcode(state.WBEND.instr) == NOR || opcode(state.WBEND.instr) == LW) {
                regA = state.WBEND.writeData;
            }
        }

        if (state.EXMEM.dest == fieldB) {
            if (state.EXMEM.opcode == ADD || state.EXMEM.opcode == NOR || state.EXMEM.opcode == LW) {
                regB = state.EXMEM.aluResult;
            }
        }
        else if (state.MEMWB.dest == fieldB) {
            if (state.MEMWB.opcode == ADD || state.MEMWB.opcode == NOR || state.MEMWB.opcode == LW) {
                regB = state.MEMWB.writeData;
            }
        }
        else if (state.WBEND.dest == fieldB) {
            if (opcode(state.WBEND.instr) == ADD || opcode(state.WBEND.instr) == NOR || opcode(state.WBEND.instr) == LW) {
                regB = state.WBEND.writeData;
            }
        }
        
        newState.EXMEM.eq = (regA == regB) ? 1 : 0;
        newState.EXMEM.opcode = opcode(state.IDEX.instr);
        newState.EXMEM.readRegB = regB;
        newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + offset;
        if (state.IDEX.opcode == ADD) {
            newState.EXMEM.aluResult = regA + regB;
        }
        else if (state.IDEX.opcode == NOR) {
            newState.EXMEM.aluResult = ~(regA | regB);
        }
        else if (state.IDEX.opcode == LW || state.IDEX.opcode == SW) {
            newState.EXMEM.aluResult = regA + offset;
        }
        


        /* --------------------- MEM stage --------------------- */
        newState.MEMWB.instr = state.EXMEM.instr;
        newState.MEMWB.opcode = opcode(state.EXMEM.instr);
        newState.MEMWB.dest = state.EXMEM.dest;
        newState.MEMWB.writeData = state.EXMEM.aluResult;
        if (opcode(state.EXMEM.instr) == LW) {
            newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
        }
        else if (opcode(state.EXMEM.instr) == SW) {
            newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.readRegB;
        }
        else if (opcode(state.EXMEM.instr) == BEQ) {
            // If Taken:
            if (state.EXMEM.eq == 1) { 
                newState.IFID.instr = 0x1c00000;
                newState.IDEX.instr = 0x1c00000;
                newState.EXMEM.instr = 0x1c00000;
                newState.pc = state.EXMEM.branchTarget;
            }
        }
        

        /* ---------------------- WB stage --------------------- */
        newState.WBEND.instr = state.MEMWB.instr;
        newState.WBEND.dest = state.MEMWB.dest;
        newState.WBEND.writeData = state.MEMWB.writeData;
        if (state.MEMWB.opcode == ADD || state.MEMWB.opcode == NOR || state.MEMWB.opcode == LW) {
            newState.reg[state.MEMWB.dest] = state.MEMWB.writeData;
        }



        /* ------------------------ END ------------------------ */
        state = newState; /* this is the last statement before end of the loop. It marks the end 
        of the cycle and updates the current state with the values calculated in this cycle */
    }
    printf("machine halted\n");
    printf("total of %d cycles executed\n", state.cycles);
    printf("final state of machine:\n");
    printState(&state);
}

void printInstruction(int instr) {
    switch (opcode(instr)) {
        case ADD:
            printf("add");
            break;
        case NOR:
            printf("nor");
            break;
        case LW:
            printf("lw");
            break;
        case SW:
            printf("sw");
            break;
        case BEQ:
            printf("beq");
            break;
        case JALR:
            printf("jalr");
            break;
        case HALT:
            printf("halt");
            break;
        case NOOP:
            printf("noop");
            break;
        default:
            printf(".fill %d", instr);
            return;
    }
    printf(" %d %d %d", field0(instr), field1(instr), field2(instr));
}

void printState(stateType *statePtr) {
    printf("\n@@@\n");
    printf("state before cycle %d starts:\n", statePtr->cycles);
    printf("\tpc = %d\n", statePtr->pc);

    printf("\tdata memory:\n");
    for (int i=0; i<statePtr->numMemory; ++i) {
        printf("\t\tdataMem[ %d ] = %d\n", i, statePtr->dataMem[i]);
    }
    printf("\tregisters:\n");
    for (int i=0; i<NUMREGS; ++i) {
        printf("\t\treg[ %d ] = %d\n", i, statePtr->reg[i]);
    }

    // IF/ID
    printf("\tIF/ID pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IFID.instr);
    printInstruction(statePtr->IFID.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IFID.pcPlus1);
    if(opcode(statePtr->IFID.instr) == NOOP){
        printf(" (Don't Care)");
    }
    
    printf("\n");
    
    // ID/EX
    int idexOp = opcode(statePtr->IDEX.instr);
    printf("\tID/EX pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->IDEX.instr);
    printInstruction(statePtr->IDEX.instr);
    printf(" )\n");
    printf("\t\tpcPlus1 = %d", statePtr->IDEX.pcPlus1);
    if(idexOp == NOOP){
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegA = %d", statePtr->IDEX.readRegA);
    if (idexOp >= HALT || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->IDEX.readRegB);
    if(idexOp == LW || idexOp > BEQ || idexOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\toffset = %d", statePtr->IDEX.offset);
    if (idexOp != LW && idexOp != SW && idexOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // EX/MEM
    int exmemOp = opcode(statePtr->EXMEM.instr);
    printf("\tEX/MEM pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->EXMEM.instr);
    printInstruction(statePtr->EXMEM.instr);
    printf(" )\n");
    printf("\t\tbranchTarget %d", statePtr->EXMEM.branchTarget);
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\teq ? %s", (statePtr->EXMEM.eq ? "True" : "False"));
    if (exmemOp != BEQ) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\taluResult = %d", statePtr->EXMEM.aluResult);
    if (exmemOp > SW || exmemOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");
    printf("\t\treadRegB = %d", statePtr->EXMEM.readRegB);
    if (exmemOp != SW) {
        printf(" (Don't Care)");
    }
    printf("\n");

    // MEM/WB
	int memwbOp = opcode(statePtr->MEMWB.instr);
    printf("\tMEM/WB pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->MEMWB.instr);
    printInstruction(statePtr->MEMWB.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->MEMWB.writeData);
    if (memwbOp >= SW || memwbOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");     

    // WB/END
	int wbendOp = opcode(statePtr->WBEND.instr);
    printf("\tWB/END pipeline register:\n");
    printf("\t\tinstruction = %d ( ", statePtr->WBEND.instr);
    printInstruction(statePtr->WBEND.instr);
    printf(" )\n");
    printf("\t\twriteData = %d", statePtr->WBEND.writeData);
    if (wbendOp >= SW || wbendOp < 0) {
        printf(" (Don't Care)");
    }
    printf("\n");

    printf("end state\n");
}

// File
#define MAXLINELENGTH 1000 // MAXLINELENGTH is the max number of characters we read

void readMachineCode(stateType *state, char* filename) {
    char line[MAXLINELENGTH];
    FILE *filePtr = fopen(filename, "r");
    if (filePtr == NULL) {
        printf("error: can't open file %s", filename);
        exit(1);
    }

    printf("instruction memory:\n");
    for (state->numMemory = 0; fgets(line, MAXLINELENGTH, filePtr) != NULL; ++state->numMemory) {
        if (sscanf(line, "%d", state->instrMem+state->numMemory) != 1) {
            printf("error in reading address %d\n", state->numMemory);
            exit(1);
        }
        printf("\tinstrMem[ %d ] = ", state->numMemory);
        printInstruction(state->dataMem[state->numMemory] = state->instrMem[state->numMemory]);
        printf("\n");
    }
}