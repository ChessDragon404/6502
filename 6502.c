#include <stdio.h>
#include <stdlib.h>

typedef unsigned char Byte;
typedef unsigned short Word;

Word PC;
Byte SP;
Byte A, X, Y;
Byte PF; // N V N/A B D I Z C
Byte Memory[64 * 1024];
Byte PageCrossed = 0;

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
    printf("%x\n", word);
    return word;
}

Word ReadWord (Word Address){
    Byte LSB = Memory[Address];
    Byte MSB = Memory[Address + 1];
    Word word = LSB + (MSB << 8);
    printf("%x\n", word);
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
    return (PF & 0b10000000) >> 7;
}

void SetNegative () {
    PF |= 0b10000000;
}

void ClearNegative () {
    PF &= ~0b10000000;
}

void cycle(int cycles){

}

void ADC(Byte Data){
    if (CheckDecimalMode()){
        Word L = (A & 0x0F) + (Data & 0x0F) + CheckCarry();
        Word U = ((A & 0xF0) + (Data & 0xF0)) >> 4;
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
    int should_carry = A + Data + CheckCarry() > 0b11111111;
    if (((A < 0b10000000) && (Data < 0b10000000)) && ((A + Data + CheckCarry) >= 0b10000000)){
        SetOverflow();
    } else if (((A >= 0b10000000) && (Data >= 0b10000000)) && ((Byte)(A + Data + CheckCarry) < 0b10000000)){
        SetOverflow();
    } else {
        ClearOverflow();
    }
    A = (Byte)(A + Data + CheckCarry());
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
    should_carry ? SetCarry() : ClearCarry();
}

void AND(Byte Data){
    A &= Data;
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
}

void LDA(Byte Data){
    A = Data;
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearZero();
}

void LDX(Byte Data){
    X = Data;
    X == 0 ? SetZero() : ClearZero();
    X >> 7 ? SetNegative() : ClearZero();
}

void LDY(Byte Data){
    Y = Data;
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

void BIT(Byte Data){
    (Data & A) == 0 ? SetZero() : ClearZero();
    Data >> 7 ? SetNegative() : ClearNegative();
    (Data & 0b01000000) >> 6 ? SetOverflow() : ClearOverflow();
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

void CMP(Byte Data){
    A >= Data ? SetCarry() : ClearCarry();
    A == Data ? SetZero() : ClearZero();
    (int)(A - Data) < 0 ? SetNegative() : ClearNegative();
}

void CPX(Byte Data){
    X >= Data ? SetCarry() : ClearCarry();
    X == Data ? SetZero() : ClearZero();
    (int)(X - Data) < 0 ? SetNegative() : ClearNegative();
}

void CPY(Byte Data){
    Y >= Data ? SetCarry() : ClearCarry();
    Y == Data ? SetZero() : ClearZero();
    (int)(Y - Data) < 0 ? SetNegative() : ClearNegative();
}

void DEC(Byte *Data){
    (*Data)--;
    *Data == 0 ? SetZero() : ClearZero();
    *Data >> 7 ? SetNegative() : ClearNegative();
}

void EOR(Byte Data){
    A ^= Data;
    A == 0 ? SetZero() : ClearZero();
    A >> 7 ? SetNegative() : ClearNegative();
}

void INC(Byte* Data){
    (*Data)++;
    *Data == 0 ? SetZero() : ClearZero();
    *Data >> 7 ? SetNegative() : ClearNegative();
}

void ORA(Byte Data){
    A |= Data;
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

Byte* IndirectX (){
    Byte ZeroPageAddr = FetchByte();
    ZeroPageAddr += X;
    ZeroPageAddr &= 0xFF;
    Word Addr = ReadWord(ZeroPageAddr);
    return ReadByte(Addr);
}

Byte* IndirectY (){
    Byte ZeroAddr = FetchByte();
    Word Addr = ReadWord(ZeroAddr);
    Addr += Y;
    PageCrossed = PC >> 8 != Addr >> 8;
    return ReadByte(Addr);
}

Byte* ZeroPage (){
    Byte Addr = FetchByte();
    return ReadByte(Addr);
}

Byte* ZeroPageX (){
    Byte baseAddr = FetchByte();
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

void Execute (int cycles) {
    int numCycles = 0;
    while (numCycles < cycles){
        Byte Ins = FetchByte();
        switch (Ins)
        {
        case 0x00: //BRK
        {
            Memory[0x0100 + SP--] = (PC & 0xFF00) >> 2;
            Memory[0x0100 + SP--] = PC & 0x00FF;
            Memory[0x0100 + SP--] = PF;
            PC = Memory[0xFFFE] + (Memory[0xFFFF] << 8);
            SetBreak();
            cycle(7);
        } break;
        case 0x01: //ORA Indirect, X
        {
            ORA(*IndirectX());
        } break;
        case 0x05: //ORA Zero Page
        {
            ORA(*ZeroPage());
            cycle(3);
        } break;
        case 0x06: //ASL Zero Page
        {
            ASL(ZeroPage());
            cycle(5);
        } break;
        case 0x08: //PHP
        {
            Memory[0x0100 & SP--] = PF;
            cycle(3);
        } break;
        case 0x09: //ORA Immediate
        {
            ORA(*FetchByte());
            cycle(2);
        } break;
        case 0x0A: //ASL Accumulator
        {
            ASL(&A);
            cycle(2);
        } break;
        case 0x0D: //ORA Absolute
        {
            ORA(*Absolute());
            cycle(4);
        } break;
        case 0x0E: //ASL Absolute
        {
            ASL(Absolute());
            cycle(6);
        } break;
        case 0x10: //BPL Relative
        {
            Branch(*FetchByte(), !CheckNegative());
        } break;
        case 0x11: //ORA Indirect, Y
        {
            ORA(*IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0x15: //ORA Zero Page, X
        {
            ORA(*ZeroPageX());
            cycle(4);
        } break;
        case 0x16: //ASL Zero Page, X
        {
            ASL(ZeroPageX());
            cycle(6);
        } break;
        case 0x18: //CLC Implied
        {
            ClearCarry();
            cycle(2);
        } break;
        case 0x19: //ORA Absolute, Y
        {
            ORA(*AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0x1D: //ORA Absolute, X
        {
            ORA(*AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0x1E: //ASL Absolute, X
        {
            ASL(AbsoluteX());
            cycle(7);
        } break;
        case 0x20: //JSR Absolute
        {
            Memory[0x0100 + SP--] = (PC - 1) & 0xFF;
            Memory[0x0100 + SP--] = (PC - 1) >> 8;
            PC = FetchWord();
            cycle(6);
        } break;
        case 0x21: //AND Indirect, X
        {
            AND(*IndirectX());
            cycle(6);
        } break;
        case 0x24: //BIT Zero Page
        {
            BIT(*ZeroPage());
            cycle(3);
        } break;
        case 0x25: //AND Zero Page
        {
            AND(*ZeroPage());
            cycle(3);
        } break;
        case 0x26: //ROL Zero Page
        {
            ROL(ZeroPage());
            cycle(5);
        } break;
        case 0x28: //PLP
        {
            PF = Memory[0x0100 + SP++];
            cycle(4);
        } break;
        case 0x29: //AND Immediate
        {
            AND(*FetchByte());
            cycle(2);
        } break;
        case 0x2A: //ROL A
        {
            ROL(&A);
            cycle(2);
        } break;
        case 0x2C: //BIT Absolute
        {
            BIT(*Absolute());
            cycle(4);
        } break;
        case 0x2D: //AND Absolute
        {
            AND(*Absolute());
            cycle(4);
        } break;
        case 0x2E: //ROL Absolute
        {
            ROL(Absolute());
            cycle(6);
        } break;
        case 0x30: //BMI Relative
        {
            Branch(*FetchByte(), CheckNegative());
        } break;
        case 0x31: //AND Indirect, Y
        {
            AND(*IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0x35: //AND Zero Page, X
        {
            AND(*ZeroPageX());
            cycle(4);
        } break;
        case 0x36: //ROL Zero Page, X
        {
            ROL(ZeroPageX());
            cycle(6);
        } break;
        case 0x39: //AND Absolute, Y
        {
            AND(*AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0x3D: //AND Absolute, X
        {
            AND(*AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0x3E: //ROL Absolute, X
        {
            ROL(AbsoluteX());
            cycle(7);
        } break;
        case 0x40: //RTI
        {
            PF = Memory[0x0100 + SP++];
            PC = Memory[0x0100 + SP++] & (Memory[0x0100 + SP++] << 8);
            cycle(6);
        } break;
        case 0x41: //EOR Indirect, X
        {
            EOR(*IndirectX());
            cycle(6);
        } break;
        case 0x45: //EOR Zero Page
        {
            EOR(*ZeroPage());
            cycle(3);
        } break;
        case 0x46: //LSR Zero Page
        {
            LSR(ZeroPage());
            cycle(5);
        } break;
        case 0x48: //PHA
        {
            Memory[0x0100 + SP--] = A;
            cycle(3);
        } break;
        case 0x49: //EOR Immediate
        {
            EOR(*FetchByte());
            cycle(2);
        } break;
        case 0x4A: //LSR Accumulator
        {
            LSR(&A);
            cycle(2);
        } break;
        case 0x4C: //JMP Absolute
        {
            PC = *Absolute();
            cycle(3);
        } break;
        case 0x4E: //LSR Absolute
        {
            LSR(Absolute());
            cycle(6);
        } break;
        case 0x4D: //EOR Absolute
        {
            EOR(*Absolute());
            cycle(4);
        } break;
        case 0x50: //BVC Relative
        {
            Branch(*FetchByte(), !CheckOverflow());
        } break;
        case 0x51: //EOR Indirect, Y
        {
            EOR(*IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0x55: //EOR Zero Page, X
        {
            EOR(*ZeroPageX());
            cycle(4);
        } break;
        case 0x56: //LSR Zero Page, X
        {
            LSR(ZeroPageX());
            cycle(6);
        } break;
        case 0x58: //CLI Implied
        {
            ClearInterruptDisable();
            cycle(2);
        } break;
        case 0x59: //EOR Absolute, Y
        {
            EOR(*AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0x5D: //EOR Absolute, X
        {
            EOR(*AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0x5E: //LSR Absolute, X
        {
            LSR(AbsoluteX());
            cycle(7);
        } break;
        case 0x60: //RTS
        {
            PC = Memory[0x0100 + SP++] | (Memory[0x0100 + SP++] << 8);
            cycle(6);
        } break;
        case 0x61: //ADC Indirect, X
        {
            ADC(*IndirectX());
            cycle(6);
        } break;
        case 0x65: //ADC Zero Page
        {
            ADC(*ZeroPage());
            cycle(3);
        } break;
        case 0x66: //ROR Zero Page
        {
            ROR(ZeroPage());
            cycle(5);
        } break;
        case 0x68: //PLA
        {
            A = Memory[0x0100 + SP++];
            (A == 0) ? SetZero() : ClearZero();
            (A >> 7) ? SetNegative() : ClearNegative();
            cycle(4);
        }
        case 0x69: //ADC Immediate
        {
            ADC(*FetchByte());
            cycle(2);
        } break;
        case 0x6A: //ROR Accumulator
        {
            ROR(&A);
            cycle(2);
        } break;
        case 0x6C: //JMP Indirect
        {
            Word InitAddr = FetchWord();
            Word Addr = Memory[InitAddr] << 8 + Memory[InitAddr + 1];
            PC = Addr;
            cycle(5);
        } break;
        case 0x6D: //ADC Absolute
        {
            ADC(*Absolute());
            cycle(4);
        } break;
        case 0x6E: //ROR Absolute
        {
            ROR(Absolute());
            cycle(7);
        } break;
        case 0x70: //BVS Relative
        {
            Branch(*FetchByte(), CheckOverflow());
        } break;
        case 0x71: //ADC Indirect, Y
        {
            ADC(*IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0x75: //ADC Zero Page, X
        {
            ADC(*ZeroPageX());
            cycle(4);
        } break;
        case 0x76: //ROR Zero Page, X
        {
            ROR(ZeroPageX());
            cycle(6);
        } break;
        case 0x79: //ADC Absolute, Y
        {
            ADC(*AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0x7D: //ADC Absolute, X
        {
            ADC(*AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0x88: //DEY Implied
        {
            Y--;
            if (Y == 0){
                SetZero();
            } else {
                ClearZero();
            }
            if (Y & 0x80 >> 7){
                SetNegative();
            } else {
                ClearNegative();
            }
            cycle(2);
        } break;
        case 0x90: //BCC Relative
        {
            Branch(*FetchByte(), !CheckCarry());
        } break;
        case 0xA0: //LDY Immediate
        {
            LDY(*FetchByte());
            cycle(2);
        } break;
        case 0xA1: //LDA Indirect, X
        {
            LDA(*IndirectX());
            cycle(6);
        } break;
        case 0xA2: //LDX Immediate
        {
            LDX(*FetchByte());
            cycle(2);
        } break;
        case 0xA4: //LDY Zero Page
        {
            LDY(*ZeroPage());
            cycle(3);
        } break;
        case 0xA5: //LDA Zero Page
        {
            LDA(*ZeroPage());
            cycle(3);
        } break;
        case 0xA6: //LDX Zero Page
        {
            LDX(*ZeroPage());
            cycle(3);
        } break;
        case 0xA9: //LDA Immediate
        {
            LDA(*FetchByte());
            cycle(2);
        } break;
        case 0xAC: //LDY Absolute
        {
            LDY(*Absolute());
        } break;
        case 0xAD: //LDA Absolute
        {
            LDA(*Absolute());
            cycle(4);
        } break;
        case 0xAE: //LDX Absolute
        {
            LDX(*Absolute());
            cycle(4);
        } break;
        case 0xB0: //BCS Relative
        {
            Branch(*FetchByte(), CheckCarry());
        } break;
        case 0xB1: //LDA Indirect, Y
        {
            LDA(*IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0xB4: //LDY Zero Page, X
        {
            LDY(*ZeroPageX());
            cycle(4);
        } break;
        case 0xB5: //LDA Zero Page, X
        {
            LDA(*ZeroPageX());
            cycle(4);
        } break;
        case 0xB6: //LDX Zero Page, Y
        {
            LDX(ReadByte((*FetchByte() + Y) & 0xFF));
            cycle(4);
        } break;
        case 0xB8: //CLV Implied
        {
            ClearOverflow();
            cycle(2);
        } break;
        case 0xB9: //LDA Absolute, Y
        {
            LDA(*AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0xBC: //LDY Absolute, X
        {
            LDY(*AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0xBD: //LDA Absolute, X
        {
            LDA(*AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0xBE: //LDX Absolute, Y
        {
            LDX(*AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0xC0: //CPY Immediate
        {
            CPY(*FetchByte());
            cycle(2);
        } break;
        case 0xC1: //CMP Indirect, X
        {
            CMP(*IndirectX());
            cycle(6);
        } break;
        case 0xC4: //CPY Zero Page
        {
            CPY(*ZeroPage());
            cycle(3);
        } break;
        case 0xC5: //CMP Zero Page
        {
            CMP(*ZeroPage());
            cycle(3);
        } break;
        case 0xC6: //DEC Zero Page
        {
            DEC(ZeroPage());
            cycle(6);
        } break;
        case 0xC8: //INY Implied
        {
            Y++;
            Y == 0 ? SetZero() : ClearZero();
            Y >> 7 ? SetNegative() : ClearNegative();
            cycle(2);
        } break;
        case 0xC9: //CMP Immediate
        {
            CMP(*FetchByte());
            cycle(2);
        } break;
        case 0xCA: //DEX Implied
        {
            X--;
            X == 0 ? SetZero() : ClearZero();
            X >> 7 ? SetNegative() : ClearNegative();
            cycle(2);
        } break;
        case 0xCC: //CPY Absolute
        {
            CPY(*Absolute());
            cycle(4);
        } break;
        case 0xCD: //CMP Absolute
        {
            CMP(*Absolute());
            cycle(4);
        } break;
        case 0xCE: //DEC Absolute
        {
            DEC(Absolute());
            cycle(6);
        } break;
        case 0xD0: //BNE Relative
        {
            Branch(*FetchByte(), !CheckZero());
        } break;
        case 0xD1: //CMP Indirect, Y
        {
            CMP(*IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0xD5: //CMP Zero Page, X
        {
            CMP(*ZeroPageX());
            cycle(4);
        } break;
        case 0xD6: //DEC Zero Page, X
        {
            DEC(ZeroPageX());
            cycle(6);
        } break;
        case 0xD8: //CLD Implied
        {
            ClearDecimalMode();
            cycle(2);
        } break;
        case 0xD9: //CMP Absolute, Y
        {
            CMP(*AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0xDD: //CMP Absolute, X
        {
            CMP(*AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0xDE: //DEC Absolute, X
        {
            DEC(AbsoluteX());
            cycle(7);
        } break;
        case 0xE0: //CPX Immediate
        {
            CPX(*FetchByte());
            cycle(2);
        } break;
        case 0xE4: //CPX Zero Page
        {
            CPX(*ZeroPage());
            cycle(3);
        } break;
        case 0xE6: //INC Zero Page
        {
            INC(ZeroPage());
            cycle(5);
        } break;
        case 0xE8: //INX Implied
        {
            X++;
            X == 0 ? SetZero() : ClearZero();
            X >> 7 ? SetNegative() : ClearNegative();
            cycle(2);
        } break;
        case 0xEA: //NOP
        {
            cycle(2);
        } break;
        case 0xEC: //CPX Absolute
        {
            CPX(*Absolute());
            cycle(4);
        } break;
        case 0xEE: //INC Absolute
        {
            INC(Absolute());
            cycle(6);
        } break;
        case 0xF0: //BEQ Relative
        {
            Branch(*FetchByte(), CheckZero());
        } break;
        case 0xF6: //INC Zero Page, X
        {
            INC(ZeroPageX());
            cycle(6);
        } break;
        case 0xFE: //INC Absolute, X
        {
            INC(AbsoluteX());
        } break;
        default:
            break;
        }
        numCycles++;
    }
}

void printBits(Byte Data){
    printf("%d%d%d%d%d%d%d%d",Data>>7,Data>>6&0b01,Data>>5&1,Data>>4&1,Data>>3&1,Data>>2&1,Data>>1&1,Data&1);
}

void printRegisters(){
    printf("A = %X, X = %X, Y = %X, SP = %X, PC = %X, SF = ", A, X, Y, SP, PC);
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

int main(void){
    SetInterruptDisable();
    ClearDecimalMode();
    PC = ReadWord(0xFFFC);
    Memory[0x0000] = 0x88;
    Execute(1);
    printStack();
    printRegisters();
}