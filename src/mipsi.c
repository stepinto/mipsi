#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>

#include "mipsi.h"
#include "parse.h"

static void help();
static void die();
static void run();
static void load_and_reschedule(struct ins *buf, int buf_len, struct ins *pc_mem);
static void debug_msg(const char *fmt, ...);
static struct ins *find_ins(int label);
static void print_ins(struct ins *i);

int n_ins;
struct ins ins_buf[INS_BUF_LEN];
int n_symbols;
char symbol[SYMBOLS][SYMBOL_LEN];

static int debug;
static int optimized;
static int profile;
static int resched_buf_len = 1;

extern FILE *yyin;
extern int yydebug;

int main(int argc, char **argv) {
	int opt;

	// yydebug = 1;

	while ((opt = getopt(argc, argv, "dO:ph")) >= 0) {
		switch (opt) {
			case 'd': debug = 1; break;
			case 'O': optimized = 1; resched_buf_len = atoi(optarg); break;
			case 'p': profile = 1; break;
			case 'h': help(); return 0;
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

	debug_msg("Parsing assembly code.\n");
	if (yyparse() > 0) { printf("Syntax error\n"); exit(1); }
	debug_msg("%d instruction(s) has been loaded.\n", n_ins);

	debug_msg("Start exectuion. Reschedule-buffer: %d.\n", resched_buf_len);
	run();

	return 0;
}

static void help() {
	printf("Usage: mipsi [OPTION] FILE\n");
	printf("  -d    debug\n");
	printf("  -O    optimize for performance\n");
	printf("  -p    enable profiling\n");
	printf("  -h    show this help\n");
	printf("\n");
}

static void run() {
	int rval[REGS] = {0};  // save reg values
	int n_data_hazards = 0;
	int n_cycles = 0;
	struct ins *resched_buf;
	struct ins *pc_mem, *pc_rb;
	char *mem;
	int rs, rt, rd, imm;
	int i;

	resched_buf = malloc(sizeof(struct ins) * resched_buf_len);
	mem = malloc(MEM_SIZE);
	bzero(mem, MEM_SIZE);
	pc_mem = ins_buf;
	pc_rb = resched_buf + resched_buf_len;
	while (pc_mem < ins_buf + n_ins) {
		if (pc_rb == resched_buf + resched_buf_len) {
			load_and_reschedule(resched_buf, resched_buf_len, pc_mem);
			pc_rb = resched_buf;
		}
		
		// assert(memcmp(pc_rb, pc_mem, sizeof(struct ins)) == 0);  // invarant

		switch (pc_rb->type) {
			case R_TYPE:
				rs = pc_rb->un.r.rs;
				rt = pc_rb->un.r.rt;
				rd = pc_rb->un.r.rd;
				break;
			case I_TYPE:
				rs = pc_rb->un.i.rs;
				rt = pc_rb->un.i.rt;
				imm = pc_rb->un.i.imm;
				break;
			case J_TYPE:
				imm = pc_rb->un.j.imm;
				break;
			default:
				assert(0);
				break;
		}
				
		switch (pc_rb->op) {
			case OP_ADD:
				assert(pc_rb->type == R_TYPE);
				rval[rd] = rval[rs] + rval[rt];
				break;
			case OP_ADDI:
				assert(pc_rb->type == I_TYPE);
				rval[rt] = rval[rs] + imm;
				break;
			case OP_AND:
				assert(pc_rb->type == R_TYPE);
				rval[rd] = rval[rs] & rval[rt];
				break;
			case OP_ANDI:
				assert(pc_rb->type == I_TYPE);
				rval[rt] = rval[rs] & imm;
				break;
			case OP_BEQ:
				assert(pc_rb->type == I_TYPE);
				if (rval[rs] == rval[rt]) {
					pc_mem = find_ins(imm);
					pc_rb = resched_buf + resched_buf_len;
				}
				break;
			case OP_BNE:
				assert(pc_rb->type == I_TYPE);
				if (rval[rs] != rval[rt]) {
					pc_mem = find_ins(imm);
					pc_rb = resched_buf + resched_buf_len;
				}
				break;
			case OP_J:
				assert(pc_rb->type == J_TYPE);
				pc_mem = find_ins(imm) - 1;
				pc_rb = resched_buf + resched_buf_len;
				break;
			case OP_JAL:
				assert(pc_rb->type == J_TYPE);
				rval[REG_RA] = pc_mem - ins_buf;
				pc_mem = find_ins(pc_rb->un.i.imm) - 1;
				pc_rb = resched_buf + resched_buf_len;
				break;
			case OP_JR:
				assert(pc_rb->type == R_TYPE);
				pc_mem = ins_buf + rval[rs];
				pc_rb = resched_buf + resched_buf_len;
				break;
			case OP_LW:
				assert(pc_rb->type == I_TYPE);
				rval[rt] = *(int*)&mem[rval[rs] + imm];
				break;
			case OP_NOP:
				assert(pc_rb->type == R_TYPE);
				break;
			case OP_NOR:
				assert(pc_rb->type == R_TYPE);
				rval[rd] = ~(rval[rs] | rval[rt]);
				break;
			case OP_ORI:
				assert(pc_rb->type == I_TYPE);
				rval[rt] = (rval[rs] | imm);
				break;
			case OP_SLT:
				assert(pc_rb->type == R_TYPE);
				rval[rd] = (rval[rs] < rval[rt]);
				break;
			case OP_SLTI:
				assert(pc_rb->type == I_TYPE);
				rval[rt] = (rval[rs] < imm);
				break;
			case OP_SLL:
				assert(pc_rb->type == I_TYPE);
				rval[rt] = (rval[rs] << imm);
				break;
			case OP_SLR:
				assert(pc_rb->type == I_TYPE);
				rval[rt] = (rval[rs] >> imm);
				break;
			case OP_SUB:
				assert(pc_rb->type == R_TYPE);
				rval[rd] = rval[rs] - rval[rt];
				break;
			case OP_SW:
				assert(pc_rb->type == I_TYPE);
				*(int*)&mem[rval[rs] + imm] = rval[rt];
				break;
			default:
				assert(0);
				break;
		}

		// debug output
		if (debug) {
			printf("%d: ", pc_mem - ins_buf);
			print_ins(pc_rb);
			for (i = 0; i < 4; i++)
				printf("%ss%d=%d", i==0?"\t# ":", ", i, rval[REG_S0 + i]);
			for (i = 0; i < 2; i++)
				printf(", t%d=%d", i, rval[REG_T0 + i]);
			printf("\n");
		}

		pc_mem++;
		pc_rb++;
	}

	free(resched_buf);
	free(mem);

	if (profile)
		printf("Cycles: %d, data hazards: %d\n", n_cycles, n_data_hazards);
}

static void load_and_reschedule(struct ins *buf, int buf_len, struct ins *pc) {
	memcpy(buf, pc, buf_len * sizeof(struct ins));
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

	printf("%s\t", opcode_name(i->op));
	switch (i->type) {
		case R_TYPE:
			printf("%s, %s, %s", reg_name(i->un.r.rd),
					reg_name(i->un.r.rs),
					reg_name(i->un.r.rt));
			break;
		case I_TYPE:
			printf("%s, %s, %d", reg_name(i->un.i.rt),
					reg_name(i->un.i.rs), i->un.i.imm);
			break;
		case J_TYPE:
			printf("%d", i->un.j.imm);
			break;
	}
}

char *reg_name(enum reg r) {
	switch (r) {
		case REG_ZERO: return "$zero"; 
		case REG_AT: return "$at";
		case REG_V0: return "$v0";
		case REG_V1: return "$v1";
		case REG_A0: return "$a0";
		case REG_A1: return "$a1";
		case REG_A2: return "$a2";
		case REG_A3: return "$a3";
		case REG_T0: return "$t0";
		case REG_T1: return "$t1";
		case REG_T2: return "$t2";
		case REG_T3: return "$t3";
		case REG_T4: return "$t4";
		case REG_T5: return "$t5";
		case REG_T6: return "$t6";
		case REG_T7: return "$t7";
		case REG_T8: return "$t8";
		case REG_T9: return "$t9";
		case REG_S0: return "$s0";
		case REG_S1: return "$s1";
		case REG_S2: return "$s2";
		case REG_S3: return "$s3";
		case REG_S4: return "$s4";
		case REG_S5: return "$s5";
		case REG_S6: return "$s6";
		case REG_S7: return "$s7";
		case REG_S8: return "$s8";
		case REG_GP: return "$gp";
		case REG_SP: return "$sp";
		case REG_RA: return "$ra";
		default: assert(0); return NULL;
	}
}

char *opcode_name(enum opcode op) {
	switch (op) {
		case OP_ADD: return "add"; 
		case OP_ADDI: return "addi"; 
		case OP_AND: return "and"; 
		case OP_ANDI: return "andi"; 
		case OP_BEQ: return "beq"; 
		case OP_BNE: return "bne"; 
		case OP_J: return "j"; 
		case OP_JAL: return "jal"; 
		case OP_JR: return "jr"; 
		case OP_LW: return "lw"; 
		case OP_NOP: return "nop"; 
		case OP_NOR: return "nor"; 
		case OP_ORI: return "ori"; 
		case OP_SLT: return "slt"; 
		case OP_SLTI: return "slti"; 
		case OP_SLL: return "sll"; 
		case OP_SLR: return "slr"; 
		case OP_SUB: return "sub"; 
		case OP_SW: return "sw";
		default: assert(0); return NULL;
	}
}

int lookup(const char *s) {
	int i;
	for (i = 0; i < n_symbols; i++)
		if (strcmp(s, symbol[i]) == 0)
			return i+1;  // never return 0
	strcpy(symbol[n_symbols], s);
	n_symbols++;
	return n_symbols;
}

enum reg make_reg(const char *s) {
	int i;

	for (i = 0; i < REG_LAST; i++)
		if (strcmp(reg_name(i), s) == 0)
			return (enum reg) i;
	assert(0);
	return -1;
}

enum opcode make_opcode(const char *s) {
	int i;

	for (i = 0; i < OP_LAST; i++)
		if (strcmp(opcode_name(i), s) == 0)
			return (enum opcode) i;
	assert(0);
	return -1;
}
