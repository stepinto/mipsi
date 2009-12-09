#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>

#include "mipsi.h"
#include "parse.h"

#define RESCHED_BUF_LEN (16)

struct {
	enum opcode op;
	enum ins_type type;
	char *name;
} opcode_info[] = {
	{OP_ADD, R_TYPE, "add"}, {OP_ADDI, I_TYPE, "addi"},
	{OP_AND, R_TYPE, "and"}, {OP_ANDI, I_TYPE, "andi"}, {OP_BEQ, I_TYPE, "beq"},
	{OP_BNE, I_TYPE, "bne"}, {OP_J, J_TYPE, "j"}, {OP_JAL, J_TYPE, "jal"},
	{OP_JR, R_TYPE, "jr"}, {OP_LW, I_TYPE, "lw"}, {OP_NOP, R_TYPE, "nop"},
	{OP_NOR, R_TYPE, "nor"}, {OP_ORI, I_TYPE, "ori"}, {OP_SLT, R_TYPE, "slt"},
	{OP_SLTI, I_TYPE, "slti"}, {OP_SLL, I_TYPE, "sll"}, {OP_SLR, I_TYPE, "slr"},
	{OP_SUB, R_TYPE, "sub"}, {OP_SW, I_TYPE, "sw"}
};

struct reg_info {
	enum reg _reg;
	char *name;
} reg_info[] = {
	{REG_ZERO,"$zero"}, {REG_AT, "$at"}, {REG_V0, "$v0"}, {REG_V1, "$v1"},
	{REG_A0, "$a0"}, {REG_A1, "$a1"}, {REG_A2, "$a2"}, {REG_A3, "$a3"},
	{REG_T0, "$t0"}, {REG_T1, "$t1"}, {REG_T2, "$t2"}, {REG_T3, "$t3"},
	{REG_T4, "$t4"}, {REG_T5, "$t5"}, {REG_T6, "$t6"}, {REG_T7, "$t7"},
	{REG_T8, "$t8"}, {REG_T9, "$t9"}, {REG_S0, "$s0"}, {REG_S1, "$s1"},
	{REG_S2, "$s2"}, {REG_S3, "$s3"}, {REG_S4, "$s4"}, {REG_S5, "$s5"},
	{REG_S6, "$s6"}, {REG_S7, "$s7"}, {REG_S8, "$s8"}, {REG_GP, "$gp"},
	{REG_SP, "$sp"}, {REG_RA, "$ra"}
};


static void help();
static void die();
static void run();
static void resched();
static void debug_msg(const char *fmt, ...);
static struct ins *find_ins(int label);
static void print_ins(struct ins *i);

static void exec_ins();
static void init_cpu();
static void init_mem();

static int eval_resched_buf();
static void dfs_permu(int dep, int n);
static int has_dep(struct ins *i, struct ins *j);
static int need_resched();
static int is_branch_op(enum opcode op);

int stall_range = 2;
int n_ins;
struct ins ins_buf[INS_BUF_LEN];
int n_symbols;
char symbol[SYMBOLS][SYMBOL_LEN];

static struct {
	int reg_value[REG_LAST];
	struct ins* pc_mem, *pc_rb;
	struct ins resched_buf[RESCHED_BUF_LEN];
	int resched_buf_len;
} cpu;
static char mem[MEM_SIZE];

static int debug;
static int optimized;
static int profile;
static int resched_buf_len = 1;

extern FILE *yyin;
extern int yydebug;

int main(int argc, char **argv) {
	int opt;
	int i;

	// yydebug = 1;

	while ((opt = getopt(argc, argv, "dO:phs:")) >= 0) {
		switch (opt) {
			case 'd': debug = 1; break;
			case 'O': optimized = 1; resched_buf_len = atoi(optarg); break;
			case 'p': profile = 1; break;
			case 'h': help(); return 0;
			case 's': stall_range = atoi(optarg); break;
			default: help(); return 1;
		}
	}
	if (optind < argc) {
		debug_msg("Read from %s.\n", argv[optind]);
		yyin = fopen(argv[optind], "r");
		if (!yyin) die();
	}
	else
		yyin = stdin;

	debug_msg("Parse assembly code.\n");
	if (yyparse() > 0) { printf("Syntax error\n"); exit(1); }
	debug_msg("%d instruction(s) has been loaded.\n", n_ins);
	if (debug) {
		for (i = 0; i < n_ins; i++) {
			print_ins(&ins_buf[i]);
			printf("\n");
		}
	}

	debug_msg("Start exectuion. Reschedule-buffer: %d.\n", resched_buf_len);
	run();

	return 0;
}

static void help() {
	printf("Usage: mipsi [OPTION] FILE\n");
	printf("  -d        debug\n");
	printf("  -O NUM    optimize for performance\n");
	printf("  -p        enable profiling\n");
	printf("  -h        show this help\n");
	printf("  -s NUM    stall range, default 2\n");
	printf("\n");
}

static void run() {
	int n_data_hazards = 0;
	int n_cycles = 0;
	char *mem;
	int rs, rt, rd, imm;
	int i;

	init_cpu();
	while (cpu.pc_mem < ins_buf + n_ins) {
		if (need_resched())
			resched();
	
		if (debug) {
			printf("%d: ", cpu.pc_mem - ins_buf);
			print_ins(cpu.pc_rb);
		}

		exec_ins();

		if (debug) {
			for (i = 0; i < 4; i++)
				printf("%ss%d=%d", i==0?"\t# ":", ", i, cpu.reg_value[REG_S0 + i]);
			for (i = 0; i < 2; i++)
				printf(", t%d=%d", i, cpu.reg_value[REG_T0 + i]);
			printf("\n");
		}

		cpu.pc_mem++;
		cpu.pc_rb++;
	}

	if (profile)
		printf("Cycles: %d, data hazards: %d\n", n_cycles, n_data_hazards);
}

static int best_hazards;
static int permu[RESCHED_BUF_LEN];
static struct ins resched_buf_best[RESCHED_BUF_LEN];
static int used[RESCHED_BUF_LEN];
static void resched() {
	int i, n;

	n = 0;
	while (n < resched_buf_len && !is_branch_op(cpu.pc_mem[n].op))
		n++;
	memcpy(cpu.resched_buf, cpu.pc_mem, n * sizeof(struct ins));
	cpu.resched_buf_len = n;

	best_hazards = 0x7fffffff;  // infinity
	dfs_permu(0, n);

	memcpy(cpu.resched_buf, resched_buf_best, n * sizeof(struct ins));
	if (n < resched_buf_len) {
		assert(is_branch_op(cpu.pc_mem[n].op));
		cpu.resched_buf[n] = cpu.pc_mem[n];
		n++;
	}
	cpu.pc_rb = cpu.resched_buf;
}

static int eval_resched_buf() {
	int i, j;
	int cnt = 0;

	for (i = 0; i < cpu.resched_buf_len; i++)
		for (j = 0; j < i && i-j < stall_range; j++)
			if (has_dep(&cpu.resched_buf[j], &cpu.resched_buf[i])) {
				cnt++;
				break;
			}
	return cnt;
}

static void dfs_permu(int dep, int n) {
	int i;
	struct ins resched_buf_tmp[RESCHED_BUF_LEN];
	int tmp;

	if (dep >= n) {
		memcpy(resched_buf_tmp, cpu.resched_buf, n * sizeof(struct ins));
		for (i = 0; i < n; i++)
			cpu.resched_buf[i] = resched_buf_tmp[permu[i]];
		tmp = eval_resched_buf();
		memcpy(cpu.resched_buf, resched_buf_tmp, n * sizeof(struct ins));

		if (tmp < best_hazards) {
			best_hazards = tmp;
			memcpy(resched_buf_best, resched_buf_tmp, n * sizeof(struct ins));
		}
		return;
	}

	for (i = 0; i < n; i++)
		if (!used[i]) {
			permu[dep] = i;
			used[i] = 1;
			dfs_permu(dep+1, n);
			used[i] = 0;
		}
}

static void debug_msg(const char *fmt, ...) {
	va_list vl;

	if (debug) {
		va_start(vl, fmt);
		vprintf(fmt, vl);
		va_end(vl);
	}
}

static struct ins *find_ins(int label) {
	int i;

	for (i = 0; i < INS_BUF_LEN; i++)
		if (ins_buf[i].label = label)
			return ins_buf + i;
	return NULL;
}

static void die() {
	perror("mipsi");
	exit(1);
}

static void print_ins(struct ins *i) {
	char *s;

	if (i->label > 0)
		printf("L%d:\t", i->label);
	else
		printf("\t");

	printf("%s\t", get_opcode_name(i->op));
	switch (i->type) {
		case R_TYPE:
			printf("%s, %s, %s", get_reg_name(i->un.r.rd),
					get_reg_name(i->un.r.rs),
					get_reg_name(i->un.r.rt));
			break;
		case I_TYPE:
			if (i->op == OP_LW || i->op == OP_SW)
				printf("%s, %d (%s)", get_reg_name(i->un.i.rt),
						i->un.i.imm, get_reg_name(i->un.i.rs));
			else
				printf("%s, %s, %d", get_reg_name(i->un.i.rt),
						get_reg_name(i->un.i.rs), i->un.i.imm);
			break;
		case J_TYPE:
			printf("%d", i->un.j.imm);
			break;
	}
}

char *get_reg_name(enum reg r) {
	int i;

	for (i = 0; i < sizeof(reg_info) / sizeof(reg_info[0]); i++)
		if (r == reg_info[i]._reg)
			return reg_info[i].name;
	assert(0);
	return NULL;
}

char *get_opcode_name(enum opcode op) {
	int i;

	for (i = 0; i < sizeof(opcode_info) / sizeof(opcode_info[0]); i++)
		if (op == opcode_info[i].op)
			return opcode_info[i].name;
	assert(0);
	return NULL;
}

int lookup_symbol(const char *s) {
	int i;
	for (i = 0; i < n_symbols; i++)
		if (strcmp(s, symbol[i]) == 0)
			return i+1;  // never return 0
	strcpy(symbol[n_symbols], s);
	n_symbols++;
	return n_symbols;
}

enum ins_type get_ins_type(enum opcode op) {
	int i;

	for (i = 0; i < sizeof(opcode_info) / sizeof(opcode_info[0]); i++)
		if (op == opcode_info[i].op)
			return opcode_info[i].type;
	assert(0);
	return (enum ins_type)(-1);
}

enum reg make_reg(const char *s) {
	int i;

	for (i = 0; i < sizeof(reg_info) / sizeof(reg_info[0]); i++)
		if (strcmp(reg_info[i].name, s) == 0)
			return (enum reg) reg_info[i]._reg;
	assert(0);
	return -1;
}

enum opcode make_opcode(const char *s) {
	int i;

	for (i = 0; i < sizeof(opcode_info) / sizeof(opcode_info[0]); i++)
		if (strcmp(opcode_info[i].name, s) == 0)
			return opcode_info[i].op;
	assert(0);
	return -1;
}

static void exec_ins() {
	int rs, rt, rd, imm;

	assert(cpu.pc_rb->type == get_ins_type(cpu.pc_rb->op));
	switch (cpu.pc_rb->type) {
		case R_TYPE:
			rs = cpu.pc_rb->un.r.rs;
			rt = cpu.pc_rb->un.r.rt;
			rd = cpu.pc_rb->un.r.rd;
			assert(0 <= rs && rs < REG_LAST);
			assert(0 <= rt && rt < REG_LAST);
			assert(0 <= rd && rd < REG_LAST);
			break;
		case I_TYPE:
			rs = cpu.pc_rb->un.i.rs;
			rt = cpu.pc_rb->un.i.rt;
			imm = cpu.pc_rb->un.i.imm;
			assert(0 <= rs && rs < REG_LAST);
			assert(0 <= rt && rt < REG_LAST);
			break;
		case J_TYPE:
			imm = cpu.pc_rb->un.j.imm;
			break;
		default:
			assert(0);
			break;
	}

	switch (cpu.pc_rb->op) {
		case OP_ADD:
			cpu.reg_value[rd] = cpu.reg_value[rs] + cpu.reg_value[rt];
			break;
		case OP_ADDI:
			cpu.reg_value[rt] = cpu.reg_value[rs] + imm;
			break;
		case OP_AND:
			cpu.reg_value[rd] = cpu.reg_value[rs] & cpu.reg_value[rt];
			break;
		case OP_ANDI:
			cpu.reg_value[rt] = cpu.reg_value[rs] & imm;
			break;
		case OP_BEQ:
			if (cpu.reg_value[rs] == cpu.reg_value[rt]) {
				cpu.pc_mem = find_ins(imm);
				cpu.pc_rb = NULL;
			}
			break;
		case OP_BNE:
			if (cpu.reg_value[rs] != cpu.reg_value[rt]) {
				cpu.pc_mem = find_ins(imm);
				cpu.pc_rb = NULL;
			}
			break;
		case OP_J:
			cpu.pc_mem = find_ins(imm) - 1;
			cpu.pc_rb = NULL;
			break;
		case OP_JAL:
			cpu.reg_value[REG_RA] = cpu.pc_mem - ins_buf;
			cpu.pc_mem = find_ins(cpu.pc_rb->un.i.imm) - 1;
			cpu.pc_rb = NULL;
			break;
		case OP_JR:
			cpu.pc_mem = ins_buf + cpu.reg_value[rs];
			cpu.pc_rb = NULL;
			break;
		case OP_LW:
			cpu.reg_value[rt] = *(int*)&mem[cpu.reg_value[rs] + imm];
			break;
		case OP_NOP:
			break;
		case OP_NOR:
			cpu.reg_value[rd] = ~(cpu.reg_value[rs] | cpu.reg_value[rt]);
			break;
		case OP_ORI:
			cpu.reg_value[rt] = (cpu.reg_value[rs] | imm);
			break;
		case OP_SLT:
			cpu.reg_value[rd] = (cpu.reg_value[rs] < cpu.reg_value[rt]);
			break;
		case OP_SLTI:
			cpu.reg_value[rt] = (cpu.reg_value[rs] < imm);
			break;
		case OP_SLL:
			cpu.reg_value[rt] = (cpu.reg_value[rs] << imm);
			break;
		case OP_SLR:
			cpu.reg_value[rt] = (cpu.reg_value[rs] >> imm);
			break;
		case OP_SUB:
			cpu.reg_value[rd] = cpu.reg_value[rs] - cpu.reg_value[rt];
			break;
		case OP_SW:
			*(int*)&mem[cpu.reg_value[rs] + imm] = cpu.reg_value[rt];
			break;
		default:
			assert(0);
			break;
	}

}

static void init_cpu() {
	bzero(&cpu, sizeof(cpu));
	cpu.pc_mem = ins_buf;
	cpu.pc_rb = NULL;
}

static void init_mem() {
	bzero(mem, MEM_SIZE);
}

static int need_resched() {
	return cpu.pc_rb == NULL || cpu.pc_rb >= cpu.resched_buf + cpu.resched_buf_len;
}

static int has_dep(struct ins *i, struct ins *j) {
	if (i->type == R_TYPE) {
		if (j->type == R_TYPE)
			return j->un.r.rs == i->un.r.rd || j->un.r.rt == i->un.r.rd;
		else if (j->type == I_TYPE)
			return j->un.i.rs == i->un.r.rd;
	}
	else if (i->type == I_TYPE) {
		if (j->type == R_TYPE)
			return j->un.r.rs == i->un.i.rt || j->un.r.rt == i->un.i.rt;
		else if (j->type == I_TYPE)
			return j->un.i.rs == i->un.i.rt;
	}
	return 0;
}

static int is_branch_op(enum opcode op) {
	return op == OP_J || op == OP_JR || op == OP_JAL || op == OP_BNE || op == OP_BEQ;
}

