// mipsi.h

#define SYMBOLS (1024)
#define SYMBOL_LEN (32)
#define INS_BUF_LEN (4096)
#define MEM_SIZE (65536)
#define REGS (32)

enum opcode {
	OP_ADD, OP_ADDI, OP_AND, OP_ANDI, OP_BEQ, OP_BNE, OP_J, OP_JAL, OP_JR,
	OP_LW, OP_NOP, OP_NOR, OP_ORI, OP_SLT, OP_SLTI, OP_SLL, OP_SLR, OP_SUB,
	OP_SW, OP_LAST
};

enum reg {
	REG_ZERO, REG_AT, REG_V0, REG_V1, REG_A0, REG_A1, REG_A2, REG_A3, REG_T0,
	REG_T1, REG_T2, REG_T3, REG_T4, REG_T5, REG_T6, REG_T7, REG_T8, REG_T9,
	REG_S0, REG_S1, REG_S2, REG_S3, REG_S4, REG_S5, REG_S6, REG_S7, REG_S8,
	REG_GP, REG_SP, REG_RA, REG_LAST
};

struct ins {
	int label;
	enum { R_TYPE, I_TYPE, J_TYPE } type;
	enum opcode op;

	union {
		struct { enum reg rs, rt, rd; } r;
		struct { enum reg rs, rt; int imm; } i;
		struct { int imm; } j;
	} un;
};

extern int n_ins;
extern struct ins ins_buf[INS_BUF_LEN];
extern int n_symbols;
extern char symbol[SYMBOLS][SYMBOL_LEN];
extern int lookup(const char *s);
extern char *reg_name(enum reg r);
extern char *opcode_name(enum opcode op);
extern enum reg make_reg(const char *s);
extern enum opcode make_opcode(const char *s);
