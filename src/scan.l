%{
#include "mipsi.h"
#include "parse.h"

static void save_id();
static void save_imm();
static void save_opcode();
static void save_reg();

%}

%option   yylineno

D			[0-9]
L			[a-zA-Z_]

%%
#.*$			{ /* comment */ }

":"			{ return ':'; }
","			{ return ','; }
"("			{ return '('; }
")"			{ return ')'; }

"add"			{ save_opcode(); return OPCODE; }
"addi"			{ save_opcode(); return OPCODE; }
"and"			{ save_opcode(); return OPCODE; }
"andi"			{ save_opcode(); return OPCODE; }
"beq"			{ save_opcode(); return OPCODE; }
"bne"			{ save_opcode(); return OPCODE; }
"j"			{ save_opcode(); return OPCODE; }
"jal"			{ save_opcode(); return OPCODE; }
"jr"			{ save_opcode(); return OPCODE; }
"lw"			{ save_opcode(); return OPCODE; }
"nor"			{ save_opcode(); return OPCODE; }
"nop"			{ save_opcode(); return OPCODE; }
"ori"			{ save_opcode(); return OPCODE; }
"slt"			{ save_opcode(); return OPCODE; }
"slti"			{ save_opcode(); return OPCODE; }
"sll"			{ save_opcode(); return OPCODE; }
"slr"			{ save_opcode(); return OPCODE; }
"sub"			{ save_opcode(); return OPCODE; }
"sw"			{ save_opcode(); return OPCODE; }

"\$zero"		{ save_reg(); return REG; }
"\$at"			{ save_reg(); return REG; }

\$v[01]		{ save_reg(); return REG; }
\$a[0-3]		{ save_reg(); return REG; }
\$t[0-9]		{ save_reg(); return REG; }
\$s[0-8]		{ save_reg(); return REG; }
\$gp			{ save_reg(); return REG; }
\$sp			{ save_reg(); return REG; }
\$ra			{ save_reg(); return REG; }

{L}({L}|{D})*		{ save_id(); return ID; }
{D}+			{ save_imm(); return IMM; }
\-{D}+		{ save_imm(); return IMM; }

[ \t\v\n\f]		{}
%%

int yywrap() { return 1; }
int yyerror(const char *s) { printf("%d: %s\n", yylineno, s); return 0; }

static void save_id() { yylval.id = lookup(yytext); }

static void save_imm() { yylval.imm = atoi(yytext); }

static void save_opcode() {
	yylval._opcode = make_opcode(yytext);
	if (yylval._opcode < 0) YYABORT();
}

static void save_reg() {
	yylval._reg = make_reg(yytext);
	if (yylval._reg < 0) YYABORT();
}

