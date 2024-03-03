#ifndef CPU8086_INSTRUCTIONS_H
#define CPU8086_INSTRUCTIONS_H

// Declarations of helper functions
uint8_t carryflag8(uint8_t b1, uint8_t b2);
uint8_t parity8(uint8_t b);
void setflag_zsp8(uint8_t b);
uint8_t parity16(uint16_t w);
void setflag_zsp16(uint16_t w);

// Declarations of CPU instructions calls
void op_incb(uint16_t opcode, void **args);

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
void op_call(uint16_t *w1);
void op_ret(void);
DEFINE_VARIANT_FCN(opvar_logicbb, uint8_t *b1, uint8_t *b2);
void op_adcbb(uint8_t *b1, uint8_t *b2);
void op_xorbb(uint8_t *b1, uint8_t *b2);
void op_orbb(uint8_t *b1, uint8_t *b2);
// void op_incb(uint16_t opcode, void **args);
void op_subbb(uint8_t *b1, uint8_t *b2);
void op_cmp(uint8_t *b1, uint8_t *b2);
DEFINE_VARIANT_FCN(opvar_cmp, uint8_t *b1, uint8_t *b2);
void op_int(uint8_t *b1);

#endif