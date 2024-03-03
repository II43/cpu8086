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
#include <syslog.h>

#include "cpu8086.h"
#include "cpu8086_instructions.h"

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

/* Memory of 512Kb */
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
                    syslog(LOG_ERR,"Invalid R/M in ModR/M byte!\n");
            }
            break;
        default:
            syslog(LOG_ERR,"Invalid mode in ModR/M byte!\n");
    }    
    // switch(value) {
    //     case 0xC0: 
    //     default:
    //         printf("Invalid ModR/M byte!\n");
    //         r = NULL;
    // }
    return r;
}

enum ArgType decode_argument_r8(uint8_t value)
{
    enum ArgType r = ARG_INVALID;
    switch(value) {
        case 0x00: r = ARG_AL; break;
        case 0x01: r = ARG_CL; break;
        case 0x02: r = ARG_DL; break;
        case 0x03: r = ARG_BL; break;
        case 0x04: r = ARG_AH; break;
        case 0x05: r = ARG_CH; break;
        case 0x06: r = ARG_DH; break;
        case 0x07: r = ARG_BH; break;
        default:
            syslog(LOG_ERR,"Invalid argument (8) referenced - 0x%02X!\n", value);
            clean_up_and_exit(EXIT_FAILURE);
    }
    return r;
}

void* decode_modrm_r8(uint8_t value)
{
    void *r = NULL;
    switch(value) {
        case 0x00: r = (void*)&AL; break;
        case 0x01: r = (void*)&CL; break;
        case 0x02: r = (void*)&DL; break;
        case 0x03: r = (void*)&BL; break;
        case 0x04: r = (void*)&AH; break;
        case 0x05: r = (void*)&CH; break;
        case 0x06: r = (void*)&DH; break;
        case 0x07: r = (void*)&BH; break;
        default:
            syslog(LOG_ERR,"Invalid registry (8) referenced - 0x%02X!\n", value);
            clean_up_and_exit(EXIT_FAILURE);
    }
    return r;
}

enum ArgType decode_argument_r16(uint8_t value)
{
    enum ArgType r = ARG_INVALID;
    switch(value) {
        case 0x00: r = ARG_AX; break;
        case 0x01: r = ARG_CX; break;
        case 0x02: r = ARG_DX; break;
        case 0x03: r = ARG_BX; break;
        case 0x04: r = ARG_SP; break;
        case 0x05: r = ARG_BP; break;
        case 0x06: r = ARG_SI; break;
        case 0x07: r = ARG_DI; break;
        default:
            syslog(LOG_ERR,"Invalid argument (16) referenced - 0x%02X!\n", value);
            clean_up_and_exit(EXIT_FAILURE);
    }
    return r;
}

void* decode_modrm_r16(uint8_t value)
{
    void *r = NULL;
    switch(value) {
        case 0x00: r = (void*)&AX; break;
        case 0x01: r = (void*)&CX; break;
        case 0x02: r = (void*)&DX; break;
        case 0x03: r = (void*)&BX; break;
        case 0x04: r = (void*)&SP; break;
        case 0x05: r = (void*)&BP; break;
        case 0x06: r = (void*)&SI; break;
        case 0x07: r = (void*)&DI; break;
        default:
            syslog(LOG_ERR,"Invalid registry (16) referenced in ModR/M byte!\n");
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
            syslog(LOG_DEBUG,"Invalid segment referenced in ModR/M byte!\n");
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
                syslog(LOG_ERR,"Argument must be of ModR/M subset!\n");
        }
    } else {
        syslog(LOG_ERR,"Invalid mode in ModR/M byte!\n");
    }
    return r;
}

uint8_t next_byte()
{
    uint8_t r;
    IP += 1;
    r = memory[INSTRUCTION_ADDRESS];
    syslog(LOG_DEBUG,"Next byte = 0x%02X at 0x%04X\n", r, INSTRUCTION_ADDRESS);
    return r;           
}

void* get_argument_address(enum ArgType argument)
{
    void *r = NULL;
    uint16_t *offset;
    switch(argument) {
        case ARG_FLAGS: return (void*)&FLAGS;
        case ARG_AX: return (void*)&AX;
        case ARG_AH: return (void*)&AH;
        case ARG_AL: return (void*)&AL;
        case ARG_BX: return (void*)&BX;
        case ARG_BH: return (void*)&BH;
        case ARG_BL: return (void*)&BL;
        case ARG_CX: return (void*)&CX;
        case ARG_CH: return (void*)&CH;
        case ARG_CL: return (void*)&CL;    
        case ARG_DX: return (void*)&DX;
        case ARG_DH: return (void*)&DH;
        case ARG_DL: return (void*)&DL;
        case ARG_CS: return (void*)&CS;
        case ARG_SI: return (void*)&SI;
        case ARG_DI: return (void*)&DI;
        case ARG_BP: return (void*)&BP;
        case ARG_MEMORY_8:
            r = (void *)&memory[INSTRUCTION_ADDRESS+1];
            syslog(LOG_DEBUG,"Memory (8): %p %p %lX 0x%02X\n", memory, r, (uint8_t*) r - memory, *((uint8_t*) r));
            IP += 1;
            break;
        case ARG_MODRM_8:
            r = decode_modrm_r8(memory[INSTRUCTION_ADDRESS+1]);
            syslog(LOG_DEBUG,"ModRM (8): %p %p %lX 0x%02X\n", memory, r, (uint8_t*) r - memory, *((uint8_t*) r));
            IP += 1;
            break;
        case ARG_MEMORY_16:
            offset = (uint16_t*)&memory[INSTRUCTION_ADDRESS+1]; 
            // r =  (void *)((uint16_t*)memory + 16*ES + *offset);
            r = (void *)offset;
            syslog(LOG_DEBUG,"Memory (16): %p %p %lX 0x%04X 0x%04X\n", memory, r, (uint8_t*) r - memory, *offset, *((uint16_t*) r));
            IP += 2;
            break;
        case ARG_MODRM_16:
            r = decode_modrm_16(memory[INSTRUCTION_ADDRESS+1]);
            syslog(LOG_DEBUG,"ModRM (16): %p %p %lX 0x%02X\n", memory, r, (uint8_t*) r - memory, *((uint8_t*) r));
            IP += 1;
            break;
        case ARG_MODRM_RM_R8:
        case ARG_MODRM_RM_R16:
        case ARG_MODRM_RM_SEG:
        case ARG_MODRM_REG_R8:
        case ARG_MODRM_REG_R16:
        case ARG_MODRM_REG_SEG:
            syslog(LOG_ERR,"ModR/M arguments not supported for now here!\n");
            //r =  get_modrm_address(argument, XXXX)    
        case ARG_INVALID:
        default:
            syslog(LOG_ERR,"Invalid argument type!\n");
    }
    return r;
}

bool is_modrm(enum ArgType argument)
{
    bool b;
    switch(argument) {
        case ARG_MODRM_RM_R8:
        case ARG_MODRM_RM_R16:
        case ARG_MODRM_RM_SEG:
        case ARG_MODRM_REG_R8:
        case ARG_MODRM_REG_R16:
        case ARG_MODRM_REG_SEG:
            b = true; 
            break;
        default:
            b = false;
    }
    return b;
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

    // 55 push BP
    cpu_instructions[0x55].type = FCN_WORD;
    cpu_instructions[0x55].fcn.word = &op_pushw;
    cpu_instructions[0x55].op1 = ARG_BP;
    // 60 pusha
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
    // 88 mov mem8,reg8
    cpu_instructions[0x88].type = FCN_MODRM_BYTEBYTE;
    cpu_instructions[0x88].fcn.bytebyte = &opvar_movbb;
    cpu_instructions[0x88].op1 = ARG_MODRM_RM_R8;
    cpu_instructions[0x88].op2 = ARG_MODRM_REG_R8;
    // 89 mov mem16,reg16
    cpu_instructions[0x89].type = FCN_MODRM_WORDWORD;
    cpu_instructions[0x89].fcn.wordword = &op_movww;
    cpu_instructions[0x89].op1 = ARG_MODRM_RM_R16;
    cpu_instructions[0x89].op2 = ARG_MODRM_REG_R16; 
    // B0 mov AL, imm8
    // B1 mov CL, imm8
    // B4 mov AH, imm8
    // ...
    for(uint8_t i = 0xB0; i <= 0xB7; i++) {
        cpu_instructions[i].type = FCN_BYTEBYTE_OBSOLETE;
        cpu_instructions[i].fcn.bytebyte_obsolete = &op_movbb;
        cpu_instructions[i].op1 = decode_argument_r8(i - 0xB0);
        cpu_instructions[i].op2 = ARG_MEMORY_8;
    }
    // cpu_instructions[0xB0].type = FCN_BYTEBYTE_OBSOLETE;
    // cpu_instructions[0xB0].fcn.bytebyte_obsolete = &op_movbb;
    // cpu_instructions[0xB0].op1 = ARG_AL;
    // cpu_instructions[0xB0].op2 = ARG_MEMORY_8;
    
    // B4 mov AH, imm8
    // cpu_instructions[0xB4].type = FCN_BYTEBYTE_OBSOLETE;
    // cpu_instructions[0xB4].fcn.bytebyte_obsolete = &op_movbb;
    // cpu_instructions[0xB4].op1 = ARG_AH;
    // cpu_instructions[0xB4].op2 = ARG_MEMORY_8;

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
    // 27 daa none
    cpu_instructions[0x27].type = FCN_NOARG;
    cpu_instructions[0x27].fcn.noarg = &op_daa;
    // 14 adc AL,imm8
    cpu_instructions[0x14].type = FCN_BYTEBYTE_OBSOLETE;
    cpu_instructions[0x14].fcn.bytebyte_obsolete = &op_adcbb;
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
    // 24 and AL,imm8
    cpu_instructions[0x24].type = FCN_BYTEBYTE;
    cpu_instructions[0x24].fcn.bytebyte = &opvar_logicbb;
    strcpy(cpu_instructions[0x24].variant, "and");
    cpu_instructions[0x24].op1 = ARG_AL;
    cpu_instructions[0x24].op2 = ARG_MEMORY_8;
    // 30 xor mem8,reg8
    cpu_instructions[0x30].type = FCN_MODRM_BYTEBYTE;
    cpu_instructions[0x30].fcn.bytebyte = &opvar_logicbb;
    strcpy(cpu_instructions[0x30].variant, "xor");
    cpu_instructions[0x30].op1 = ARG_MODRM_RM_R8;
    cpu_instructions[0x30].op2 = ARG_MODRM_REG_R8;
    // 08 or mem8,reg8
    cpu_instructions[0x08].type = FCN_MODRM_BYTEBYTE;
    cpu_instructions[0x08].fcn.bytebyte = &opvar_logicbb;
    strcpy(cpu_instructions[0x08].variant, "or");
    cpu_instructions[0x08].op1 = ARG_MODRM_RM_R8;
    cpu_instructions[0x08].op2 = ARG_MODRM_REG_R8;
    // EB jmp rel8
    cpu_instructions[0xEB].type = FCN_BYTE_VARIANT;
    cpu_instructions[0xEB].fcn.byte_variant = &opvar_jump;
    strcpy(cpu_instructions[0xEB].variant, "jmp");
    cpu_instructions[0xEB].op1 = ARG_MEMORY_8;
    // 72 jb rel8
    cpu_instructions[0x72].type = FCN_BYTE_VARIANT;
    cpu_instructions[0x72].fcn.byte_variant = &opvar_jump;
    strcpy(cpu_instructions[0x72].variant, "jb");
    cpu_instructions[0x72].op1 = ARG_MEMORY_8;
    // 76 jbe rel8
    cpu_instructions[0x76].type = FCN_BYTE_VARIANT;
    cpu_instructions[0x76].fcn.byte_variant = &opvar_jump;
    strcpy(cpu_instructions[0x76].variant, "jbe");
    cpu_instructions[0x76].op1 = ARG_MEMORY_8;

    // FE dec reg8
    // FE inc reg8 !
    cpu_instructions[0xFE].type = FCN_GENERIC;
    cpu_instructions[0xFE].fcn.generic = &op_incb;
    cpu_instructions[0xFE].op1 = ARG_MODRM_RM_R8;
    // 3C cmp AL, imm8
    cpu_instructions[0x3C].type = FCN_BYTEBYTE_OBSOLETE;
    cpu_instructions[0x3C].fcn.bytebyte_obsolete = &op_cmp;
    cpu_instructions[0x3C].op1 = ARG_AL;
    cpu_instructions[0x3C].op2 = ARG_MEMORY_8;
    // 80 cmp reg8,imm8
    cpu_instructions[0x80].type = FCN_BYTEBYTE;
    cpu_instructions[0x80].fcn.bytebyte = &opvar_cmp;
    cpu_instructions[0x80].op1 = ARG_MODRM_RM_R8;
    cpu_instructions[0x80].op2 = ARG_MEMORY_8;

    /* Memory */
    stack_start = STACK_ADDRESS;
    //printf("STACK starts at 0x%04X\n", stack_start);
    print_info();
}

void cpu_run()
{
    uint64_t ticks = 0;
    uint8_t opcode;
    uint8_t *op1_8, *op2_8;
    uint16_t *op1_16, *op2_16;
    void *op_args[2] = {NULL, NULL};
    ModRM tmp_modrm;
    bool running = true;
    while(running) {
        opcode = memory[INSTRUCTION_ADDRESS];
        syslog(LOG_DEBUG,"#%ld address: 0x%04X, opcode: 0x%02X\n", ticks, INSTRUCTION_ADDRESS, opcode);
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
                    if (is_modrm(cpu_instructions[opcode].op1) || is_modrm(cpu_instructions[opcode].op2)) {
                      tmp_modrm.byte = next_byte();  
                    }
                    if (is_modrm(cpu_instructions[opcode].op1)) {
                        op1_8 = (uint8_t*) get_modrm_address(cpu_instructions[opcode].op1, tmp_modrm);
                    } else {
                        op1_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op1);
                    }
                    if (is_modrm(cpu_instructions[opcode].op2)) {
                        op2_8 = (uint8_t*) get_modrm_address(cpu_instructions[opcode].op2, tmp_modrm);
                    } else {
                        op2_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op2);
                    }
                    (*cpu_instructions[opcode].fcn.bytebyte)(op1_8, op2_8, cpu_instructions[opcode].variant);
                    break;
                case FCN_BYTEBYTE_OBSOLETE:
                    op1_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op1);
                    op2_8 = (uint8_t*) get_argument_address(cpu_instructions[opcode].op2);
                    (*cpu_instructions[opcode].fcn.bytebyte_obsolete)(op1_8, op2_8);
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
                    (*cpu_instructions[opcode].fcn.bytebyte)(op1_8, op2_8, cpu_instructions[opcode].variant);
                    break;    
                case FCN_MODRM_WORDWORD:
                    tmp_modrm.byte = next_byte();
                    op1_16 = (uint16_t*) get_modrm_address(cpu_instructions[opcode].op1, tmp_modrm);
                    op2_16 = (uint16_t*) get_modrm_address(cpu_instructions[opcode].op2, tmp_modrm);
                    (*cpu_instructions[opcode].fcn.wordword)(op1_16, op2_16);
                    break;
                case FCN_GENERIC:
                    if (is_modrm(cpu_instructions[opcode].op1) || is_modrm(cpu_instructions[opcode].op2)) {
                      tmp_modrm.byte = next_byte();  
                    }
                    // Op 1
                    if (is_modrm(cpu_instructions[opcode].op1)) {
                        op_args[0] = get_modrm_address(cpu_instructions[opcode].op1, tmp_modrm);
                    } else {
                        op_args[0] =  get_argument_address(cpu_instructions[opcode].op1);
                    }
                    // Op 2
                    if (is_modrm(cpu_instructions[opcode].op2)) {
                         op_args[1] = get_modrm_address(cpu_instructions[opcode].op2, tmp_modrm);
                    } else {
                         op_args[1] = get_argument_address(cpu_instructions[opcode].op2);
                    }
                    (*cpu_instructions[opcode].fcn.generic)((uint16_t) opcode, op_args);
                    break;

                default:
                    syslog(LOG_ERR,"Unknown function type %d\n", cpu_instructions[opcode].type);
            }
        } else {
            syslog(LOG_ERR,"Not implemented instruction 0x%02X\n", opcode);
            clean_up_and_exit(1);
        }
        print_info();
        // Step in
        IP += 1;
        ticks += 1;
        if (ticks > 5500) running = false;
    }
}

/* Loading binary program from a file - e.g. from a COM file */
void load_program(char *fn, uint16_t offset)
{
    FILE *fp;
    uint32_t length;
    printf("Loading program from %s\n", fn);
    syslog(LOG_INFO,"Loading program from %s\n", fn);
    fp = fopen(fn, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        length = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fread (memory+offset, 1, length, fp) != length) {
            printf("Error reading!\n");
            syslog(LOG_ERR, "Error reading %s!\n", fn);
        }
        fclose (fp);
    } else {
        printf("Cannot open file!\n");
        syslog(LOG_ERR, "Cannot open file %s!\n", fn);
    }
}

void clean_up_and_exit(int status)
{
    printf("Exit(%d)\n", status);
    closelog();
    exit(status);
}

/* Printing */
void print_stack(void)
{
    const uint16_t buffer_size = 64;
    char buffer[buffer_size];
    uint16_t i = stack_start - 1, j = 0, written;
    strcpy(buffer, "");
    while (i >= STACK_ADDRESS) {
        if ((stack_start -1 - i) > 0 && (stack_start -1 - i) % 8 == 0) {
            //syslog(LOG_DEBUG,"\n");
            syslog(LOG_DEBUG, "%s", buffer);
            j = 0;
        }
        //syslog(LOG_DEBUG,"%02X ", memory[i]);
        written = sprintf(buffer+j, "%02X ", memory[i]);
        i--;
        j += written; 
    } 
    syslog(LOG_DEBUG, "%s", buffer);
}

void print_registry(void)
{
    syslog(LOG_DEBUG,"AX %04X  BX %04X  CX %04X  DX %04x\n", AX, BX, CX, DX);
    syslog(LOG_DEBUG,"SP %04X  BP %04X  SI %04X  DI %04X\n", SP, BP, SI, DI);
    syslog(LOG_DEBUG,"ES %04X  CS %04X  SS %04X  DS %04X\n", ES, CS, SS, DS);
    syslog(LOG_DEBUG,"FLAGS %04X (CF=%d,OF=%d,AF=%d,ZF=%d,SF=%d)\n", FLAGS.X, FLAGS.CF, FLAGS.OF, FLAGS.AF, FLAGS.ZF, FLAGS.SF);
    syslog(LOG_DEBUG,"INSTRUCTION AT: %04X\n", INSTRUCTION_ADDRESS);
    syslog(LOG_DEBUG,"STACK AT:       %04X (%04X)\n", STACK_ADDRESS, stack_start);
}

void print_info(void)
{
    syslog(LOG_DEBUG,"--- REGISTRY & STACK ---\n");
    print_registry();
    print_stack();
    syslog(LOG_DEBUG,"------------------------\n");
}