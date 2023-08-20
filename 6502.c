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

Byte FetchByte (){
    return Memory[PC++];
}

Byte ReadByte (Word Address){
    return Memory[Address];
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
        if ((A & 0b10000000) >> 7){
            SetNegative();
        } else {
            ClearNegative();
        }
        char signedA = *(char *)&A;
        if (signedA < -128 || signedA > 127){
            SetOverflow();
        } else {
            ClearOverflow();
        }
        return;
    }
    int should_carry;
    if (A + Data + CheckCarry() > 0b11111111){
        should_carry = 1;
    } else {
        should_carry = 0;
    }
    if (((A < 0b10000000) && (Data < 0b10000000)) && ((A + Data + CheckCarry) >= 0b10000000)){
        SetOverflow();
    } else if (((A >= 0b10000000) && (Data >= 0b10000000)) && ((Byte)(A + Data + CheckCarry) < 0b10000000)){
        SetOverflow();
    } else {
        ClearOverflow();
    }
    A = (Byte)(A + Data + CheckCarry());
    if (A == 0){
        SetZero();
    } else {
        ClearZero();
    }
    if ((A & 0b10000000) >> 7){
        SetNegative();
    } else {
        ClearNegative();
    }
    if (should_carry){
        SetCarry();
    } else {
        ClearCarry();
    }
}

void AND(Byte Data){
    A &= Data;
    if (A == 0){
        SetZero();
    } else {
        ClearZero();
    }
    if ((A & 0b10000000) >> 7){
        SetNegative();
    } else {
        ClearNegative();
    }
}

void LDA(Byte Data){
    A = Data;
    if (A == 0){
        SetZero();
    } else {
        ClearZero();
    }
    if ((A & 0b10000000) >> 7){
        SetNegative();
    } else {
        ClearZero();
    }
}

void ASL(Byte* Data){
    unsigned int tmp = (*Data << 1);
    if ((tmp & 0b100000000) >> 8){
        SetCarry();
    } else {
        ClearCarry();
    }
    *Data <<= 1;
    if (*Data == 0){
        SetZero();
    } else {
        ClearZero();
    }
    if ((*Data | 0b10000000) >> 7){
        SetNegative();
    } else {
        ClearNegative();
    }
}

void BIT(Byte Data){
    if ((Data & A) == 0){
        SetZero();
    } else {
        ClearZero();
    }
    if ((Data & 0b10000000) >> 7){
        SetNegative();
    } else {
        ClearNegative();
    }
    if ((Data & 0b01000000) >> 6){
        SetOverflow();
    } else {
        ClearOverflow();
    }
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
    if (A >= Data){
        SetCarry();
    } else {
        ClearCarry();
    }
    if (A == Data){
        SetZero();
    } else {
        ClearZero();
    }
    if ((int)(A - Data) < 0){
        SetNegative();
    } else {
        ClearNegative();
    }
}

void CPX(Byte Data){
    if (X >= Data){
        SetCarry();
    } else {
        ClearCarry();
    }
    if (X == Data){
        SetZero();
    } else {
        ClearZero();
    }
    if ((int)(X - Data) < 0){
        SetNegative();
    } else {
        ClearNegative();
    }
}

void CPY(Byte Data){
    if (Y >= Data){
        SetCarry();
    } else {
        ClearCarry();
    }
    if (Y == Data){
        SetZero();
    } else {
        ClearZero();
    }
    if ((int)(Y - Data) < 0){
        SetNegative();
    } else {
        ClearNegative();
    }
}

void DEC(Byte *Data){
    (*Data)--;
    if (*Data == 0){
        SetZero();
    } else {
        ClearZero();
    }
    if ((*Data) & 0x80 >> 7){
        SetNegative();
    } else {
        ClearNegative();
    }
}

void EOR(Byte Data){
    A ^= Data;
    if (A == 0){
        SetZero();
    } else {
        ClearZero();
    }
    if (A & 0x80 >> 7){
        SetNegative();
    } else {
        ClearNegative();
    }
}

Byte IndirectX (){
    Byte ZeroPageAddr = FetchByte();
    ZeroPageAddr += X;
    ZeroPageAddr &= 0xFF;
    Word Addr = ReadWord(ZeroPageAddr);
    return ReadByte(Addr);
}

Byte IndirectY (){
    Byte ZeroAddr = FetchByte();
    Word Addr = ReadWord(ZeroAddr);
    Addr += Y;
    PageCrossed = PC >> 8 != Addr >> 8;
    return ReadByte(Addr);
}

Byte ZeroPage (){
    Byte Addr = FetchByte();
    return ReadByte(Addr);
}

Byte ZeroPageX (){
    Byte baseAddr = FetchByte();
    Byte Addr = (baseAddr + X) & 0xFF;
    return ReadByte(Addr);
}

Byte Absolute (){
    Word Addr = FetchWord();
    return ReadByte(Addr);
}

Byte AbsoluteX (){
    Word Addr = FetchWord();
    Addr += X;
    PageCrossed = PC >> 8 != Addr >> 8;
    return ReadByte(Addr);
}

Byte AbsoluteY (){
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
            Memory[0x0100 + SP] = (PC & 0xFF00) >> 2;
            SP--;
            Memory[0x0100 + SP] = PC & 0x00FF;
            SP--;
            Memory[0x0100 + SP] = PF;
            SP--;
            PC = Memory[0xFFFE] + (Memory[0xFFFF] << 8);
            SetBreak();
            cycle(7);
        } break;
        case 0x06: //ASL Zero Page
        {
            Byte Addr = FetchByte();
            ASL(&Memory[Addr]);
            cycle(5);
        } break;
        case 0x0A: //ASL Accumulator
        {
            ASL(&A);
            cycle(2);
        } break;
        case 0x0E: //ASL Absolute
        {
            Word Addr = FetchWord();
            ASL(&Memory[Addr]);
            cycle(6);
        } break;
        case 0x10: //BPL Relative
        {
            Branch(FetchByte(), !CheckNegative());
        } break;
        case 0x16: //ASL Zero Page, X
        {
            Byte baseAddr = FetchByte();
            Byte Addr = (baseAddr + X) & 0xFF;
            ASL(&Memory[Addr]);
            cycle(6);
        } break;
        case 0x18: //CLC Implied
        {
            ClearCarry();
            cycle(2);
        } break;
        case 0x1E: //ASL Absolute, X
        {
            Word Addr = FetchWord();
            Addr += X;
            ASL(&Memory[Addr]);
            cycle(7);
        } break;
        case 0x21: //AND Indirect, X
        {
            AND(IndirectX());
            cycle(6);
        } break;
        case 0x24: //BIT Zero Page
        {
            BIT(ZeroPage());
            cycle(3);
        } break;
        case 0x25: //AND Zero Page
        {
            AND(ZeroPage());
            cycle(3);
        } break;
        case 0x29: //AND Immediate
        {
            AND(FetchByte());
            cycle(2);
        } break;
        case 0x2C: //BIT Absolute
        {
            BIT(Absolute());
            cycle(4);
        } break;
        case 0x2D: //AND Absolute
        {
            AND(Absolute());
            cycle(4);
        } break;
        case 0x30: //BMI Relative
        {
            Branch(FetchByte(), CheckNegative());
        } break;
        case 0x31: //AND Indirect, Y
        {
            AND(IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0x35: //AND Zero Page, X
        {
            AND(ZeroPageX());
            cycle(4);
        } break;
        case 0x39: //AND Absolute, Y
        {
            AND(AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0x3D: //AND Absolute, X
        {
            AND(AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0x41: //EOR Indirect, X
        {
            EOR(IndirectX());
            cycle(6);
        } break;
        case 0x45: //EOR Zero Page
        {
            EOR(ZeroPage());
            cycle(3);
        } break;
        case 0x49: //EOR Immediate
        {
            EOR(FetchByte());
            cycle(2);
        } break;
        case 0x4D: //EOR Absolute
        {
            EOR(Absolute());
            cycle(4);
        }
        case 0x50: //BVC Relative
        {
            Branch(FetchByte(), !CheckOverflow());
        } break;
        case 0x51: //EOR Indirect, Y
        {
            EOR(IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0x55: //EOR Zero Page, X
        {
            EOR(ZeroPageX());
            cycle(4);
        } break;
        case 0x58: //CLI Implied
        {
            ClearInterruptDisable();
            cycle(2);
        } break;
        case 0x59: //EOR Absolute, Y
        {
            EOR(AbsoluteY());
            cycle(4 + PageCrossed);
        }
        case 0x5D: //EOR Absolute, X
        {
            EOR(AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0x61: //ADC Indirect, X
        {
            ADC(IndirectX());
            cycle(6);
        } break;
        case 0x65: //ADC Zero Page
        {
            ADC(ZeroPage());
            cycle(3);
        } break;
        case 0x69: //ADC Immediate
        {
            ADC(FetchByte());
            cycle(2);
        } break;
        case 0x6D: //ADC Absolute
        {
            ADC(Absolute());
            cycle(4);
        } break;
        case 0x70: //BVS Relative
        {
            Branch(FetchByte(), CheckOverflow());
        } break;
        case 0x71: //ADC Indirect, Y
        {
            ADC(IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0x75: //ADC Zero Page, X
        {
            ADC(ZeroPageX());
            cycle(4);
        } break;
        case 0x79: //ADC Absolute, Y
        {
            ADC(AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0x7D: //ADC Absolute, X
        {
            ADC(AbsoluteX());
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
            Branch(FetchByte(), !CheckCarry());
        } break;
        case 0xA1: //LDA Indirect, X
        {
            LDA(IndirectX());
            cycle(6);
        } break;
        case 0xA5: //LDA Zero Page
        {
            LDA(ZeroPage());
            cycle(3);
        } break;
        case 0xA9: //LDA Immediate
        {
            LDA(FetchByte());
            cycle(2);
        } break;
        case 0xAD: //LDA Absolute
        {
            LDA(Absolute());
            cycle(4);
        } break;
        case 0xB0: //BCS Relative
        {
            Branch(FetchByte(), CheckCarry());
        } break;
        case 0xB1: //LDA Indirect, Y
        {
            LDA(IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0xB5: //LDA Zero Page, X
        {
            LDA(ZeroPageX());
            cycle(4);
        } break;
        case 0xB8: //CLV Implied
        {
            ClearOverflow();
            cycle(2);
        } break;
        case 0xB9: //LDA Absolute, Y
        {
            LDA(AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0xBD: //LDA Absolute, X
        {
            LDA(AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0xC0: //CPY Immediate
        {
            CPY(FetchByte());
            cycle(2);
        } break;
        case 0xC1: //CMP Indirect, X
        {
            CMP(IndirectX());
            cycle(6);
        } break;
        case 0xC4: //CPY Zero Page
        {
            CPY(ZeroPage());
            cycle(3);
        } break;
        case 0xC5: //CMP Zero Page
        {
            CMP(ZeroPage());
            cycle(3);
        } break;
        case 0xC6: //DEC Zero Page
        {
            DEC(&Memory[FetchByte()]);
            cycle(6);
        } break;
        case 0xC9: //CMP Immediate
        {
            CMP(FetchByte());
            cycle(2);
        } break;
        case 0xCA: //DEX Implied
        {
            X--;
            if (X == 0){
                SetZero();
            } else {
                ClearZero();
            }
            if (X & 0x80 >> 7){
                SetNegative();
            } else {
                ClearNegative();
            }
            cycle(2);
        } break;
        case 0xCC: //CPY Absolute
        {
            CPY(Absolute());
            cycle(4);
        } break;
        case 0xCD: //CMP Absolute
        {
            CMP(Absolute());
            cycle(4);
        } break;
        case 0xCE: //DEC Absolute
        {
            DEC(&Memory[FetchWord()]);
            cycle(6);
        } break;
        case 0xD0: //BNE Relative
        {
            Branch(FetchByte(), !CheckZero());
        } break;
        case 0xD1: //CMP Indirect, Y
        {
            CMP(IndirectY());
            cycle(5 + PageCrossed);
        } break;
        case 0xD5: //CMP Zero Page, X
        {
            CMP(ZeroPageX());
            cycle(4);
        } break;
        case 0xD6:
        {
            DEC(&Memory[(FetchByte() + X) & 0xFF]);
            cycle(6);
        } break;
        case 0xD8: //CLD Implied
        {
            ClearDecimalMode();
            cycle(2);
        } break;
        case 0xD9: //CMP Absolute, Y
        {
            CMP(AbsoluteY());
            cycle(4 + PageCrossed);
        } break;
        case 0xDD: //CMP Absolute, X
        {
            CMP(AbsoluteX());
            cycle(4 + PageCrossed);
        } break;
        case 0xDE: //DEC Absolute, X
        {
            DEC(&Memory[FetchWord() + X]);
            cycle(7);
        } break;
        case 0xE0: //CPX Immediate
        {
            CPX(FetchByte());
            cycle(2);
        } break;
        case 0xE4: //CPX Zero Page
        {
            CPX(ZeroPage());
            cycle(3);
        } break;
        case 0xEC: //CPX Absolute
        {
            CPX(Absolute());
            cycle(4);
        } break;
        case 0xF0: //BEQ Relative
        {
            Branch(FetchByte(), CheckZero());
        } break;
        default:
            break;
        }
        numCycles++;
    }
}

void printRegisters(){
    printf("A = %X, X = %X, Y = %X, SP = %X, PC = %X, SF = %X\n", A, X, Y, SP, PC, PF);
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