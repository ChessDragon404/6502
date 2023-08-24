#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef unsigned char Byte; //1 Byte
typedef unsigned short Word; //2 Bytes

typedef struct Instruction {
    void (*Ins)(Byte*);
    Byte* (*Adr)();
    Byte cycles;
    Byte UsePageCrossed;
} Instruction;

Instruction instructions[0xFF] = {0};


Word PC; // Program Counter
Byte SP; // Stack Pointer
Byte X, Y; //Index Register
Byte A; //Accumulator
Byte PF; //Program Flags N(-) V(overflow) N/A B(Break) D(decimal) I(disable interrupt) Z(0) C(carry)*/
Byte Memory[64 * 1024]; //64 KiB of Memory
Byte IRQ; //Interrupt Request
Byte NMI; //Non Maskable Interrupt
Byte PageCrossed = 0;

struct timespec timer;

Byte* FetchByte (){
    return &Memory[PC++];
}

Byte* ReadByte (Word Address){
    return &Memory[Address];
}

Word FetchWord (){
    Byte LSB = Memory[PC++];
    Byte MSB = Memory[PC++];
    Word word = LSB + (MSB << 8);
    return word;
}

Word ReadWord (Word Address){
    Byte LSB = Memory[Address];
    Byte MSB = Memory[Address + 1];
    Word word = LSB + (MSB << 8);
    return word;
}

Byte CheckCarry () {
    return (PF & 0b00000001);
}

void SetCarry () {
    PF |= 0b00000001;
}

void ClearCarry () {
    PF &= ~0b00000001;
}

Byte CheckZero () {
    return (PF & 0b00000010) >> 1;
}

void SetZero () {
    PF |= 0b00000010;
}

void ClearZero () {
    PF &= ~0b00000010;
}

Byte CheckInterruptDisable () {
    return (PF & 0b00000100) >> 2;
}

void SetInterruptDisable () {
    PF |= 0b00000100;
}

void ClearInterruptDisable () {
    PF &= ~0b00000100;
}

Byte CheckDecimalMode () {
    return (PF & 0b00001000) >> 3;
}

void SetDecimalMode () {
    PF |= 0b00001000;
}

void ClearDecimalMode () {
    PF &= ~0b00001000;
}

Byte CheckBreak () {
    return (PF & 0b00010000) >> 4;
}

void SetBreak () {
    PF |= 0b00010000;
}

void ClearBreak () {
    PF &= ~0b00010000;
}

Byte CheckOverflow () {
    return (PF & 0b01000000) >> 6;
}

void SetOverflow () {
    PF |= 0b01000000;
}

void ClearOverflow () {
    PF &= ~0b01000000;
}

Byte CheckNegative () {
    return PF >> 7;
}

void SetNegative () {
    PF |= 0b10000000;
}

void ClearNegative () {
    PF &= ~0b10000000;
}

// call this function to start a nanosecond-resolution timer
struct timespec timer_start(){
    struct timespec start_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_time);
    return start_time;
}

// call this function to end a timer, returning nanoseconds elapsed as a long
long timer_end(struct timespec start_time){
    struct timespec end_time;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
    long diffInNanos = (end_time.tv_sec - start_time.tv_sec) * (long)1e9 + (end_time.tv_nsec - start_time.tv_nsec);
    return diffInNanos;
}

void cycle(int cycles){
    long ns = timer_end(timer);
    for (int i = 0; i < (cycles * 600 - ns) / 250; i++){
        timer_end(timer);
    }
}

void HandleIRQ(){
    Memory[0x0100 + SP++] = PC & 0xFF;
    Memory[0x0100 + SP++] = PC >> 8;
    Memory[0x0100 + SP++] = PF;
    PC = ReadWord(0xFFFE);
    cycle(7);
}

void HandleNMI(){
    Memory[0x0100 + SP++] = PC & 0xFF;
    Memory[0x0100 + SP++] = PC >> 8;
    Memory[0x0100 + SP++] = PF;
    PC = ReadWord(0xFFFA);
    cycle(7);
}

void ADC(Byte* Data){
    if (CheckDecimalMode()){
        Word L = (A & 0x0F) + (*Data & 0x0F) + CheckCarry();
        Word U = ((A & 0xF0) + (*Data & 0xF0)) >> 4;
        if (L >= 0x0A){
            U++;
            L -= 0x0A;
        }
        if (U >= 0x0A){
            U -= 0x0A;
            SetCarry();
        } else {
            ClearCarry();
        }
        A = L | (U << 4);
        A >> 7 ? SetNegative() : ClearNegative();
        char signedA = *(char *)&A;
        signedA < -128 || signedA > 127 ? SetOverflow() : ClearOverflow();
        return;
    }
    int should_carry = A + *Data + CheckCarry() > 0b11111111;
    if (((A < 0x80) && (*Data < 0x80)) && ((A + *Data + CheckCarry() >= 0x80))){
        SetOverflow();
    } else if (((A >= 0b10000000) && ((*Data) >= 0b10000000)) && ((Byte)(A + (*Data) + CheckCarry() < 0b10000000))){
        SetOverflow();
    } else {
        ClearOverflow();
    }
    A = (Byte)(A + *Data + CheckCarry());
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
    should_carry ? SetCarry() : ClearCarry();
}

void SBC(Byte *Data){
    Byte tmp = A + (~*Data + 1) + ~(1 - CheckCarry()) + 1;
    if (CheckDecimalMode()){
        ((A >> 7 && !(*Data >> 7)) && !(tmp >> 7)) || ((!(A >> 7) && *Data >> 7)  && tmp >> 7) ? SetOverflow() : ClearOverflow();
        Byte L = (A & 0xF) - (*Data & 0xF) - (1 - CheckCarry());
        Byte U = (A & 0xF0) - (*Data & 0xF0);
        if (L < 0){
            L = 10 - L;
            U--;
        }
        U < 0 ? ClearCarry() : SetCarry();
        A = (U << 4) + L;
        !A ? SetZero() : ClearZero();
        A >> 7 ? SetNegative() : ClearNegative();
        return;
    }
    int should_carry = (Word)(A + (~*Data + 1) + ~(1 - CheckCarry()) + 1) > 0xFF;
    ((A >> 7 && !(*Data >> 7)) && !(tmp >> 7)) || ((!(A >> 7) && *Data >> 7)  && tmp >> 7) ? SetOverflow() : ClearOverflow();
    A += (~*Data + 1) + ~(1 - CheckCarry()) + 1;
    A ? ClearZero() : SetZero();
    A >> 7 ? SetNegative() : ClearNegative();
    !should_carry ? SetCarry() : ClearNegative();
}

void AND(Byte *Data){
    A &= *Data;
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
}

void LDA(Byte *Data){
    A = *Data;
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearZero();
}

void LDX(Byte *Data){
    X = *Data;
    X == 0 ? SetZero() : ClearZero();
    X >> 7 ? SetNegative() : ClearZero();
}

void LDY(Byte *Data){
    Y = *Data;
    Y == 0 ? SetZero() : ClearZero();
    Y >> 7 ? SetNegative() : ClearZero();
}

void ASL(Byte* Data){
    unsigned int tmp = (*Data << 1);
    (tmp & 0b100000000) >> 8 ? SetCarry() : ClearCarry();
    *Data <<= 1;
    *Data == 0 ? SetZero() : ClearZero();
    *Data >> 7 ? SetNegative() : ClearNegative();
}

void LSR(Byte* Data){
    *Data & 1 ? SetCarry() : ClearCarry();
    *Data >>= 1;
    *Data == 0 ? SetZero() : ClearZero();
    *Data >> 7 ? SetNegative() : ClearNegative();
}

void BIT(Byte *Data){
    (*Data & A) == 0 ? SetZero() : ClearZero();
    *Data >> 7 ? SetNegative() : ClearNegative();
    (*Data & 0b01000000) >> 6 ? SetOverflow() : ClearOverflow();
}

void Branch(Byte uOffset, Byte Condition){
    int cycles = 2;
    char Offset = *(char *)&uOffset;
    if (Condition){
        Word tmp = PC + Offset;
        if (tmp >> 8 == PC >> 8){
            cycles++;
        } else {
            cycles += 2;
        }
        PC = tmp;
    }
    cycle(cycles);
}

void CMP(Byte* Data){
    A >= *Data ? SetCarry() : ClearCarry();
    A == *Data ? SetZero() : ClearZero();
    (int)(A - *Data) < 0 ? SetNegative() : ClearNegative();
}

void CPX(Byte* Data){
    X >= *Data ? SetCarry() : ClearCarry();
    X == *Data ? SetZero() : ClearZero();
    (int)(X - *Data) < 0 ? SetNegative() : ClearNegative();
}

void CPY(Byte* Data){
    Y >= *Data ? SetCarry() : ClearCarry();
    Y == *Data ? SetZero() : ClearZero();
    (int)(Y - *Data) < 0 ? SetNegative() : ClearNegative();
}

void DEC(Byte *Data){
    (*Data)--;
    *Data == 0 ? SetZero() : ClearZero();
    *Data >> 7 ? SetNegative() : ClearNegative();
}

void EOR(Byte *Data){
    A ^= *Data;
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
}

void INC(Byte* Data){
    (*Data)++;
    *Data == 0 ? SetZero() : ClearZero();
    *Data >> 7 ? SetNegative() : ClearNegative();
}

void ORA(Byte *Data){
    A |= *Data;
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
}

void ROL(Byte *Data){
    Byte C = CheckCarry();
    (*Data >> 7) ? SetCarry() : ClearCarry();
    *Data <<= 1;
    *Data |= C;
    *Data == 0 ? SetZero() : ClearZero();
}

void ROR(Byte *Data){
    Byte C = *Data & 1;
    *Data >>= 1;
    *Data |= CheckCarry() << 7;
    C ? SetCarry() : ClearCarry();
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
}

void BRK(Byte *Data){
    Memory[0x0100 + SP--] = (PC & 0xFF00) >> 8;
    Memory[0x0100 + SP--] = PC & 0x00FF;
    Memory[0x0100 + SP--] = PF;
    PC = Memory[0xFFFE] + (Memory[0xFFFF] << 8);
    SetBreak();
}

void PHP(Byte *Data){
    Memory[0x0100 & SP--] = PF;
}

void BPL(Byte *Data){
    Branch(*Data, !CheckNegative());
}

void CLC(Byte *Data){
    ClearCarry();
}

void JSR(Byte *Data){
    Memory[0x0100 + SP--] = (PC - 1) & 0xFF;
    Memory[0x0100 + SP--] = (PC - 1) >> 8;
    PC = FetchWord();
}

void PLP(Byte *Data){
    PF = Memory[0x0100 + ++SP];
}

void BMI(Byte *Data){
    Branch(*Data, CheckNegative());
}

void SEC(Byte *Data){
    SetCarry();
}

void RTI(Byte *Data){
    PF = Memory[0x0100 + ++SP];
    PC = Memory[0x0100 + ++SP];
    PC += (Memory[0x0100 + ++SP] << 8);
}

void PHA(Byte *Data){
    Memory[0x0100 + SP--] = A;
}

void JMPA(Byte *Data){
    PC = FetchWord();
}

void BVC(Byte *Data){
    Branch(*Data, !CheckOverflow());
}

void CLI(Byte *Data){
    ClearInterruptDisable();
}

void RTS(Byte *Data){
    PC = Memory[0x0100 + ++SP];
    PC += (Memory[0x0100 + ++SP] << 8);
}

void PLA(Byte *Data){
    A = Memory[0x0100 + ++SP];
    (A == 0) ? SetZero() : ClearZero();
    (A >> 7) ? SetNegative() : ClearNegative();
}

void JMPI(Byte *Data){
    Word InitAddr = FetchWord();
    Word Addr = (Memory[InitAddr] << 8) + Memory[InitAddr + 1];
    PC = Addr;
}

void BVS(Byte *Data){
    Branch(*Data, CheckOverflow());
}

void SEI(Byte *Data){
    SetInterruptDisable();
}

void STA(Byte *Data){
    *Data = A;
}

void STY(Byte *Data){
    *Data = Y;
}

void STX(Byte *Data){
    *Data = X;
}

void DEY(Byte *Data){
    Y--;
    Y == 0 ? SetZero() : ClearZero();
    Y >> 7 ? SetNegative() : ClearNegative();
}

void TXA(Byte *Data){
    A = X;
    !A ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearZero();
}

void BCC(Byte *Data){
    Branch(*Data, !CheckCarry());
}

void TYA(Byte *Data) {
    A = Y;
    !A ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
}

void TXS(Byte *Data){
    SP = X;
}

void TAY(Byte *Data){
    Y = A;
    !Y ? SetZero() : ClearZero();
    Y >> 7 ? SetNegative() : ClearNegative();
}

void TAX(Byte *Data){
    X = A;
    X ? ClearZero() : SetZero();
    X >> 7 ? SetNegative() : ClearNegative();
}

void BCS(Byte *Data){
    Branch(*Data, CheckCarry());
}

void CLV(Byte *Data){
    ClearOverflow();
}

void TSX(Byte *Data){
    X = SP;
    !X ? SetZero() : ClearZero();
    X >> 7 ? SetNegative() : ClearNegative();
}

void INY(Byte *Data){
    Y++;
    Y == 0 ? SetZero() : ClearZero();
    Y >> 7 ? SetNegative() : ClearNegative();
}

void DEX(Byte *Data){
    X--;
    X == 0 ? SetZero() : ClearZero();
    X >> 7 ? SetNegative() : ClearNegative();
}

void BNE(Byte *Data){
    Branch(*Data, !CheckZero());
}

void CLD(Byte *Data){
    ClearDecimalMode();
}

void INX(Byte *Data){
    X++;
    X == 0 ? SetZero() : ClearZero();
    X >> 7 ? SetNegative() : ClearNegative();
}

void NOP(Byte *Data){}

void BEQ(Byte *Data){
    Branch(*Data, CheckZero());
}

void SED(Byte *Data){
    SetDecimalMode();
}

Byte* IndirectX (){
    Byte ZeroPageAddr = *FetchByte();
    ZeroPageAddr += X;
    ZeroPageAddr &= 0xFF;
    Word Addr = ReadWord(ZeroPageAddr);
    return ReadByte(Addr);
}

Byte* IndirectY (){
    Byte ZeroAddr = *FetchByte();
    Word Addr = ReadWord(ZeroAddr);
    Addr += Y;
    PageCrossed = PC >> 8 != Addr >> 8;
    return ReadByte(Addr);
}

Byte* ZeroPage (){
    Byte Addr = *FetchByte();
    return ReadByte(Addr);
}

Byte* ZeroPageX (){
    Byte baseAddr = *FetchByte();
    Byte Addr = (baseAddr + X) & 0xFF;
    return ReadByte(Addr);
}

Byte* Absolute (){
    Word Addr = FetchWord();
    return ReadByte(Addr);
}

Byte* AbsoluteX (){
    Word Addr = FetchWord();
    Addr += X;
    PageCrossed = PC >> 8 != Addr >> 8;
    return ReadByte(Addr);
}

Byte* AbsoluteY (){
    Word Addr = FetchWord();
    Addr += Y;
    PageCrossed = PC >> 8 != Addr >> 8;
    return ReadByte(Addr);
}

Byte* Implied (){
    return 0;
}

Byte* Accumulator (){
    return &A;
}

Byte* ZeroPageY (){
    return &Memory[((*FetchByte() + Y) & 0xFF)];
}

void Execute () {
    timer = timer_start();
    if (IRQ && !CheckInterruptDisable()){
        HandleIRQ();
    }
    if (NMI){
        HandleNMI();
    }
    Byte I = *FetchByte();
    Instruction INS = instructions[I];
    INS.Ins(INS.Adr());
    cycle(INS.cycles + (INS.UsePageCrossed ? PageCrossed : 0));
}

void printBits(Byte Data){
    printf("%d%d%d%d%d%d%d%d",Data>>7,Data>>6&0b01,Data>>5&1,Data>>4&1,Data>>3&1,Data>>2&1,Data>>1&1,Data&1);
}

void printRegisters(){
    printf("A = %X, X = %X, Y = %X, SP = %X, PC = %X, PF = ", A, X, Y, SP, PC);
    printBits(PF);
    printf("\n");
}

void printStack(){
    for (int i = 0; i <= 0xF; i++){
        for (int j = 0; j <= 0xF; j++){
            printf("%02X ", Memory[0x0100 + (i << 4) + j]);
        }
        printf("\n");
    }
    printf("\n");
}

void assignInstructions(){
    instructions[0x00] = (Instruction){&BRK, &Implied, 7, 0};
    instructions[0x01] = (Instruction){&ORA, &IndirectX, 6, 0};
    instructions[0x05] = (Instruction){&ORA, &ZeroPage, 3, 0};
    instructions[0x06] = (Instruction){&ORA, &ZeroPage, 5, 0};
    instructions[0x08] = (Instruction){&PHP, &Implied, 3, 0};
    instructions[0x09] = (Instruction){&ORA, &FetchByte, 2, 0};
    instructions[0x0A] = (Instruction){&ASL, &Accumulator, 2, 0};
    instructions[0x0D] = (Instruction){&ORA, &Absolute, 4, 0};
    instructions[0x0E] = (Instruction){&ASL, &Absolute, 6, 0};
    instructions[0x10] = (Instruction){&BPL, &FetchByte, 0, 0};
    instructions[0x11] = (Instruction){&ORA, &IndirectY, 5, 1};
    instructions[0x15] = (Instruction){&ORA, &ZeroPageX, 4, 0};
    instructions[0x16] = (Instruction){&ASL, &ZeroPageX, 6, 0};
    instructions[0x18] = (Instruction){&CLC, &Implied, 2, 0};
    instructions[0x19] = (Instruction){&ORA, &AbsoluteY, 4, 1};
    instructions[0x1D] = (Instruction){&ORA, &AbsoluteX, 4, 1};
    instructions[0x1E] = (Instruction){&ASL, &AbsoluteX, 7, 0};
    instructions[0x20] = (Instruction){&JSR, &Implied, 6, 0};
    instructions[0x21] = (Instruction){&AND, &IndirectX, 6, 0};
    instructions[0x24] = (Instruction){&BIT, &ZeroPage, 3, 0};
    instructions[0x25] = (Instruction){&AND, &ZeroPage, 3, 0};
    instructions[0x26] = (Instruction){&ROL, &ZeroPage, 5, 0};
    instructions[0x28] = (Instruction){&PLP, &Implied, 4, 0};
    instructions[0x29] = (Instruction){&AND, &FetchByte, 2, 0};
    instructions[0x2A] = (Instruction){&ROL, &Accumulator, 2, 0};
    instructions[0x2C] = (Instruction){&BIT, &Absolute, 4, 0};
    instructions[0x2D] = (Instruction){&AND, &Absolute, 4, 0};
    instructions[0x2E] = (Instruction){&ROL, &Absolute, 6, 0};
    instructions[0x30] = (Instruction){&BMI, &FetchByte, 0, 0};
    instructions[0x31] = (Instruction){&AND, &IndirectY, 5, 1};
    instructions[0x35] = (Instruction){&AND, &ZeroPageX, 4, 0};
    instructions[0x36] = (Instruction){&ROL, &ZeroPageX, 6, 0};
    instructions[0x38] = (Instruction){&SEC, &Implied, 2, 0};
    instructions[0x39] = (Instruction){&AND, &AbsoluteY, 4, 1};
    instructions[0x3D] = (Instruction){&AND, &AbsoluteX, 4, 1};
    instructions[0x3E] = (Instruction){&ROL, &AbsoluteX, 7, 0};
    instructions[0x40] = (Instruction){&RTI, &Implied, 6, 0};
    instructions[0x41] = (Instruction){&EOR, &IndirectX, 6, 0};
    instructions[0x45] = (Instruction){&EOR, &ZeroPage, 3, 0};
    instructions[0x46] = (Instruction){&LSR, &ZeroPage, 5, 0};
    instructions[0x48] = (Instruction){&PHA, &Implied, 3, 0};
    instructions[0x49] = (Instruction){&EOR, &FetchByte, 2, 0};
    instructions[0x4A] = (Instruction){&LSR, &Accumulator, 2, 0};
    instructions[0x4C] = (Instruction){&JMPA, &Implied, 3, 0};
    instructions[0x4D] = (Instruction){&EOR, &Absolute, 4, 0};
    instructions[0x4E] = (Instruction){&LSR, &Absolute, 6, 0};
    instructions[0x50] = (Instruction){&BVC, &FetchByte, 0, 0};
    instructions[0x51] = (Instruction){&EOR, &IndirectY, 5, 1};
    instructions[0x55] = (Instruction){&EOR, &ZeroPageX, 4, 0};
    instructions[0x56] = (Instruction){&LSR, &ZeroPageX, 6, 0};
    instructions[0x58] = (Instruction){&CLI, &Implied, 2, 0};
    instructions[0x59] = (Instruction){&EOR, &AbsoluteY, 4, 1};
    instructions[0x5D] = (Instruction){&EOR, &AbsoluteX, 4, 1};
    instructions[0x5E] = (Instruction){&LSR, &AbsoluteX, 7, 0};
    instructions[0x60] = (Instruction){&RTS, &Implied, 6, 0};
    instructions[0x61] = (Instruction){&ADC, &IndirectX, 6, 0};
    instructions[0x65] = (Instruction){&ADC, &ZeroPage, 3, 0};
    instructions[0x66] = (Instruction){&ROR, &ZeroPage, 5, 0};
    instructions[0x68] = (Instruction){&PLA, &Implied, 4, 0};
    instructions[0x69] = (Instruction){&ADC, &FetchByte, 2, 0};
    instructions[0x6A] = (Instruction){&ROR, &Accumulator, 2, 0};
    instructions[0x6C] = (Instruction){&JMPI, &Implied, 5, 0};
    instructions[0x6D] = (Instruction){&ADC, &Absolute, 4, 0};
    instructions[0x6E] = (Instruction){&ROR, &Absolute, 6, 0};
    instructions[0x70] = (Instruction){&BVS, &FetchByte, 0, 0};
    instructions[0x71] = (Instruction){&ADC, &IndirectY, 5, 1};
    instructions[0x75] = (Instruction){&ADC, &ZeroPageX, 4, 0};
    instructions[0x76] = (Instruction){&ROR, &ZeroPageX, 6, 0};
    instructions[0x78] = (Instruction){&SEI, &Implied, 2, 0};
    instructions[0x79] = (Instruction){&ADC, &AbsoluteY, 4, 1};
    instructions[0x7D] = (Instruction){&ADC, &AbsoluteX, 4, 1};
    instructions[0x7E] = (Instruction){&ROR, &AbsoluteX, 7, 0};
    instructions[0x81] = (Instruction){&STA, &IndirectX, 6, 0};
    instructions[0x84] = (Instruction){&STY, &ZeroPage, 3, 0};
    instructions[0x85] = (Instruction){&STA, &ZeroPage, 3, 0};
    instructions[0x86] = (Instruction){&STX, &ZeroPage, 3, 0};
    instructions[0x88] = (Instruction){&DEY, &Implied, 2, 0};
    instructions[0x8A] = (Instruction){&TXA, &Implied, 2, 0};
    instructions[0x8C] = (Instruction){&STY, &Absolute, 4, 0};
    instructions[0x8D] = (Instruction){&STA, &Absolute, 4, 0};
    instructions[0x8E] = (Instruction){&STX, &Absolute, 4, 0};
    instructions[0x90] = (Instruction){&BCC, &FetchByte, 0, 0};
    instructions[0x91] = (Instruction){&STA, &IndirectY, 6, 0};
    instructions[0x94] = (Instruction){&STY, &ZeroPageX, 4, 0};
    instructions[0x95] = (Instruction){&STA, &ZeroPageX, 4, 0};
    instructions[0x96] = (Instruction){&STX, &ZeroPageY, 4, 0};
    instructions[0x98] = (Instruction){&TYA, &Implied, 2, 0};
    instructions[0x99] = (Instruction){&STA, &AbsoluteX, 5, 0};
    instructions[0x9A] = (Instruction){&TXS, &Implied, 2, 0};
    instructions[0x9D] = (Instruction){&STA, &AbsoluteX, 5, 0};
    instructions[0xA0] = (Instruction){&LDY, &FetchByte, 2, 0};
    instructions[0xA1] = (Instruction){&LDA, &IndirectX, 6, 0};
    instructions[0xA2] = (Instruction){&LDX, &FetchByte, 2, 0};
    instructions[0xA4] = (Instruction){&LDY, &ZeroPage, 3, 0};
    instructions[0xA5] = (Instruction){&LDA, &ZeroPage, 3, 0};
    instructions[0xA6] = (Instruction){&LDX, &ZeroPage, 3, 0};
    instructions[0xA8] = (Instruction){&TAY, &Implied, 2, 0};
    instructions[0xA9] = (Instruction){&LDA, &FetchByte, 2, 0};
    instructions[0xAA] = (Instruction){&TAX, &Implied, 2, 0};
    instructions[0xAC] = (Instruction){&LDY, &Absolute, 4, 0};
    instructions[0xAD] = (Instruction){&LDA, &Absolute, 4, 0};
    instructions[0xAE] = (Instruction){&LDX, &Absolute, 4, 0};
    instructions[0xB0] = (Instruction){&BCS, &FetchByte, 0, 0};
    instructions[0xB1] = (Instruction){&LDA, &IndirectY, 5, 1};
    instructions[0xB4] = (Instruction){&LDY, &ZeroPageX, 4, 0};
    instructions[0xB5] = (Instruction){&LDA, &ZeroPageX, 4, 0};
    instructions[0xB6] = (Instruction){&LDX, &ZeroPageY, 4, 0};
    instructions[0xB8] = (Instruction){&CLV, &Implied, 2, 0};
    instructions[0xB9] = (Instruction){&LDA, &AbsoluteY, 4, 1};
    instructions[0xBA] = (Instruction){&TSX, &Implied, 2, 0};
    instructions[0xBC] = (Instruction){&LDY, &AbsoluteX, 4, 1};
    instructions[0xBD] = (Instruction){&LDA, &AbsoluteX, 4, 1};
    instructions[0xBE] = (Instruction){&LDX, &AbsoluteY, 4, 1};
    instructions[0xC0] = (Instruction){&CPY, &FetchByte, 2, 0};
    instructions[0xC1] = (Instruction){&CMP, &IndirectX, 6, 0};
    instructions[0xC4] = (Instruction){&CPY, &ZeroPage, 3, 0};
    instructions[0xC5] = (Instruction){&CMP, &ZeroPage, 3, 0};
    instructions[0xC6] = (Instruction){&DEC, &ZeroPage, 5, 0};
    instructions[0xC8] = (Instruction){&INY, &Implied, 2, 0};
    instructions[0xC9] = (Instruction){&CMP, &FetchByte, 2, 0};
    instructions[0xCA] = (Instruction){&DEX, &Implied, 2, 0};
    instructions[0xCC] = (Instruction){&CPY, &Absolute, 4, 0};
    instructions[0xCD] = (Instruction){&CMP, &Absolute, 4, 0};
    instructions[0xCE] = (Instruction){&DEC, &Absolute, 6, 0};
    instructions[0xD0] = (Instruction){&BNE, &FetchByte, 0, 0};
    instructions[0xD1] = (Instruction){&CMP, &IndirectY, 5, 1};
    instructions[0xD5] = (Instruction){&CMP, &ZeroPageX, 4, 0};
    instructions[0xD6] = (Instruction){&DEC, &ZeroPageX, 6, 0};
    instructions[0xD8] = (Instruction){&CLD, &Implied, 2, 0};
    instructions[0xD9] = (Instruction){&CMP, &AbsoluteY, 4, 1};
    instructions[0xDD] = (Instruction){&CMP, &AbsoluteX, 4, 1};
    instructions[0xDE] = (Instruction){&DEC, &AbsoluteX, 7, 0};
    instructions[0xE0] = (Instruction){&CPX, &FetchByte, 2, 0};
    instructions[0xE1] = (Instruction){&SBC, &IndirectX, 6, 0};
    instructions[0xE4] = (Instruction){&CPX, &ZeroPage, 3, 0};
    instructions[0xE5] = (Instruction){&SBC, &ZeroPage, 3, 0};
    instructions[0xE6] = (Instruction){&INC, &ZeroPage, 5, 0};
    instructions[0xE8] = (Instruction){&INX, &Implied, 2, 0};
    instructions[0xE9] = (Instruction){&SBC, &FetchByte, 2, 0};
    instructions[0xEA] = (Instruction){&NOP, &Implied, 2, 0};
    instructions[0xEC] = (Instruction){&CPX, &Absolute, 4, 0};
    instructions[0xED] = (Instruction){&SBC, &Absolute, 4, 0};
    instructions[0xEE] = (Instruction){&INC, &Absolute, 6, 0};
    instructions[0xF0] = (Instruction){&BEQ, &FetchByte, 0, 0};
    instructions[0xF1] = (Instruction){&BEQ, &IndirectY, 5, 1};
    instructions[0xF5] = (Instruction){&SBC, &ZeroPageX, 4, 0};
    instructions[0xF6] = (Instruction){&INC, &ZeroPageX, 6, 0};
    instructions[0xF8] = (Instruction){&SED, &Implied, 2, 0};
    instructions[0xF9] = (Instruction){&SBC, &AbsoluteY, 4, 1};
    instructions[0xFD] = (Instruction){&SBC, &AbsoluteX, 4, 1};
    instructions[0xFE] = (Instruction){&SBC, &AbsoluteX, 7, 0};
}

int main(void){
    assignInstructions();
    IRQ = NMI = 0;
    SetInterruptDisable();
    ClearDecimalMode();
    PC = 0;
    SP = 0xFF;
    Memory[0x0000] = 0xB5;
    Memory[0x0001] = 0x69;
    Memory[0x0069] = 0x42;
    long ns[100000];
    struct timespec t;
    timer = timer_start();
    for (int i = 0; i < 100000; i++){
        t = timer_start();
        Execute();
        ns[i] = timer_end(t);
        PC = 0;
    }
    long sum = 0;
    for (int i = 0; i < 100000; i++){
        sum += ns[i];
    }
    sum /= 100000;
    printf("%ld\n", sum);
    printRegisters();
}