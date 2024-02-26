// Resources used:
// https://www.ic.unicamp.br/~celio/mc404-2006/flags.html
// https://c9x.me/x86/
// http://www.c-jump.com/CIS77/reference/Instructions_by_Opcode.html
// https://www.felixcloutier.com/x86/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cpu8086.h"

/* Status (or flag) register */
StatusRegister FLAGS;

/* General purposes registers */
GeneralPurposeRegister A;
GeneralPurposeRegister B;
GeneralPurposeRegister C;
GeneralPurposeRegister D;

/* Segment registers*/
Register CS;
Register DS;
Register SS;
Register ES;

/* Pointers and Index Registers */
Register IP;
Register BP;
Register SP;
Register SI;
Register DI;

/* Instructions */
#define N_INSTRUCTIONS 0xFF
CpuInstruction cpu_instructions[N_INSTRUCTIONS] = {0};

void op_addb(uint8_t *b1);
void op_pushf(void); //not used - to be removed
void op_pusha(void);
void op_push_cs(void); //not used - to be removed
void op_movww(uint16_t *w1, uint16_t *w2);
void op_movsw(void);
void op_movbb(uint8_t *b1, uint8_t *b2);
void op_lodsb(void);
void op_stosb(void);
void op_stosw(void);
void op_pushw(uint16_t *w1);
void op_popw(uint16_t *w1);
void op_rolwb(uint16_t *w1, uint8_t *b2);
void op_andbb(uint8_t *b1, uint8_t *b2);
void op_daa(void);
void op_adcbb(uint8_t *b1, uint8_t *b2);
void op_decw(uint16_t *w1);
void opvar_jump(uint8_t *b1, char *variant);
void op_jz(uint8_t *b1);
void op_jnz(uint8_t *b1);
void op_loop(uint8_t *b1);
void op_int(uint8_t *b1);
void op_call(uint16_t *w1);
void op_ret(void);
void op_xorbb(uint8_t *b1, uint8_t *b2);
void op_orbb(uint8_t *b1, uint8_t *b2);


uint8_t carryflag8(uint8_t b1, uint8_t b2);
uint8_t parity8(uint8_t b);
void setflag_zsp8(uint8_t b);
uint8_t parity16(uint16_t w);
void setflag_zsp16(uint16_t w);

void print_stack(void);
void print_registry(void);
void print_info(void);


/* Memory of 512Kb */
#define MEMORY_SIZE 0x80000
uint8_t memory[MEMORY_SIZE];
uint16_t stack_start;

/* -------------------------------------------------- */
void* decode_modrm_16(uint8_t value)
{
    // http://www.righto.com/2023/02/8086-modrm-addressing.html
    void *r = NULL;
    ModRM n;
    n.byte = value;
    switch(n.mod) {
        case 0x03:
            // register-register mode
            switch(n.rm) {
                case 0x00: r = (void*)&AX; break;
                default:
                    printf("Invalid R/M in ModR/M byte!\n");
            }
            break;
        default:
            printf("Invalid mode in ModR/M byte!\n");
    }    
    // switch(value) {
    //     case 0xC0: 
    //     default:
    //         printf("Invalid ModR/M byte!\n");
    //         r = NULL;
    // }
    return r;
}

void* decode_modrm_r8(uint8_t value)
{
    void *r = NULL;
    switch(value) {
        case 0x00: r = (void*)&AL; break;
        case 0x07: r = (void*)&BH; break;
        default:
            printf("Invalid registry (8) referenced in ModR/M byte!\n");
    }
    return r;
}

void* decode_modrm_r16(uint8_t value)
{
    void *r = NULL;
    switch(value) {
        case 0x00: r = (void*)&AX; break;
        default:
            printf("Invalid registry (16) referenced in ModR/M byte!\n");
    }
    return r;
}

void* decode_modrm_seg(uint8_t value)
{
    void *r = NULL;
    switch(value) {
        case 0x01: r = (void*)&CS; break;
        case 0x03: r = (void*)&DS; break;
        default:
            printf("Invalid segment referenced in ModR/M byte!\n");
    }
    return r;
}


void *get_modrm_address(enum ArgType argument, ModRM value)
{
    void *r = NULL;
    if (value.mod == 0x03) {
        switch(argument) {
            case ARG_MODRM_RM_R8:
                r = decode_modrm_r8(value.rm);
                break;
            case ARG_MODRM_REG_R8:
                r = decode_modrm_r8(value.reg);
                break;
            case ARG_MODRM_RM_R16:
                r = decode_modrm_r16(value.rm);
                break;
            case ARG_MODRM_RM_SEG:
                r = decode_modrm_seg(value.rm);
                break;
            case ARG_MODRM_REG_R16:
                r = decode_modrm_r16(value.reg);
                break;
            case ARG_MODRM_REG_SEG:
                r = decode_modrm_seg(value.reg);
                break;
            default:
                printf("Argument must be of ModR/M subset!\n");
        }
    } else {
        printf("Invalid mode in ModR/M byte!\n");
    }
    return r;
}

uint8_t next_byte()
{
    uint8_t r;
    IP += 1;
    r = memory[INSTRUCTION_ADDRESS];
    printf("Next byte = 0x%02X at 0x%04X\n", r, INSTRUCTION_ADDRESS);
    return r;           
}

void* get_argument_address(enum ArgType argument)
{
    void *r;
    uint16_t *offset;
    switch(argument) {
        case ARG_FLAGS: return (void*)&FLAGS;
        case ARG_AX: return (void*)&AX;
        case ARG_AH: return (void*)&AH;
        case ARG_AL: return (void*)&AL;
        case ARG_BX: return (void*)&BX;
        case ARG_CX: return (void*)&CX;
        case ARG_DX: return (void*)&DX;
        case ARG_CS: return (void*)&CS;
        case ARG_SI: return (void*)&SI;
        case ARG_DI: return (void*)&DI;
        case ARG_MEMORY_8:
            r = (void *)&memory[INSTRUCTION_ADDRESS+1];
            printf("Memory (8): %p %p %lX 0x%02X\n", memory, r, (uint8_t*) r - memory, *((uint8_t*) r));
            IP += 1;
            break;
        case ARG_MEMORY_16:
            offset = (uint16_t*)&memory[INSTRUCTION_ADDRESS+1]; 
            // r =  (void *)((uint16_t*)memory + 16*ES + *offset);
            r = (void *)offset;
            printf("Memory (16): %p %p %lX %04X %04X 0x%04X\n", memory, r, (uint8_t*) r - memory, *offset, *((uint16_t*) r), (16*ES));
            IP += 2;
            break;
        case ARG_MODRM_16:
            r = decode_modrm_16(memory[INSTRUCTION_ADDRESS+1]);
            printf("ModRM (16): %p %p %lX 0x%02X (AX at %p)\n", memory, r, (uint8_t*) r - memory, *((uint8_t*) r), &AX);
            IP += 1;
            break;      
        case ARG_INVALID:
        default:
            printf("Invalid argument type!\n");
            return NULL;
    }
    return r;
}

/* Initialization */
void cpu_init()
{
    /* Registers */
    AX = 0x0000;
    BX = 0x0000;
    CX = 0x00FF;
    DX = 0x01ED;

    IP = 0x0100;
    BP = 0x091C;
    SP = 0x0FFF;
    SI = 0x0100;
    DI = 0xFFFE;

    CS = 0x01ED;
    DS = 0x01ED;
    SS = 0x01ED;
    ES = 0x01ED;

    FLAGS.IF = 1;

    /* Instructions */
    for (uint8_t i=0; i < N_INSTRUCTIONS; i++) {
        cpu_instructions[i].type = FCN_INVALID;
    }

    cpu_instructions[0x04].type = FCN_BYTE;
    cpu_instructions[0x04].fcn.byte = &op_addb;
    cpu_instructions[0x04].op1 = ARG_AL;
    // 9C pushf none
    // cpu_instructions[0x9C].type = FCN_NOARG;
    // cpu_instructions[0x9C].fcn.noarg = &op_pushf;
    cpu_instructions[0x9C].type = FCN_WORD;
    cpu_instructions[0x9C].fcn.word = &op_pushw;
    cpu_instructions[0x9C].op1 = ARG_FLAGS;

    cpu_instructions[0x60].type = FCN_NOARG;
    cpu_instructions[0x60].fcn.noarg = &op_pusha;
    
    // 0E push CS
    // cpu_instructions[0x0E].type = FCN_NOARG;
    // cpu_instructions[0x0E].fcn.noarg = &op_push_cs;
    cpu_instructions[0x0E].type = FCN_WORD;
    cpu_instructions[0x0E].fcn.word = &op_pushw;
    cpu_instructions[0x0E].op1 = ARG_CS;  
    // BF mov DI, imm16
    cpu_instructions[0xBF].type = FCN_WORDWORD;
    cpu_instructions[0xBF].fcn.wordword = &op_movww;
    cpu_instructions[0xBF].op1 = ARG_DI;
    cpu_instructions[0xBF].op2 = ARG_MEMORY_16;
    // BE mov SI, imm16
    cpu_instructions[0xBE].type = FCN_WORDWORD;
    cpu_instructions[0xBE].fcn.wordword = &op_movww;
    cpu_instructions[0xBE].op1 = ARG_SI;
    cpu_instructions[0xBE].op2 = ARG_MEMORY_16;
    // B8 mov AX, imm16
    cpu_instructions[0xB8].type = FCN_WORDWORD;
    cpu_instructions[0xB8].fcn.wordword = &op_movww;
    cpu_instructions[0xB8].op1 = ARG_AX;
    cpu_instructions[0xB8].op2 = ARG_MEMORY_16;
    // BA mov DX, imm16
    cpu_instructions[0xBA].type = FCN_WORDWORD;
    cpu_instructions[0xBA].fcn.wordword = &op_movww;
    cpu_instructions[0xBA].op1 = ARG_DX;
    cpu_instructions[0xBA].op2 = ARG_MEMORY_16;
    // BB mov BX, imm16
    cpu_instructions[0xBB].type = FCN_WORDWORD;
    cpu_instructions[0xBB].fcn.wordword = &op_movww;
    cpu_instructions[0xBB].op1 = ARG_BX;
    cpu_instructions[0xBB].op2 = ARG_MEMORY_16;
    // B9 mov CX, imm16
    cpu_instructions[0xB9].type = FCN_WORDWORD;
    cpu_instructions[0xB9].fcn.wordword = &op_movww;
    cpu_instructions[0xB9].op1 = ARG_CX;
    cpu_instructions[0xB9].op2 = ARG_MEMORY_16;
    // A5 movsw none
    // Move word at address DS:(E)SI to address ES:(E)DI
    cpu_instructions[0xA5].type = FCN_NOARG;
    cpu_instructions[0xA5].fcn.noarg = &op_movsw;
    // B0 mov AL, imm8
    cpu_instructions[0xB0].type = FCN_BYTEBYTE;
    cpu_instructions[0xB0].fcn.bytebyte = &op_movbb;
    cpu_instructions[0xB0].op1 = ARG_AL;
    cpu_instructions[0xB0].op2 = ARG_MEMORY_8;
    // B4 mov AH, imm8
    cpu_instructions[0xB4].type = FCN_BYTEBYTE;
    cpu_instructions[0xB4].fcn.bytebyte = &op_movbb;
    cpu_instructions[0xB4].op1 = ARG_AH;
    cpu_instructions[0xB4].op2 = ARG_MEMORY_8;
    // AC lodsb none
    cpu_instructions[0xAC].type = FCN_NOARG;
    cpu_instructions[0xAC].fcn.noarg = &op_lodsb;
    // AA stosb none
    // Store AL at address ES:(E)DI.
    cpu_instructions[0xAA].type = FCN_NOARG;
    cpu_instructions[0xAA].fcn.noarg = &op_stosb;
    // AB stosw none
    cpu_instructions[0xAB].type = FCN_NOARG;
    cpu_instructions[0xAB].fcn.noarg = &op_stosw;
    // 58 pop AX
    cpu_instructions[0x58].type = FCN_WORD;
    cpu_instructions[0x58].fcn.word = &op_popw;
    cpu_instructions[0x58].op1 = ARG_AX;
    // C1 rol mem16,imm8
    cpu_instructions[0xC1].type = FCN_WORDBYTE;
    cpu_instructions[0xC1].fcn.wordbyte = &op_rolwb;
    cpu_instructions[0xC1].op1 = ARG_MODRM_16;
    cpu_instructions[0xC1].op2 = ARG_MEMORY_8;
    // 50 push AX 
    cpu_instructions[0x50].type = FCN_WORD;
    cpu_instructions[0x50].fcn.word = &op_pushw;
    cpu_instructions[0x50].op1 = ARG_AX;
    // 24 and AL,imm8
    cpu_instructions[0x24].type = FCN_BYTEBYTE;
    cpu_instructions[0x24].fcn.bytebyte = &op_andbb;
    cpu_instructions[0x24].op1 = ARG_AL;
    cpu_instructions[0x24].op2 = ARG_MEMORY_8;
    // 27 daa none
    cpu_instructions[0x27].type = FCN_NOARG;
    cpu_instructions[0x27].fcn.noarg = &op_daa;
    // 14 adc AL,imm8
    cpu_instructions[0x14].type = FCN_BYTEBYTE;
    cpu_instructions[0x14].fcn.bytebyte = &op_adcbb;
    cpu_instructions[0x14].op1 = ARG_AL;
    cpu_instructions[0x14].op2 = ARG_MEMORY_8;
    // 4B dec BX
    cpu_instructions[0x4B].type = FCN_WORD;
    cpu_instructions[0x4B].fcn.word = &op_decw;
    cpu_instructions[0x4B].op1 = ARG_BX;
    // 74 je (jz) rel8
    cpu_instructions[0x74].type = FCN_BYTE;
    cpu_instructions[0x74].fcn.byte = &op_jz;
    cpu_instructions[0x74].op1 = ARG_MEMORY_8;
    // 75 jne (jnz) rel8
    cpu_instructions[0x75].type = FCN_BYTE;
    cpu_instructions[0x75].fcn.byte = &op_jnz;
    cpu_instructions[0x75].op1 = ARG_MEMORY_8;
    // E2 loop rel8
    cpu_instructions[0xE2].type = FCN_BYTE;
    cpu_instructions[0xE2].fcn.byte = &op_loop;
    cpu_instructions[0xE2].op1 = ARG_MEMORY_8;
    // INT imm8
    cpu_instructions[0xCD].type = FCN_BYTE;
    cpu_instructions[0xCD].fcn.byte = &op_int;
    cpu_instructions[0xCD].op1 = ARG_MEMORY_8;
    // 8C mov reg16, sreg
    cpu_instructions[0x8C].type = FCN_MODRM_WORDWORD;
    cpu_instructions[0x8C].fcn.wordword = &op_movww;
    cpu_instructions[0x8C].op1 = ARG_MODRM_RM_R16;
    cpu_instructions[0x8C].op2 = ARG_MODRM_REG_SEG;
    // 8E mov sreg, reg16
    cpu_instructions[0x8E].type = FCN_MODRM_WORDWORD;
    cpu_instructions[0x8E].fcn.wordword = &op_movww;
    cpu_instructions[0x8E].op1 = ARG_MODRM_REG_SEG;
    cpu_instructions[0x8E].op2 = ARG_MODRM_RM_R16;
    // E8 call rel16
    cpu_instructions[0xE8].type = FCN_WORD;
    cpu_instructions[0xE8].fcn.word = &op_call;
    cpu_instructions[0xE8].op1 = ARG_MEMORY_16;
    // C3 ret (near) none
    cpu_instructions[0xC3].type = FCN_NOARG;
    cpu_instructions[0xC3].fcn.noarg = &op_ret;
    // 30 xor mem8,reg8
    cpu_instructions[0x30].type = FCN_MODRM_BYTEBYTE;
    cpu_instructions[0x30].fcn.bytebyte = &op_xorbb;
    cpu_instructions[0x30].op1 = ARG_MODRM_RM_R8;
    cpu_instructions[0x30].op2 = ARG_MODRM_REG_R8;
    // 08 xo mem8,reg8
    cpu_instructions[0x08].type = FCN_MODRM_BYTEBYTE;
    cpu_instructions[0x08].fcn.bytebyte = &op_orbb;
    cpu_instructions[0x08].op1 = ARG_MODRM_RM_R8;
    cpu_instructions[0x08].op2 = ARG_MODRM_REG_R8;
    // EB jmp rel8
    cpu_instructions[0xEB].type = FCN_BYTE_VARIANT;
    cpu_instructions[0xEB].fcn.byte_variant = &opvar_jump;
    strcpy(cpu_instructions[0xEB].variant, "jmp");
    cpu_instructions[0xEB].op1 = ARG_MEMORY_8;

    /* Memory */
    stack_start = STACK_ADDRESS;
    //printf("STACK starts at 0x%04X\n", stack_start);
    print_info();
}

/* Loading binary program from a file - e.g. from a COM file */
void load_program(char *fn, uint16_t offset)
{
    FILE *fp;
    uint32_t length;
    printf("Loading program from %s\n", fn);
    fp = fopen(fn, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fread (memory+offset, 1, length, fp) != length) {
            printf("Error reading!\n");
        }
        fclose (fp);
    } else {
        printf("Cannot open file!\n");
    }
}

void cpu_run()
{
    uint64_t ticks = 0;
    uint8_t opcode;
    uint8_t *op1_8, *op2_8;
    uint16_t *op1_16, *op2_16;
    ModRM tmp_modrm;
    bool running = true;
    while(running) {
        opcode = memory[INSTRUCTION_ADDRESS];
        printf("#%ld address: 0x%04X, opcode: 0x%02X\n", ticks, INSTRUCTION_ADDRESS, opcode);
        if (cpu_instructions[opcode].type != FCN_INVALID) {
            switch(cpu_instructions[opcode].type) {
                case FCN_NOARG:
                    (*cpu_instructions[opcode].fcn.noarg)();
                    break;
                case FCN_BYTE:
                    op1_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op1);
                    (*cpu_instructions[opcode].fcn.byte)(op1_8);
                    break;
                case FCN_BYTE_VARIANT:
                    op1_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op1);
                    (*cpu_instructions[opcode].fcn.byte_variant)(op1_8, cpu_instructions[opcode].variant);
                    break;
                case FCN_BYTEBYTE:
                    op1_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op1);
                    op2_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op2);
                    (*cpu_instructions[opcode].fcn.bytebyte)(op1_8, op2_8);
                    break;
                case FCN_WORD:
                    op1_16 = (uint16_t*) get_argument_address(cpu_instructions[opcode].op1);
                    (*cpu_instructions[opcode].fcn.word)(op1_16);
                    break;    
                case FCN_WORDWORD:
                    op1_16 = (uint16_t*) get_argument_address(cpu_instructions[opcode].op1);
                    op2_16 = (uint16_t*) get_argument_address(cpu_instructions[opcode].op2);
                    (*cpu_instructions[opcode].fcn.wordword)(op1_16, op2_16);
                    break;
                case FCN_WORDBYTE:
                    op1_16 = (uint16_t*) get_argument_address(cpu_instructions[opcode].op1);
                    op2_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op2);
                    (*cpu_instructions[opcode].fcn.wordbyte)(op1_16, op2_8);
                    break;
                case FCN_MODRM_BYTEBYTE:
                    tmp_modrm.byte = next_byte();
                    op1_8 = (uint8_t*) get_modrm_address(cpu_instructions[opcode].op1, tmp_modrm);
                    op2_8 = (uint8_t*) get_modrm_address(cpu_instructions[opcode].op2, tmp_modrm);
                    (*cpu_instructions[opcode].fcn.bytebyte)(op1_8, op2_8);
                    break;    
                case FCN_MODRM_WORDWORD:
                    tmp_modrm.byte = next_byte();
                    op1_16 = (uint16_t*) get_modrm_address(cpu_instructions[opcode].op1, tmp_modrm);
                    op2_16 = (uint16_t*) get_modrm_address(cpu_instructions[opcode].op2, tmp_modrm);
                    (*cpu_instructions[opcode].fcn.wordword)(op1_16, op2_16);
                    break;
                default:
                    printf("Unknown function type %d\n", cpu_instructions[opcode].type);
            }
        } else {
            printf("Not implemented instruction 0x%02X\n", opcode);
            exit(1);
        }
        print_info();
        // Step in
        IP += 1;
        ticks += 1;
        if (ticks > 5500) running = false;
    }
}

/* Instructions */
void op_addb(uint8_t *b1)
{
    printf("op_addb %p (AL at %p = 0x%02X), 0x%02X + 0x%02X\n", b1, &AL, AL, *b1, memory[INSTRUCTION_ADDRESS+1]);
    IP += 1;
    
    // if(*b1 == 0) FLAGS.ZF = 1; else FLAGS.ZF = 0;
    // if(*b1 < 0x80) FLAGS.SF = 0; else FLAGS.SF = 1;

    uint16_t r16;
    uint8_t *b2 = &memory[INSTRUCTION_ADDRESS];
    r16 = (uint16_t)*b1 + (uint16_t)*b2;
    *b1 += memory[INSTRUCTION_ADDRESS];
    setflag_zsp8(*b1);
    // OF
	if (((r16 ^ *b1) & (r16 ^ *b2) & 0x80) == 0x80) FLAGS.OF = 1;
    else FLAGS.OF = 0;
    // CF
	if (r16 & 0xFF00) FLAGS.CF = 1;
    else FLAGS.CF = 0;
	// AF		
	if (((*b1 ^ *b2 ^ r16) & 0x10) == 0x10) FLAGS.AF = 1;
    else FLAGS.AF = 0;
}

void op_pushf(void)
{
    // Not used
    printf("op_pushf, stack address: 0x%04X\n", STACK_ADDRESS);
    SP -= 2;
    memory[STACK_ADDRESS+2] = FLAGS.H;
    memory[STACK_ADDRESS+1] = FLAGS.L;
    print_stack();
}

void op_pusha(void)
{
    printf("op_pusha, stack address: 0x%04X\n", STACK_ADDRESS);
    Register orig_SP = SP;
    
    op_pushw(&AX);
    op_pushw(&CX);
    op_pushw(&DX);
    op_pushw(&BX);
    op_pushw(&orig_SP);
    op_pushw(&BP);
    op_pushw(&SI);
    op_pushw(&DI);

    // SP -= 2;
    // memory[STACK_ADDRESS+2] = AH;
    // memory[STACK_ADDRESS+1] = AL;

    // SP -= 2;
    // memcpy(memory+STACK_ADDRESS+1, &CX, 2);
    // SP -= 2;
    // memcpy(memory+STACK_ADDRESS+1, &DX, 2);
    // SP -= 2;
    // memcpy(memory+STACK_ADDRESS+1, &BX, 2);
    // SP -= 2;
    // memcpy(memory+STACK_ADDRESS+1, &orig_SP, 2);
    // SP -= 2;
    // memcpy(memory+STACK_ADDRESS+1, &BP, 2);
    // SP -= 2;
    // memcpy(memory+STACK_ADDRESS+1, &SI, 2);
    // SP -= 2;
    // memcpy(memory+STACK_ADDRESS+1, &DI, 2);
    // print_stack();
}

void op_push_cs(void)
{
    // Not used
    printf("op_push CS, stack address: 0x%04X\n", STACK_ADDRESS);
    SP -= 2;
    memcpy(memory+STACK_ADDRESS+1, &CS, 2);
    //printf("0x%04X, 0x%02X 0x%02X\n", CS, memory[STACK_ADDRESS+1], memory[STACK_ADDRESS+2]);
    print_stack();
}

void op_movww(uint16_t *w1, uint16_t *w2)
{
    printf("op_movww %p %p; %p %X\n", w1, &DI, w2, *w2);
    *w1 = *w2;
    // printf("DI (0x%04X)\n",DI);
    // printf("SI (0x%04X)\n",SI);
}

void op_movsw(void)
{
    // A5 movsw none
    // Move word at address DS:(E)SI to address ES:(E)DI
    printf("op_movsw %02X %02X <-- %02X %02X\n", memory[16*ES+DI], memory[16*ES+DI+1], memory[16*DS+SI], memory[16*DS+SI+1]);
    // printf("DI (%04X) --> SI (0x%04X)\n", DI, SI);
    memory[16*ES+DI] = memory[16*DS+SI];
    memory[16*ES+DI+1] = memory[16*DS+SI+1];
}

void op_movbb(uint8_t *b1, uint8_t *b2)
{
    printf("op_movbb %p %p; %p %X <-- %X\n", b1, &AL, b1, *b1, *b2);
    *b1 = *b2;
}

void op_lodsb(void)
{
    // AC lodsb none
    // Load byte at address DS:(E)SI into AL.
    printf("op_lodsb %02X (16*DS+SI) <-- %02X '%c' (AL)\n", memory[16*DS+SI], AL, AL);
    AL = memory[16*DS+SI];
    if(FLAGS.DF == 0) SI += 1;
	else SI -= 1;
}

void op_stosb(void)
{
    // AA stosb none
    // Store AL at address ES:(E)DI.
    printf("op_stosb %02X (16*ES+DI) <-- %02X '%c' (AL)\n", memory[16*ES+DI], AL, AL);
    memory[16*ES+DI] = AL;
    DI += 1;
}

void op_stosw(void)
{
    // AB stosw none
    // Store AX at address ES:(E)DI.
    printf("op_stosw %02X (16*ES+DI) <-- %02X (AX)\n", memory[16*ES+DI], AX);
    memcpy(memory+16*ES+DI, &AX, 2);
    DI += 2;
}


void op_pushw(uint16_t *w1)
{
    printf("op_pushw %p, stack address: 0x%04X\n", w1, STACK_ADDRESS);
    printf("Pushing %04X (%p)\n", *w1, w1);
    SP -= 2;
    memcpy(memory+STACK_ADDRESS, w1,  2);
    //printf("0x%04X (AX=0x%04X), 0x%02X 0x%02X\n", *w1, AX, memory[STACK_ADDRESS+1], memory[STACK_ADDRESS+2]);
    //print_stack();
}

void op_popw(uint16_t *w1)
{
    printf("op_popw %p, stack address: 0x%04X\n", w1, STACK_ADDRESS);
    memcpy(w1, memory+STACK_ADDRESS, 2);
    SP += 2;
    //printf("0x%04X (AX=0x%04X), 0x%02X 0x%02X\n", *w1, AX, memory[STACK_ADDRESS-1], memory[STACK_ADDRESS-2]);
    printf("Poping: 0x%04X\n", *w1);
    //print_stack();
}

void op_rolwb(uint16_t *w1, uint8_t *b2)
{
    printf("op_rolwb %p (AX at %p); %X << %X\n", w1, &AX, *w1, *b2);
    //*w1 = *w1 << *b2;
    for (uint8_t i = 0; i < *b2; i++) {
        if (*w1 & 0x8000) FLAGS.CF = 1;
        else FLAGS.CF = 0;
        *w1 = *w1 << 1;
        *w1 = *w1 | FLAGS.CF;
    }
    if (*b2 == 1) {
        if((*w1 & 0x8000) && FLAGS.CF) FLAGS.OF = 1;
        else FLAGS.OF = 0;
    } else {
        FLAGS.OF = 0;
    }
    printf("Result at %p = 0x%04X\n", w1, *w1);
}

void op_andbb(uint8_t *b1, uint8_t *b2)
{
    printf("op_andbb %p %p; (AL at %p) 0x%02X & 0x%02X\n", b1, b2, &AL, *b1, *b2);
    *b1 = *b1 & *b2;
    FLAGS.CF = 0;
    FLAGS.OF = 0;
    setflag_zsp8(*b1);
}

uint8_t carryflag8(uint8_t b1, uint8_t b2)
{
    uint16_t result;
    uint8_t carry = 0;
    result = (uint16_t)b1 + (uint16_t)b2;
    if (result & 0xFF00) carry = 1;
    return carry;
}

void op_daa(void)
{
    printf("op_daa\n");
    uint8_t old_AL = AL;
    uint8_t old_CF = FLAGS.CF;
    FLAGS.CF = 0;
    if ((AL & 0xF) > 9 || FLAGS.AF == 1) {
        FLAGS.CF = old_CF | carryflag8(AL, 6);
        AL += 6;
        FLAGS.AF = 1;
    } else {
        FLAGS.AF = 0;
    }
    if (old_AL > 0x99 || old_CF == 1) {
        AL += 0x60;
        FLAGS.CF = 1;
    } else {
        FLAGS.CF = 0;
    }
}

void op_adcbb(uint8_t *b1, uint8_t *b2)
{
    printf("op_adcbb %p %p; (AL at %p) 0x%02X + 0x%02X + %d\n", b1, b2, &AL, *b1, *b2, FLAGS.CF);
    uint16_t r16;
    r16 = (uint16_t)*b1 + (uint16_t)*b2 + (uint16_t)FLAGS.CF;
    *b1 = *b1 + *b2 + FLAGS.CF;
    setflag_zsp8(*b1);
    // OF
	if (((r16 ^ *b1) & (r16 ^ *b2) & 0x80) == 0x80) FLAGS.OF = 1;
    else FLAGS.OF = 0;
    // CF
	if (r16 & 0xFF00) FLAGS.CF = 1;
    else FLAGS.CF = 0;
	// AF		
	if (((*b1 ^ *b2 ^ r16) & 0x10) == 0x10) FLAGS.AF = 1;
    else FLAGS.AF = 0;
}  

uint8_t parity8(uint8_t b) 
{
    b ^= (b >> 8);
    b ^= (b >> 4);
    b ^= (b >> 2);
    b ^= (b >> 1);
    return (b & 1);
}

void setflag_zsp8(uint8_t b) 
{
	// ZF
    if (!b) FLAGS.ZF = 1;
    else FLAGS.ZF = 0;
	// SF
	if (b & 0x80) FLAGS.SF = 1;
	else FLAGS.SF = 0;
    // PF
	FLAGS.PF = parity8(b); 
}

void op_decw(uint16_t *w1)
{
    printf("op_decw %p 0x%04X\n", w1, *w1);
    uint16_t r32;
    uint16_t d = 0x01;
    r32 = (uint32_t)*w1 - (uint32_t)d;
    *w1 -= 1;
    setflag_zsp16(*w1);
    // OF
	if ((r32 ^ *w1) & (*w1 ^ d) & 0x8000) FLAGS.OF = 1;
    else FLAGS.OF = 0;
    // CF
	if (r32 & 0xFFFF0000) FLAGS.CF = 1;
    else FLAGS.CF = 0;
	// AF		
	if ((*w1 ^ d ^ r32) & 0x10) FLAGS.AF = 1;
    else FLAGS.AF = 0;
}

uint8_t parity16(uint16_t w) 
{
    w ^= (w >> 16);
    w ^= (w >> 8);
    w ^= (w >> 4);
    w ^= (w >> 2);
    w ^= (w >> 1);
    return (w & 1);
}

void setflag_zsp16(uint16_t w) 
{
	// ZF
    if (!w) FLAGS.ZF = 1;
    else FLAGS.ZF = 0;
	// SF
	if (w & 0x8000) FLAGS.SF = 1;
	else FLAGS.SF = 0;
    // PF
	FLAGS.PF = parity16(w); 
}

void opvar_jump(uint8_t *b1, char *variant)
{
    bool b_jumped = false;
    int16_t sb1 = (int16_t)((int8_t)*b1);
    printf("opvar_jump (%s) %p 0x%02X %d\n", variant, b1, *b1, sb1);
    if (strcmp(variant, "jmp") == 0) {
        // EB jmp rel8
        IP = IP + sb1;
        b_jumped = true;
    } else {
        printf("Unknown variant: %s", variant);
        exit(1);
    }
    if (b_jumped) printf("Jumped to 0x%04X\n", IP);
}

void op_jz(uint8_t *b1)
{
    /* JNE and JNZ are just different names for a conditional jump when ZF is equal to 0. */
    int16_t sb1 = (int16_t)((int8_t)*b1);
    printf("op_jz %p 0x%02X %d\n", b1, *b1, sb1);
    if(FLAGS.ZF == 1) {
        IP = IP + sb1;
        printf("Jumped to 0x%04X\n", IP);
    }
}

void op_jnz(uint8_t *b1)
{
    /* JNE and JNZ are just different names for a conditional jump when ZF is equal to 0. */
    int16_t sb1 = (int16_t)((int8_t)*b1);
    printf("op_jnz %p 0x%02X %d\n", b1, *b1, sb1);
    if(FLAGS.ZF == 0) {
        IP = IP + sb1;
        printf("Jumped to 0x%04X\n", IP);
    }
}

void op_loop(uint8_t *b1)
{
    int16_t sb1 = (int16_t)((int8_t)*b1);
    printf("op_loop %p 0x%02X %d, CX=%d\n", b1, *b1, sb1, CX);
    CX = CX - 1;
    if(CX) {
        IP = IP + sb1;
        printf("Jumped to 0x%04X\n", IP);
    }
}

void op_int(uint8_t *b1)
{
    uint16_t i = 0;
    printf("op_int %p 0x%02X\n", b1, *b1);
    // mov ah, function number
    //    ; input parameters
    //    int 21h
    // Resources used:
    // https://www.philadelphia.edu.jo/academics/qhamarsheh/uploads/Lecture%2021%20MS-DOS%20Function%20Calls%20_INT%2021h_.pdf
    // http://bbc.nvg.org/doc/Master%20512%20Technical%20Guide/m512techb_int21.htm
    if (*b1 == 0x21) {
        printf("Interrupt 0x%02X: MS-DOS Function Call %02X\n", *b1, AH);
        switch(AH) {
            case 0x09:
                // 09h - Write string (terminated by a $ character)to standard output.
                // DS:DX = segment:offset of string
                i = 0;
                printf("PRINTING (0x%04X %c):\n", 16*DS + 0*DX, memory[16*DS+DX+i]);
                // $ = 0x24 = 36
                while (memory[16*DS+DX+i] != '$') {
                    printf("%c", memory[16*DS+DX+i]);
                    i++;
                }
                printf("\n");
                break;
            case 0x4C:
                printf("EXITING 0x%02X!\n", AL);
                exit(AL);
                break;
            default:
                printf("Not implemented interrupt routine AH = 0x%02X\n", AH);
                exit(1);   
        }
    } else if (*b1 == 0x10) {
        printf("Interrupt 0x%02X: MS-DOS Function Call %02X\n", *b1, AH);
        switch(AH) {
            case 0x0E:
                printf("PRINTING: %c\n", AL);
                break;
            default:
                printf("Not implemented interrupt routine AH = 0x%02X\n", AH);
                exit(1);
        }

    } else {
        printf("Not implemented interrupt call 0x%02X\n", *b1);
        exit(1);
    }
    
}	

void op_call(uint16_t *w1)
{
    int16_t sw1 = (int16_t)(*w1);
    printf("op_call %p 0x%04X %d\n", w1, *w1, sw1);
    op_pushw(&IP);
    IP = IP + sw1;
    printf("Jumped to 0x%04X\n", IP);
}

void op_ret(void)
{
    uint16_t sw1;
    op_popw(&sw1);
    printf("op_ret 0x%04X\n", sw1);
    if (sw1 == 0x0000) {
        printf("Seems like nothing is on stack (0x%04X)! Nowhere to return = exit program!\n", sw1);
        exit(0);
    }
    IP = sw1;
    printf("Returned to 0x%04X\n", IP);
}

void op_xorbb(uint8_t *b1, uint8_t *b2)
{
    printf("op_xorbb %p %p; (BH at %p) 0x%02X & 0x%02X\n", b1, b2, &BH, *b1, *b2);
    *b1 = *b1 ^ *b2;
    FLAGS.CF = 0;
    FLAGS.OF = 0;
    setflag_zsp8(*b1);
}

void op_orbb(uint8_t *b1, uint8_t *b2)
{
    printf("op_orbb %p %p; 0x%02X & 0x%02X\n", b1, b2, *b1, *b2);
    *b1 = *b1 | *b2;
    FLAGS.CF = 0;
    FLAGS.OF = 0;
    setflag_zsp8(*b1);
}

/* Printing */
void print_stack(void)
{
    uint16_t i = stack_start - 1;
    while (i >= STACK_ADDRESS) {
        if ((stack_start -1 - i) > 0 && (stack_start -1 - i) % 8 == 0) printf("\n");
        printf("%02X ", memory[i]);
        i--;
    }
    printf("\n");
}

void print_registry(void)
{
    printf("AX %04X  BX %04X  CX %04X  DX %04x\n", AX, BX, CX, DX);
    printf("SP %04X  BP %04X  SI %04X  DI %04X\n", SP, BP, SI, DI);
    printf("ES %04X  CS %04X  SS %04X  DS %04X\n", ES, CS, SS, DS);
    printf("FLAGS %04X (CF=%d,OF=%d,AF=%d,ZF=%d,SF=%d)\n", FLAGS.X, FLAGS.CF, FLAGS.OF, FLAGS.AF, FLAGS.ZF, FLAGS.SF);
    printf("INSTRUCTION AT: %04X\n", INSTRUCTION_ADDRESS);
    printf("STACK AT:       %04X (%04X)\n", STACK_ADDRESS, stack_start);
}

void print_info(void)
{
    printf("--- REGISTRY & STACK ---\n");
    print_registry();
    print_stack();
    printf("------------------------\n");
}

int main(int argc, char **argv)
{
    GeneralPurposeRegister T;
    T.X = 0x1234;
    printf("Test of general purpose register\n");
    printf("X=0x%04X H=0x%02X L=0x%02X\n", T.X, T.H, T.L);

    //
    cpu_init();
    // load_program("yourhelp.com", 0x1FD0);
    load_program("helloworld.com", 0x1FD0);
    // 0x100+16*0x01ED = 0x100 + 0x1ED0

    // memory[0x1FD1] = 0x04;
    cpu_run();
}