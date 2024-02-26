#ifndef CPU8086_H
#define CPU8086_H

typedef uint8_t bit;

/* Status (or flag) register */
typedef union {
    uint16_t X;
    struct {
        uint8_t H;
        uint8_t L;
    };
    struct {
        bit not_used_12_to_15:4;
        bit OF:1;
        bit DF:1;
        bit IF:1;
        bit TF:1;
        bit SF:1;
        bit ZF:1;
        bit not_used_5:1;
        bit AF:1;
        bit not_used_3:1;
        bit PF:1;
        bit not_used_1:1;
        bit CF:1;
    };
} StatusRegister;

/* General purposes registers */
typedef union {
    uint16_t X;
    struct {
        uint8_t L; //L
        uint8_t H; //H
    };
} GeneralPurposeRegister;

#define AX A.X
#define AH A.H
#define AL A.L

#define BX B.X
#define BH B.H
#define BL B.L

#define CX C.X
#define CH C.H
#define CL C.L

#define DX D.X
#define DH D.H
#define DL D.L

enum ArgType {
    ARG_INVALID, 
    ARG_FLAGS,
    ARG_AX, ARG_AH, ARG_AL,
    ARG_BX, ARG_BH, ARG_BL,
    ARG_CX, ARG_CH, ARG_CL,
    ARG_DX, ARG_DH, ARG_DL,
    ARG_CS, ARG_DS, ARG_SS, ARG_ES,
    ARG_IP, ARG_BP, ARG_SP,
    ARG_SI, ARG_DI,
    ARG_MEMORY_8, ARG_MEMORY_16,
    ARG_MODRM_8, ARG_MODRM_16,
    ARG_MODRM_RM_R8, ARG_MODRM_REG_R8,
    ARG_MODRM_RM_R16, ARG_MODRM_RM_SEG, 
    ARG_MODRM_REG_R16, ARG_MODRM_REG_SEG,
}; 

/* All the other registers */
typedef uint16_t Register;

/* Instructions table */
typedef union {
  void (*noarg)(void);
  void (*byte)(uint8_t *op1);
  void (*byte_variant)(uint8_t *op1, char *variant);
  void (*bytebyte)(uint8_t *op1, uint8_t *op2);
  void (*word)(uint16_t *op1);
  void (*wordword)(uint16_t *op1, uint16_t *op2);
  void (*wordbyte)(uint16_t *op1, uint8_t *op2);
} fcn_instruction;

enum FcnType {
    FCN_INVALID, FCN_NOARG, 
    FCN_BYTE, FCN_BYTE_VARIANT, FCN_WORD, FCN_WORD_VARIANT,
    FCN_BYTEBYTE, FCN_WORDWORD, FCN_WORDBYTE, 
    FCN_MODRM_BYTEBYTE, FCN_MODRM_WORDWORD
};

typedef struct {
    enum FcnType type;
    fcn_instruction fcn;
    char variant[8];
    enum ArgType op1;
    enum ArgType op2;
} CpuInstruction;


/* ModR/M byte*/
typedef union {
    uint8_t byte;
    struct {
        uint8_t rm:3;
        uint8_t reg:3;
        uint8_t mod:2;
    };
} ModRM;



// typedef void (*fcn_instruction)(void);

/* Memory */
#define INSTRUCTION_ADDRESS (16 * CS + IP)
#define STACK_ADDRESS (16 * SS + SP)

# endif