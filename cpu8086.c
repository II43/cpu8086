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
DEFINE_VARIANT_FCN(opvar_movbb, uint8_t *b1, uint8_t *b2);
void op_lodsb(void);
void op_stosb(void);
void op_stosw(void);
void op_pushw(uint16_t *w1);
void op_popw(uint16_t *w1);
void op_rolwb(uint16_t *w1, uint8_t *b2);
void op_andbb(uint8_t *b1, uint8_t *b2);
void op_daa(void);
void op_decw(uint16_t *w1);
//void opvar_jump(uint8_t *b1, char *variant);
DEFINE_VARIANT_FCN(opvar_jump, uint8_t *b1);
void op_jz(uint8_t *b1);
void op_jnz(uint8_t *b1);
void op_loop(uint8_t *b1);
void op_int(uint8_t *b1);
void op_call(uint16_t *w1);
void op_ret(void);
DEFINE_VARIANT_FCN(opvar_logicbb, uint8_t *b1, uint8_t *b2);
void op_adcbb(uint8_t *b1, uint8_t *b2);
void op_xorbb(uint8_t *b1, uint8_t *b2);
void op_orbb(uint8_t *b1, uint8_t *b2);
void op_incb(uint16_t opcode, void **args);
void op_subbb(uint8_t *b1, uint8_t *b2);
void op_cmp(uint8_t *b1, uint8_t *b2);
DEFINE_VARIANT_FCN(opvar_cmp, uint8_t *b1, uint8_t *b2);

uint8_t carryflag8(uint8_t b1, uint8_t b2);
uint8_t parity8(uint8_t b);
void setflag_zsp8(uint8_t b);
uint8_t parity16(uint16_t w);
void setflag_zsp16(uint16_t w);

void print_stack(void);
void print_registry(void);
void print_info(void);

void clean_up_and_exit(int status);

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

void* decode_modrm_r16(uint8_t value)
{
    void *r = NULL;
    switch(value) {
        case 0x00: r = (void*)&AX; break;
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

/* Instructions */
void op_addb(uint8_t *b1)
{
    syslog(LOG_DEBUG,"op_addb %p (AL at %p = 0x%02X), 0x%02X + 0x%02X\n", b1, &AL, AL, *b1, memory[INSTRUCTION_ADDRESS+1]);
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
    syslog(LOG_DEBUG,"op_pushf, stack address: 0x%04X\n", STACK_ADDRESS);
    SP -= 2;
    memory[STACK_ADDRESS+2] = FLAGS.H;
    memory[STACK_ADDRESS+1] = FLAGS.L;
    print_stack();
}

void op_pusha(void)
{
    syslog(LOG_DEBUG,"op_pusha, stack address: 0x%04X\n", STACK_ADDRESS);
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

// Not used - to be removed
void op_push_cs(void)
{
    syslog(LOG_DEBUG,"op_push CS, stack address: 0x%04X\n", STACK_ADDRESS);
    SP -= 2;
    memcpy(memory+STACK_ADDRESS+1, &CS, 2);
    //printf("0x%04X, 0x%02X 0x%02X\n", CS, memory[STACK_ADDRESS+1], memory[STACK_ADDRESS+2]);
    print_stack();
}

void op_movww(uint16_t *w1, uint16_t *w2)
{
    syslog(LOG_DEBUG,"op_movww %p 0x%04X <-- %p; 0x%04X\n", w1, *w1, w2, *w2);
    *w1 = *w2;
}

void op_movsw(void)
{
    // A5 movsw none
    // Move word at address DS:(E)SI to address ES:(E)DI
    syslog(LOG_DEBUG,"op_movsw %02X %02X <-- %02X %02X\n", memory[16*ES+DI], memory[16*ES+DI+1], memory[16*DS+SI], memory[16*DS+SI+1]);
    memory[16*ES+DI] = memory[16*DS+SI];
    memory[16*ES+DI+1] = memory[16*DS+SI+1];
}

void op_movbb(uint8_t *b1, uint8_t *b2)
{
    syslog(LOG_DEBUG,"op_movbb %p 0x%02X <-- %p 0x%02X\n", b1, *b1, b2, *b2);
    *b1 = *b2;
}


DEFINE_VARIANT_FCN(opvar_movbb, uint8_t *b1, uint8_t *b2)
{
    // Just forward to movbb - no variants
    op_movbb(b1, b2);
}

void op_lodsb(void)
{
    // AC lodsb none
    // Load byte at address DS:(E)SI into AL.
    syslog(LOG_DEBUG,"op_lodsb %02X (16*DS+SI) <-- %02X '%c' (AL)\n", memory[16*DS+SI], AL, AL);
    AL = memory[16*DS+SI];
    if(FLAGS.DF == 0) SI += 1;
	else SI -= 1;
}

void op_stosb(void)
{
    // AA stosb none
    // Store AL at address ES:(E)DI.
    syslog(LOG_DEBUG,"op_stosb %02X (16*ES+DI) <-- %02X '%c' (AL)\n", memory[16*ES+DI], AL, AL);
    memory[16*ES+DI] = AL;
    DI += 1;
}

void op_stosw(void)
{
    // AB stosw none
    // Store AX at address ES:(E)DI.
    syslog(LOG_DEBUG,"op_stosw %02X (16*ES+DI) <-- %02X (AX)\n", memory[16*ES+DI], AX);
    memcpy(memory+16*ES+DI, &AX, 2);
    DI += 2;
}


void op_pushw(uint16_t *w1)
{
    syslog(LOG_DEBUG,"op_pushw %p, stack address: 0x%04X\n", w1, STACK_ADDRESS);
    syslog(LOG_DEBUG,"Pushing %04X (%p)\n", *w1, w1);
    SP -= 2;
    memcpy(memory+STACK_ADDRESS, w1,  2);
    //printf("0x%04X (AX=0x%04X), 0x%02X 0x%02X\n", *w1, AX, memory[STACK_ADDRESS+1], memory[STACK_ADDRESS+2]);
    //print_stack();
}

void op_popw(uint16_t *w1)
{
    syslog(LOG_DEBUG,"op_popw %p, stack address: 0x%04X\n", w1, STACK_ADDRESS);
    memcpy(w1, memory+STACK_ADDRESS, 2);
    SP += 2;
    syslog(LOG_DEBUG,"Poping: 0x%04X\n", *w1);
}

void op_rolwb(uint16_t *w1, uint8_t *b2)
{
    syslog(LOG_DEBUG,"op_rolwb %p; %X << %X\n", w1, *w1, *b2);
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
    syslog(LOG_DEBUG,"Result at %p = 0x%04X\n", w1, *w1);
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
    syslog(LOG_DEBUG,"op_daa\n");
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
    syslog(LOG_DEBUG,"op_adcbb %p %p; (AL at %p) 0x%02X + 0x%02X + %d\n", b1, b2, &AL, *b1, *b2, FLAGS.CF);
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
    syslog(LOG_DEBUG,"op_decw %p 0x%04X\n", w1, *w1);
    uint32_t r32;
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
	if ((*w1 ^ d ^ r32) & 0x1000) FLAGS.AF = 1;
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

DEFINE_VARIANT_FCN(opvar_jump, uint8_t *b1)
{
    bool b_jumped = false;
    int16_t sb1 = (int16_t)((int8_t)*b1);
    syslog(LOG_DEBUG,"opvar_jump (%s) %p 0x%02X %d\n", variant, b1, *b1, sb1);
    if (CHECK_VARIANT("jmp")) {
        // EB jmp rel8
        b_jumped = true; 
    } else if(CHECK_VARIANT("jb")) {
        // 72 jb rel8
        if (FLAGS.CF) b_jumped = true;    
    } else if(CHECK_VARIANT("jbe")) {
        // 76 jbe rel8
        if (FLAGS.CF || FLAGS.ZF) b_jumped = true;
    } else {
        syslog(LOG_ERR,"Unknown variant: %s", variant);
        clean_up_and_exit(1);
    }
    if (b_jumped) {
        IP = IP + sb1;
        syslog(LOG_DEBUG,"Jumped to 0x%04X\n", IP);
    }
}

void op_jz(uint8_t *b1)
{
    /* JNE and JNZ are just different names for a conditional jump when ZF is equal to 0. */
    int16_t sb1 = (int16_t)((int8_t)*b1);
    syslog(LOG_DEBUG,"op_jz %p 0x%02X %d\n", b1, *b1, sb1);
    if(FLAGS.ZF == 1) {
        IP = IP + sb1;
        syslog(LOG_DEBUG,"Jumped to 0x%04X\n", IP);
    }
}

void op_jnz(uint8_t *b1)
{
    /* JNE and JNZ are just different names for a conditional jump when ZF is equal to 0. */
    int16_t sb1 = (int16_t)((int8_t)*b1);
    syslog(LOG_DEBUG,"op_jnz %p 0x%02X %d\n", b1, *b1, sb1);
    if(FLAGS.ZF == 0) {
        IP = IP + sb1;
        syslog(LOG_DEBUG,"Jumped to 0x%04X\n", IP);
    }
}

void op_loop(uint8_t *b1)
{
    int16_t sb1 = (int16_t)((int8_t)*b1);
    syslog(LOG_DEBUG,"op_loop %p 0x%02X %d, CX=%d\n", b1, *b1, sb1, CX);
    CX = CX - 1;
    if(CX) {
        IP = IP + sb1;
        syslog(LOG_DEBUG,"Jumped to 0x%04X\n", IP);
    }
}

void op_int(uint8_t *b1)
{
    uint16_t i = 0;
    syslog(LOG_DEBUG,"op_int %p 0x%02X\n", b1, *b1);
    // mov ah, function number
    //    ; input parameters
    //    int 21h
    // Resources used:
    // https://www.philadelphia.edu.jo/academics/qhamarsheh/uploads/Lecture%2021%20MS-DOS%20Function%20Calls%20_INT%2021h_.pdf
    // http://bbc.nvg.org/doc/Master%20512%20Technical%20Guide/m512techb_int21.htm
    if (*b1 == 0x21) {
        syslog(LOG_DEBUG,"Interrupt 0x%02X: MS-DOS Function Call %02X\n", *b1, AH);
        switch(AH) {
            case 0x09:
                // 09h - Write string (terminated by a $ character)to standard output.
                // DS:DX = segment:offset of string
                i = 0;
                syslog(LOG_DEBUG,"PRINTING (0x%04X %c):\n", 16*DS + 0*DX, memory[16*DS+DX+i]);
                // $ = 0x24 = 36
                while (memory[16*DS+DX+i] != '$') {
                    printf("%c", memory[16*DS+DX+i]);
                    syslog(LOG_INFO,"%c", memory[16*DS+DX+i]);
                    i++;
                }
                //printf("\n");
                break;
            case 0x4C:
                syslog(LOG_DEBUG,"EXITING 0x%02X!\n", AL);
                clean_up_and_exit(AL);
                break;
            default:
                syslog(LOG_DEBUG,"Not implemented interrupt routine AH = 0x%02X\n", AH);
                clean_up_and_exit(1);   
        }
    } else if (*b1 == 0x10) {
        syslog(LOG_DEBUG,"Interrupt 0x%02X: MS-DOS Function Call %02X\n", *b1, AH);
        switch(AH) {
            case 0x0E:
                printf("%c", AL);
                syslog(LOG_DEBUG,"PRINTING: %c\n", AL);
                break;
            default:
                syslog(LOG_DEBUG,"Not implemented interrupt routine AH = 0x%02X\n", AH);
                clean_up_and_exit(1);
        }

    } else {
        syslog(LOG_DEBUG,"Not implemented interrupt call 0x%02X\n", *b1);
        clean_up_and_exit(1);
    }
    
}	

void op_call(uint16_t *w1)
{
    int16_t sw1 = (int16_t)(*w1);
    syslog(LOG_DEBUG,"op_call %p 0x%04X %d\n", w1, *w1, sw1);
    op_pushw(&IP);
    IP = IP + sw1;
    syslog(LOG_DEBUG,"Jumped to 0x%04X\n", IP);
}

void op_ret(void)
{
    uint16_t sw1;
    op_popw(&sw1);
    syslog(LOG_DEBUG,"op_ret 0x%04X\n", sw1);
    if (sw1 == 0x0000) {
        syslog(LOG_DEBUG,"Seems like nothing is on stack (0x%04X)! Nowhere to return = exit program!\n", sw1);
        clean_up_and_exit(0);
    }
    IP = sw1;
    syslog(LOG_DEBUG,"Returned to 0x%04X\n", IP);
}

/* Logic instructions*/
DEFINE_VARIANT_FCN(opvar_logicbb, uint8_t *b1, uint8_t *b2)
{
    syslog(LOG_DEBUG,"opvar_logicbb (%s) %p %p 0x%02X 0x%02X\n", variant, b1, b2, *b1, *b2);    
    if (CHECK_VARIANT("and")) {
        *b1 = *b1 & *b2;
    } else if (CHECK_VARIANT("or")) {
        *b1 = *b1 | *b2;
    } else if (CHECK_VARIANT("xor")) {
        *b1 = *b1 ^ *b2;
    } else {
        syslog(LOG_DEBUG,"Unknown variant: %s", variant);
        clean_up_and_exit(1);
    }
    FLAGS.CF = 0;
    FLAGS.OF = 0;
    setflag_zsp8(*b1);
}

void op_andbb(uint8_t *b1, uint8_t *b2)
{
    syslog(LOG_DEBUG,"op_andbb %p %p; (AL at %p) 0x%02X & 0x%02X\n", b1, b2, &AL, *b1, *b2);
    *b1 = *b1 & *b2;
    FLAGS.CF = 0;
    FLAGS.OF = 0;
    setflag_zsp8(*b1);
}

void op_xorbb(uint8_t *b1, uint8_t *b2)
{
    syslog(LOG_DEBUG,"op_xorbb %p %p; (BH at %p) 0x%02X & 0x%02X\n", b1, b2, &BH, *b1, *b2);
    *b1 = *b1 ^ *b2;
    FLAGS.CF = 0;
    FLAGS.OF = 0;
    setflag_zsp8(*b1);
}

void op_orbb(uint8_t *b1, uint8_t *b2)
{
    syslog(LOG_DEBUG,"op_orbb %p %p; 0x%02X & 0x%02X\n", b1, b2, *b1, *b2);
    *b1 = *b1 | *b2;
    FLAGS.CF = 0;
    FLAGS.OF = 0;
    setflag_zsp8(*b1);
}

void op_incb(uint16_t opcode, void **args) 
{
    uint8_t *b1;
    syslog(LOG_DEBUG,"op_incb 0x%04X", opcode);
    switch(opcode) {
        case 0xFE:
            //FE inc reg8
            b1 = args[0];
            syslog(LOG_DEBUG,"Arg(s): %p; 0x%02X", b1, *b1);
            *b1 = *b1 + 1;
            break;
        default:
            syslog(LOG_ERR,"Not supported opcode 0x%04X", opcode);
    }
}

void op_subbb(uint8_t *b1, uint8_t *b2)
{
    syslog(LOG_DEBUG,"op_cmp %p 0x%02X - %p 0x%02X\n", b1, *b1, b2, *b2);
    uint16_t r16;
    r16 = (uint16_t)*b1 - (uint16_t)*b2;
    *b1 -= *b2;
    setflag_zsp8(*b1);
    // OF
	if ((r16 ^ *b1) & (*b1 ^ *b2) & 0x80) FLAGS.OF = 1;
    else FLAGS.OF = 0;
    // CF
	if (r16 & 0xFF00) FLAGS.CF = 1;
    else FLAGS.CF = 0;
	// AF		
	if ((*b1 ^ *b2 ^ r16) & 0x10) FLAGS.AF = 1;
    else FLAGS.AF = 0;
}

void op_cmp(uint8_t *b1, uint8_t *b2)
{
    uint8_t tmp = *b1;
    op_subbb(&tmp, b2);
}

DEFINE_VARIANT_FCN(opvar_cmp, uint8_t *b1, uint8_t *b2)
{
    // Just forward to movbb - no variants
    op_cmp(b1, b2);
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

void clean_up_and_exit(int status)
{
    printf("Exit(%d)\n", status);
    closelog();
    exit(status);
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        printf("Error: You must pass a binary program to run as first argument!\n");
        exit(1);
    }
    // Logging
    openlog("cpu8086", LOG_CONS, LOG_LOCAL0);
    // Self-Test
    GeneralPurposeRegister T;
    T.X = 0x1234;
    syslog(LOG_DEBUG,"Test of general purpose register\n");
    syslog(LOG_DEBUG,"X=0x%04X H=0x%02X L=0x%02X\n", T.X, T.H, T.L);

    // Main
    cpu_init();
    // load_program("yourhelp.com", 0x1FD0);
    load_program(argv[1], 0x1FD0);
    // 0x100+16*0x01ED = 0x100 + 0x1ED0

    // memory[0x1FD1] = 0x04;
    printf("Running\n");
    cpu_run();
    printf("Done");
    closelog();
}