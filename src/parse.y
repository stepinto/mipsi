%{
#include "mipsi.h"
#define YYDEBUG 1

static struct ins *curr = ins_buf;
%}

%union {
	int id;
	int imm;
	enum opcode _opcode;
	enum reg _reg;
};

%token<id> ID
%token<imm> IMM
%token<_opcode> OPCODE
%token<_reg> REG

%start program
%debug
%%

program
	: inst program {}
	| /* empty */ {}
	;

inst
	: label_opt inst_body { n_ins++; curr++; }
	;

label_opt
	: ID ':' { curr->label = $1; }
	| /* empty */ { curr->label = 0; }
	;

inst_body
	: OPCODE REG ',' REG ',' REG {
		if (get_ins_type($1) != R_TYPE) {
			yyerror("Expect r-type opcode.");
			YYERROR;
		}
		curr->type = R_TYPE;
		curr->op = $1;
		curr->un.r.rd = $2;
		curr->un.r.rs = $4;
		curr->un.r.rt = $6;
	}
	| OPCODE REG ',' REG ',' IMM {
		if (get_ins_type($1) != I_TYPE) {
			yyerror("Expect i-type opcode.");
			YYERROR;
		}
		curr->type = I_TYPE;
		curr->op = $1;
		curr->un.i.rt = $2;
		curr->un.i.rs = $4;
		curr->un.i.imm = $6;
	}
	| OPCODE REG ',' REG ',' ID {
		if (get_ins_type($1) != I_TYPE) {
			yyerror("Expect i-type opcode.");
			YYERROR;
		}
		curr->type = I_TYPE;
		curr->op = $1;
		curr->un.i.rt = $2;
		curr->un.i.rs = $4;
		curr->un.i.imm = $6;
	}
	| OPCODE REG ',' IMM '(' REG ')' {
		if ($1 != OP_LW && $1 != OP_SW) {
			yyerror("Expect lw or sw.");
			YYERROR;
		}
		curr->type = I_TYPE;
		curr->op = $1;
		curr->un.i.rt = $2;
		curr->un.i.rs = $6;
		curr->un.i.imm = $4;
	}
	| OPCODE REG {
		if ($1 != OP_JR) {
			yyerror("Expect jr.");
			YYERROR;
		}
		curr->type = R_TYPE;
		curr->op = $1;
		curr->un.r.rs = OP_JR;
	}
	| OPCODE ID {
		if ($1 != OP_JAL && $1 != OP_J) {
			yyerror("Expect jal or j.");
			YYERROR;
		}
		curr->type = J_TYPE;
		curr->op = $1;
		curr->un.j.imm = $2;
	}
	| OPCODE {
		if ($1 != OP_NOP) {
			yyerror("Expect nop.");
			YYERROR;
		}
		curr->type = R_TYPE;
		curr->op = OP_NOP;
	}
	;

