
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include <stdio.h>
#include <string.h>

#include "cpu8086.h"
#include "cpu8086_instructions.h"
#include "bios.h"

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

// void op_incb(uint16_t opcode, void **args) 
// {
//     uint8_t *b1;
//     syslog(LOG_DEBUG,"op_incb 0x%04X", opcode);
//     switch(opcode) {
//         case 0xFE:
//             //FE inc reg8
//             b1 = args[0];
//             syslog(LOG_DEBUG,"Arg(s): %p; 0x%02X", b1, *b1);
//             *b1 = *b1 + 1;
//             break;
//         default:
//             syslog(LOG_ERR,"Not supported opcode 0x%04X", opcode);
//     }
// }

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

/* Interrupt calls */
void op_int(uint8_t *b1)
{
    syslog(LOG_DEBUG,"op_int %p 0x%02X\n", b1, *b1);
    // mov ah, function number
    //    ; input parameters
    //    int 21h
    // Resources used:
    // https://www.philadelphia.edu.jo/academics/qhamarsheh/uploads/Lecture%2021%20MS-DOS%20Function%20Calls%20_INT%2021h_.pdf
    // http://bbc.nvg.org/doc/Master%20512%20Technical%20Guide/m512techb_int21.htm
    bios_interrupt(*b1);
}	