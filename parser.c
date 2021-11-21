#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "scanner.h"
#include "ast.h"

/**
 * parser.c
 * By: Matthew Yungbluth
 * Class: CSc 453
 * Assignment 2 Milestone 3
 * This program reads in tokens from scanner.c and decides if they follow the correct grammar rules of c
 * Also checks correct syntax if --chk_decl is written as a command line argument when running the program
 * Also creates an AST and prints if --print_ast is written as a command line argument when running the program
 * Requires scanner.c, scanner.h, driver.c, ast-print.c, and ast.h
 */

struct Symbol {
	char* id;
	int type;
	int numArgs;
	int isFunc;
	int fp_offset;
	struct Symbol *next;
};

struct ASTnode {
	NodeType ntype;
	char* name;
	struct Symbol *st_ref;
	int num;
	struct instr *nodeInstr;
	struct ASTnode *child0, *child1, *child2;
};

typedef enum {
  _INTEGER,
  _STRING
} Typename;

typedef enum {
  ASSGOP,	//x := y op z
  ASSGNUMS,	//x := y
  IFRELOP,	//if x relop y goto L. L is a label; relop ∈ {<=, <, >=, >, ==, !=}.
  GOTO,		//goto L. L is a label
  LABEL,	//label L. L is a label; it must be unique in the program
  ENTER,	//enter f. f is (a pointer to the symbol table entry of) a function.
  LEAVE,	//leave f. f is (a pointer to the symbol table entry of) a function.
  PARAM,	//param x
  CALL,		//call f, n. f is (a pointer to the symbol table entry of) a function; n is the number of arguments being passed.
  RETURNTYPE,	//return to the caller.
  RETURNID,	//return x. return to the caller with return value x
  RETRIEVE,	//retrieve x. copy the return value into x.
  FUNCVARIABLE,
  LOCALVAR,
  GLOBALVAR,
  IFRELOPGOTO,
  OPTYPEEQ,
  OPTYPENE,
  OPTYPELE,
  OPTYPELT,
  OPTYPEGE,
  OPTYPEGT
} opType;

struct Instruction {
	opType op;
	struct Symbol *place;
	struct ASTnode *src0;
	struct ASTnode *src1;
	struct ASTnode *src2;
	struct Symbol *dest;
	struct Instruction *next;
};

Token cur_tok;
int parse();
void match(Token expected);
void fn_call();
void func_defn();
struct ASTnode *opt_expr_list();
struct ASTnode *opt_formals();
struct ASTnode *opt_stmt_list();
void opt_var_decls();
void prog();
struct ASTnode *stmt();
void type();
void var_decl();
struct ASTnode *formals();
void id_list();
void decl_or_func();
int get_token();
int get_line_num();
void push(struct Symbol* scopeHead, char* nodeID, int nodeType);
int prevDeclared(struct Symbol* scopeHead, char* nodeID);
void idCheck();
void printLL();
struct ASTnode *if_stmt();
struct ASTnode *while_stmt();
struct ASTnode *return_stmt();
struct ASTnode *expr_list();
struct ASTnode *bool_exp();
struct ASTnode *arith_exp();
NodeType relop();
struct ASTnode *fn_call_or_assg_stmt();
void set_num_args(char* lexeme);
int get_num_args(char* lexeme);
void check_correct_type(int expected, char* thisLexeme);
void set_func(char* lexeme, int isFuncBool, int globalLocal);
int check_declared_var(char* thisLexeme, int thisScope);
void check_var_use_declared_func(char* thisLexeme);
struct Symbol *newtemp(Typename t);
struct Instruction *newlabel();
struct Instruction *newinstr(opType op, struct Symbol *place, struct Symbol *src1, struct Symbol *src2, struct Symbol *symDest, struct Instruction *instrDest);
void print_assem_func_intro(int numArgs);
void print_assem_func_outro();
void create_three_address_code(struct ASTnode *astHead);
void print_assem_code();
void free_temp(int x);
struct Symbol *get_symbol_from_id(char *id, struct Symbol *scopeHead);
void add_end_instructionLL(struct Instruction *thisInstr);
int get_cur_offset(char *id);
int print_ast_flag;
int chk_decl_flag;
int gen_code_flag;
char* lexeme;
int scope;
int curArgs;
int tempsAvail[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
char tempsUse[10][5] = {"t0", "t1", "t2", "t3", "t4", "t5" ,"t6", "t7", "t8", "t9"};
int currentGlobalOffset = 1;
int firstFuncAssem = 0;
int inFuncAssm = 0;

//0 scope is global, 1 scope is local

struct Symbol* globalHead = NULL;
struct Symbol* localHead = NULL;
struct Instruction* instructionLL = NULL;
struct Symbol* currentLocalAssm = NULL;
//only need global and local symbol tables currently, said in lecture somewhere

//basically main, gets token and calls prog() to start parsing
int parse() {
	curArgs = 0;
	cur_tok = get_token();
	if (gen_code_flag == 1) {
		char* printchar = "println";
		globalHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		globalHead->id = printchar;
		globalHead->isFunc = 1;
		globalHead->type = FUNC_DEF;
		globalHead->numArgs = 1;
	}
	prog();
	if (gen_code_flag == 1) {
		printf(".align 2\n.data\n_nl: .asciiz \"\\n\"\n\n.align 2\n");
		print_assem_code();
	}

	return 0;
}

int get_cur_offset(char *id) {
	struct Symbol* thisSymbol = (struct Symbol*)malloc(sizeof(struct Symbol));
	if (get_symbol_from_id(id, currentLocalAssm) == NULL) {
		//global
		thisSymbol = get_symbol_from_id(id, globalHead);
	} else {
		//local
		thisSymbol = get_symbol_from_id(id, currentLocalAssm);
	}
	return thisSymbol->fp_offset;
}

int create_new_cur_offset(char *id) {
	struct Symbol* thisSymbol = (struct Symbol*)malloc(sizeof(struct Symbol));
	if (get_symbol_from_id(id, currentLocalAssm) == NULL) {
		//global
		thisSymbol = get_symbol_from_id(id, globalHead);
	} else {
		//local
		thisSymbol = get_symbol_from_id(id, currentLocalAssm);
	}
	thisSymbol->fp_offset = -(currentGlobalOffset*4);
	currentGlobalOffset++;
	return thisSymbol->fp_offset;
}

void print_assem_func_intro(int numArgs) {
	numArgs++;
	numArgs = numArgs * 4;
	printf("\nla\t$sp,\t-48($sp)\nsw\t$fp,\t44($sp)\nsw\t$ra,\t0($sp)\nla\t$fp,\t0($sp)\nla\t$sp,\t-44($sp)\n");
	if (currentLocalAssm != NULL) {
		currentLocalAssm->fp_offset = -((currentGlobalOffset-1)*4);
	}
}

void print_assem_func_outro() {
	printf("\nla\t$sp,\t0($fp)\nlw\t$ra,\t0($sp)\nlw\t$fp,\t44($sp)\nla\t$sp,\t48($sp)\njr\t$ra\n");
}

void add_end_instructionLL(struct Instruction *thisInstr) {
	if (instructionLL == NULL) {
		instructionLL = thisInstr;
		return;
	}
	struct Instruction *newInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
	newInstr->next = instructionLL->next;
	int i = 0;
	while (newInstr->next != NULL) {
		newInstr = newInstr->next;
		i++;
	}
	struct Instruction *newTestInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
	newTestInstr->dest = thisInstr->dest;
	newTestInstr->next = thisInstr->next;
	newTestInstr->op = thisInstr->op;
	newTestInstr->place = thisInstr->place;
	newTestInstr->src0 = thisInstr->src0;
	newTestInstr->src1 = thisInstr->src1;
	newTestInstr->src2 = thisInstr->src2;
	if (newInstr->next == instructionLL->next) {
		instructionLL->next = newTestInstr;
	} else {
		newInstr->next = newTestInstr;
	}
}

void create_three_address_code(struct ASTnode *astHead) {
	if (astHead == NULL) {
		return;
	}
	switch(astHead->ntype) {
		struct Instruction *thisInstr;
		case FUNC_DEF:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = ENTER;
			thisInstr->place = get_symbol_from_id(astHead->name, globalHead);
			thisInstr->src1 = astHead;
			currentLocalAssm = astHead->st_ref;
			add_end_instructionLL(thisInstr);
			create_three_address_code(astHead->child0);
			struct Instruction *nextInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			nextInstr->op = LEAVE;
			nextInstr->place = get_symbol_from_id(astHead->name, globalHead);
			add_end_instructionLL(nextInstr);
			break;
		case FUNC_CALL:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = CALL;
			thisInstr->dest = get_symbol_from_id(astHead->name, globalHead);
			thisInstr->src1 = astHead->child0;
			add_end_instructionLL(thisInstr);
			break;
		case ASSG:
			create_three_address_code(astHead->child0);
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = ASSGNUMS;
			//child0 is LHS
			struct Symbol *thisSym = get_symbol_from_id(astHead->child0->name, localHead);
			if (thisSym == NULL) {
				thisSym = get_symbol_from_id(astHead->child0->name, globalHead);
			}
			if (thisSym == NULL) {
				printf("ERROR SYMBOL NOT FOUND");
				exit(1);
			}
			thisInstr->dest = thisSym;
			thisInstr->place = thisSym;
			//set RHS
			thisInstr->src1 = astHead->child1;
			add_end_instructionLL(thisInstr);
			break;
		case STMT_LIST:
			create_three_address_code(astHead->child0);
			create_three_address_code(astHead->child1);
			break;
		case IDENTIFIER:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			if (prevDeclared(globalHead, astHead->name) == 1) {
				thisInstr->op = GLOBALVAR;
			} else if (astHead->st_ref == NULL) {
				thisInstr->op = LOCALVAR;
			} else {
				thisInstr->op = FUNCVARIABLE;
			}
			thisInstr->src1 = astHead;
			create_new_cur_offset(astHead->name);
			add_end_instructionLL(thisInstr);
			break;
		case INTCONST:
			break;
		case EXPR_LIST:
			break;
		case IF:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = IFRELOPGOTO;
			thisInstr->src0 = astHead->child0;
			thisInstr->src1 = astHead->child1;
			thisInstr->src2 = astHead->child2;
			add_end_instructionLL(thisInstr);
			break;
		case EQ:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = OPTYPEEQ;
			thisInstr->src1 = astHead->child0;
			thisInstr->src2 = astHead->child1;
			add_end_instructionLL(thisInstr);
			break;
		case NE:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = OPTYPENE;
			thisInstr->src1 = astHead->child0;
			thisInstr->src2 = astHead->child1;
			add_end_instructionLL(thisInstr);
			break;
		case LE:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = OPTYPELE;
			thisInstr->src1 = astHead->child0;
			thisInstr->src2 = astHead->child1;
			add_end_instructionLL(thisInstr);
			break;
		case LT:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = OPTYPELT;
			thisInstr->src1 = astHead->child0;
			thisInstr->src2 = astHead->child1;
			add_end_instructionLL(thisInstr);
			break;
		case GE:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = OPTYPEGE;
			thisInstr->src1 = astHead->child0;
			thisInstr->src2 = astHead->child1;
			add_end_instructionLL(thisInstr);
			break;
		case GT:
			thisInstr = (struct Instruction*)malloc(sizeof(struct Instruction));
			thisInstr->op = OPTYPEGT;
			thisInstr->src1 = astHead->child0;
			thisInstr->src2 = astHead->child1;
			add_end_instructionLL(thisInstr);
			break;
		default:
			return;
	}
}

void print_assem_code() {
	struct Instruction *loopLL = (struct Instruction*)malloc(sizeof(struct Instruction));
	loopLL->dest = instructionLL->dest;
	loopLL->next = instructionLL->next;
	loopLL->op = instructionLL->op;
	loopLL->place = instructionLL->place;
	loopLL->src1 = instructionLL->src1;
	loopLL->src2 = instructionLL->src2;
	while (loopLL != NULL) {
		switch(loopLL->op) {
			case ENTER:
				;
				inFuncAssm = 1;
				if (firstFuncAssem == 0) {
					printf(".text\nprintln:\n\tli $v0, 1\n\tlw $a0, 0($sp)\n\tsyscall\n\tli $v0, 4\n\tla $a0, _nl\n\tsyscall\n\tjr $ra\n");
					firstFuncAssem = 1;
				}
				printf("\n%s:\n", loopLL->place->id);
				currentLocalAssm = loopLL->src1->st_ref;
				print_assem_func_intro(loopLL->place->numArgs);
				break;
			case LEAVE:
				;
				print_assem_func_outro();
				break;
			case ASSGNUMS:
				;
				//first get LHS, first find if local
				if (prevDeclared(currentLocalAssm, loopLL->place->id) == 1) {
					//local
					//get RHS
					int thisRHS;
					if (loopLL->src1->ntype == INTCONST) {
						thisRHS = loopLL->src1->num;
						//get offset
						int curOffset = get_cur_offset(loopLL->place->id);
						//use the offset
						printf("\naddu\t$t0,\t$zero,\t%d", thisRHS);
						printf("\nsw\t$t0,\t%d($fp)\n", curOffset);
					} else {
						int i = 0;
						//RHS is an identifier find if its a formal, a local, or a global
						if ((prevDeclared(currentLocalAssm, loopLL->src1->name) == 1) && (get_cur_offset(loopLL->src1->name) != 0)) {
							//local
							//how to store local variables?
							//store negative fp
							int curOffset = get_cur_offset(loopLL->src1->name);
							printf("\nlw $t0, %d($fp)", curOffset);
							printf("\nla $sp, -4($sp)");
							printf("\nsw $t0, 0($sp)\n");
						} else {
							//its a global variable
							if (prevDeclared(globalHead, loopLL->src1->name) == 1) {
								//global
								printf("\nla $t0, %s", loopLL->src1->name);
								printf("\nlw $t1, 0($t0)");
								printf("\nla $sp, -4($sp)");
								printf("\nsw $t1, 0($sp)\n");
							} else {
								//formal of the function, value passed in from another function
								i++;
								printf("\nlw $t0, %d($fp)", (44 + (i*4)));
								printf("\nla $sp, -4($sp)");
								printf("\nsw $t0, 0($sp)\n");
							}
						}
					}
				} else {
					//global
					int thisRHS;
					if (loopLL->src1->ntype == INTCONST) {
						thisRHS = loopLL->src1->num;
					}
					if (prevDeclared(globalHead, loopLL->place->id) == 1) {
						//confirmed global
						printf("\nla\t$t1,\t%s", loopLL->place->id);
						printf("\naddu\t$t0,\t$zero,\t%d", thisRHS);
						printf("\nsw\t$t0,\t0($t1)\n");
					} else {
						//error, not defined?
						printf("\n%s\n", loopLL->place->id);
						printf("\nERROR ASSGNUMS NOT DEFINED LHS\n");
						exit(1);
					}
				}
				break;
			case CALL:
				;
				if (loopLL != NULL) {
					struct ASTnode *thisArgs;
					thisArgs = (struct ASTnode*)malloc(sizeof(struct ASTnode));
					thisArgs = loopLL->src1;
					while (thisArgs != NULL) {
						int i = 0;
						if (thisArgs->ntype == INTCONST) {
							printf("\nli $t0, %d\n", thisArgs->num);
							printf("\nla $sp, -4($sp)\n");
							printf("\nsw $t0, 0($sp)\n");
						} else if (thisArgs->ntype == IDENTIFIER) {
							//check if it's a local variable
							if ((prevDeclared(currentLocalAssm, thisArgs->name) == 1) && (get_cur_offset(thisArgs->name) != 0)) {
								//local
								//how to store local variables?
								//store negative fp
								int curOffset = get_cur_offset(thisArgs->name);
								printf("\nlw $t0, %d($fp)", curOffset);
								printf("\nla $sp, -4($sp)");
								printf("\nsw $t0, 0($sp)\n");
							} else {
								//its a global variable
								if (prevDeclared(globalHead, thisArgs->name) == 1) {
									//global
									printf("\nla $t0, %s", thisArgs->name);
									printf("\nlw $t1, 0($t0)");
									printf("\nla $sp, -4($sp)");
									printf("\nsw $t1, 0($sp)\n");
								} else {
									//formal of the function, value passed in from another function
									i++;
									printf("\nlw $t0, %d($fp)", (44 + (i*4)));
									printf("\nla $sp, -4($sp)");
									printf("\nsw $t0, 0($sp)\n");
								}
							}
						}
						thisArgs = thisArgs->child0;
					}
					printf("\njal %s\n", loopLL->dest->id);
				}
				break;
			case GLOBALVAR:
				if (inFuncAssm == 0) {
					printf("\n%s :\t.space\t4\n", loopLL->src1->name);
				}
				break;
			case IFRELOPGOTO:
				break;
			case OPTYPEEQ:
				break;
			case OPTYPENE:
				break;
			case OPTYPELE:
				break;
			case OPTYPELT:
				break;
			case OPTYPEGE:
				break;
			case OPTYPEGT:
				break;
			default:
				break;
		}
		loopLL = loopLL->next;
	}
}

void free_temp(int x) {
	tempsAvail[x] = 0;
}

struct Symbol *get_symbol_from_id(char *id, struct Symbol *scopeHead) {
	struct Symbol* curHead = NULL;
	if (scopeHead != NULL) {
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = scopeHead->id;
		curHead->type = scopeHead->type;
		curHead->numArgs = scopeHead->numArgs;
		curHead->isFunc = scopeHead->isFunc;
		curHead->next = scopeHead->next;
		curHead->fp_offset = scopeHead->fp_offset;
	}
	while (curHead != NULL) {
		if (strcmp(curHead->id, id) == 0) {
			return curHead;
		}
		curHead = curHead->next;
	}
	//shouldn't get here
	return curHead;
}

struct Symbol *newtemp(Typename t) {
	struct Symbol *newSymbol = (struct Symbol*)malloc(sizeof(struct Symbol));
	int i = 0;
	for (i = 0; i < 10; i++) {
		if (tempsAvail[i] == 0) {
			tempsAvail[i] = 1;
			break;
		}
	}
	newSymbol->id = tempsUse[i];
	newSymbol->type = t;
	newSymbol->isFunc = 0;
	struct Symbol *curHead = NULL;
	if (localHead != NULL) {
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = localHead->id;
		curHead->type = localHead->type;
		curHead->numArgs = localHead->numArgs;
		curHead->isFunc = localHead->isFunc;
		curHead->next = localHead->next;
		curHead->fp_offset = localHead->fp_offset;
	}
	newSymbol->next = curHead;
	localHead = newSymbol;
	return newSymbol;
}

//struct Instruction *newlabel() {
	//return newinstr(LABEL, label_num)
//}

//0 scope = global, 1 = local
//returns 0 for var, 1 for func
int check_declared_var(char* thisLexeme, int thisScope) {
	struct Symbol* curHead = NULL;
	if (thisScope == 0) {
		if (globalHead != NULL) {
			curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
			curHead->id = globalHead->id;
			curHead->type = globalHead->type;
			curHead->numArgs = globalHead->numArgs;
			curHead->isFunc = globalHead->isFunc;
			curHead->next = globalHead->next;
			curHead->fp_offset = globalHead->fp_offset;
		}
	}
	if (thisScope == 1) {
		if (localHead != NULL) {
			curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
			curHead->id = localHead->id;
			curHead->type = localHead->type;
			curHead->numArgs = localHead->numArgs;
			curHead->isFunc = localHead->isFunc;
			curHead->next = localHead->next;
			curHead->fp_offset = localHead->fp_offset;
		}
	}
	while (curHead != NULL) {
		if (strcmp(curHead->id, thisLexeme) == 0) {
			if (curHead->isFunc == 0) {
				//0 for var
				return 0;
			} else {
				//1 for func
				return 1;
			}
		}
		curHead = curHead->next;
	}
	return -1;
}

void set_func(char* lexeme, int isFuncBool, int globalLocal) {
	struct Symbol* curHead = NULL;
	if (globalLocal == 0 && globalHead != NULL) {
		//global
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = globalHead->id;
		curHead->type = globalHead->type;
		curHead->numArgs = globalHead->numArgs;
		curHead->isFunc = globalHead->isFunc;
		curHead->next = globalHead->next;
		curHead->fp_offset = globalHead->fp_offset;
	} else if (globalLocal == 1 && localHead != NULL) {
		//local
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = localHead->id;
		curHead->type = localHead->type;
		curHead->numArgs = localHead->numArgs;
		curHead->isFunc = localHead->isFunc;
		curHead->next = localHead->next;
		curHead->fp_offset = localHead->fp_offset;
	}
	struct Symbol* thisHead = NULL;
	if (globalLocal == 0) {
		thisHead = globalHead;
	} else if (globalLocal == 1) {
		thisHead = localHead;
	}
	int i = 0;
	while (thisHead != NULL) {
		if (strcmp(thisHead->id, lexeme) == 0) {
			thisHead->isFunc = isFuncBool;
			break;
		}
		thisHead = thisHead->next;
		i++;
	}
	if (i > 0) {
		if (globalLocal == 0) {
			globalHead = curHead;
		} else if (globalLocal == 1) {
			localHead = curHead;
		}
	}
}

void set_num_args(char* lexeme) {
	struct Symbol* curHead = NULL;
	if (globalHead != NULL) {
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = globalHead->id;
		curHead->type = globalHead->type;
		curHead->numArgs = globalHead->numArgs;
		curHead->isFunc = globalHead->isFunc;
		curHead->next = globalHead->next;
		curHead->fp_offset = globalHead->fp_offset;
	}
	int i = 0;
	while (globalHead != NULL) {
		if (strcmp(globalHead->id, lexeme) == 0) {
			globalHead->numArgs = curArgs;
			break;
		}
		globalHead = globalHead->next;
		i++;
	}
	if (i > 0) {
		globalHead = curHead;
	}
}

int get_num_args(char* lexeme) {
	struct Symbol* curHead = NULL;
	if (globalHead != NULL) {
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = globalHead->id;
		curHead->type = globalHead->type;
		curHead->numArgs = globalHead->numArgs;
		curHead->isFunc = globalHead->isFunc;
		curHead->next = globalHead->next;
		curHead->fp_offset = globalHead->fp_offset;
	} else {
		return 0;
	}
	while (curHead != NULL) {
		if (strcmp(curHead->id, lexeme) == 0) {
			return curHead->numArgs;
		}
		curHead = curHead->next;
	}
	return 0;
}

//1 for func, 0 for var
void check_correct_type(int expected, char* thisLexeme) {
	struct Symbol* curHead = NULL;
	if (scope == 0 && globalHead != NULL) {
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = globalHead->id;
		curHead->type = globalHead->type;
		curHead->numArgs = globalHead->numArgs;
		curHead->isFunc = globalHead->isFunc;
		curHead->next = globalHead->next;
		curHead->fp_offset = globalHead->fp_offset;
	}
	if (scope == 1 && localHead != NULL) {
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = localHead->id;
		curHead->type = localHead->type;
		curHead->numArgs = localHead->numArgs;
		curHead->isFunc = localHead->isFunc;
		curHead->next = localHead->next;
		curHead->fp_offset = localHead->fp_offset;
	}
	while (curHead != NULL) {
		if (curHead->id == lexeme) {
			if (curHead->isFunc == expected) {
				return;
			} else {
				//wrong type, exit with error
				int line_num = get_line_num();
				printf("ERROR WRONG TYPE VAR ON LINE %d \n", line_num);
				exit(1);
			}
		}
		curHead = curHead->next;
	}
}

//add the new id to the symbol table and make it the new head of the linked list
void push(struct Symbol* scopeHead, char* nodeID, int nodeType) {
	struct Symbol* newSymbol = (struct Symbol*)malloc(sizeof(struct Symbol));
	newSymbol->id = nodeID;
	newSymbol->type = nodeType;
	if (scope == 0) {
		newSymbol->numArgs = curArgs;
	} else {
		newSymbol->numArgs = 0;
	}
	newSymbol->next = scopeHead;
	if (scope == 0) {
		globalHead = newSymbol;
	}
	if (scope == 1) {
		localHead = newSymbol;
	}
}

//checks if the id passed is in the symbol table passed
//1 for yes, 0 for no
int prevDeclared(struct Symbol* curHead, char* nodeID) {
	while (curHead != NULL) {
		//0 false 1 true
		if (strcmp(curHead->id, nodeID) == 0) {
			return 1;
		}
		curHead = curHead->next;
	}
	return 0;
}

//checks if the current lexeme is already in the current scope's symbol table
//pushes it to the symbol table if it is not, exits with an error message if it is
void idCheck() {
	struct Symbol* head;
	if (scope == 0) {
		head = globalHead;
	}
	if (scope == 1) {
		head = localHead;
	}
	if (chk_decl_flag == 1) {
		struct Symbol* curHead = NULL;
		if (scope == 0) {
			if (globalHead != NULL) {
				curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
				curHead->id = globalHead->id;
				curHead->type = globalHead->type;
				curHead->numArgs = globalHead->numArgs;
				curHead->isFunc = globalHead->isFunc;
				curHead->next = globalHead->next;
				curHead->fp_offset = globalHead->fp_offset;
			}
		}
		if (scope == 1) {
			if (localHead != NULL) {
				curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
				curHead->id = localHead->id;
				curHead->type = localHead->type;
				curHead->numArgs = localHead->numArgs;
				curHead->isFunc = localHead->isFunc;
				curHead->next = localHead->next;
				curHead->fp_offset = localHead->fp_offset;
			}
		}
		if (prevDeclared(curHead, lexeme)) {
			//previously declared, exit with error
			int line_num = get_line_num();
			printf("ERROR VARIABLE PREVIOUSLY DECLARED ON LINE %d \n", line_num);
			exit(1);
		}
		push(head, lexeme, kwINT);
	}
}

//prints the linked lists for the global and local symbol tables
//not actually used in program, just for debugging purposes
void printLL() {
	struct Symbol* curHead = NULL;
	if (globalHead != NULL) {
		curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHead->id = globalHead->id;
		curHead->type = globalHead->type;
		curHead->numArgs = globalHead->numArgs;
		curHead->isFunc = globalHead->isFunc;
		curHead->next = globalHead->next;
		curHead->fp_offset = globalHead->fp_offset;
	}
	printf("Global Vars:\n");
	while (curHead != NULL) {
		printf("id: %s ", curHead->id);
		printf("numArgs: %d", curHead->numArgs);
		printf("isFunc 0 is var, 1 is func: %d\n\n", curHead->isFunc);
		curHead = curHead->next;
	}
	struct Symbol* curHeadTwo = NULL;
	if (localHead != NULL) {
		curHeadTwo = (struct Symbol*)malloc(sizeof(struct Symbol));
		curHeadTwo->id = localHead->id;
		curHeadTwo->type = localHead->type;
		curHeadTwo->numArgs = localHead->numArgs;
		curHeadTwo->isFunc = localHead->isFunc;
		curHeadTwo->next = localHead->next;
		curHead->fp_offset = localHead->fp_offset;
	}
	printf("\n\nLocalVars:\n");
	while(curHeadTwo != NULL) {
		printf("id: %s ", curHeadTwo->id);
		printf("numArgs: %d", curHeadTwo->numArgs);
		printf("isFunc 0 is var, 1 is func: %d\n\n", curHeadTwo->isFunc);
		curHeadTwo = curHeadTwo->next;
	}
	printf("\n");
}

//if the token matches what it should be, continue, else exit with error
void match(Token expected) {
	if (cur_tok == expected) {
		cur_tok = get_token();
	} else {
		int line_num = get_line_num();
		printf("ERROR FOUND ON LINE %d \n", line_num);
		exit(1);
	}
}

struct ASTnode *opt_expr_list() {
	//first: <epsilon>, follow: RPAREN

	//opt_expr_list : /* epsilon */
	struct ASTnode *ast = NULL;
	if (cur_tok == ID || cur_tok == INTCON) {
		//expr_list
		ast = expr_list();
	}
	return ast;

	//WHAT TO DO WHEN FIRST IS EPSILON?!?
}

void var_decl() {
	type();
	id_list();
	match(SEMI);
}

struct ASTnode *opt_formals() {
	//first: <epsilon>, follow: RPAREN

	//opt_formals : /* epsilon */
	struct ASTnode *ast = NULL;
	if (cur_tok == kwINT) {
		ast = formals();
	}
	return ast;
}

struct ASTnode *formals() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	type();
	ast->ntype = IDENTIFIER;
	if (chk_decl_flag == 1) {
		//0 for var
		idCheck();
		set_func(lexeme, 0, 1);
	}
	ast->name = lexeme;
	match(ID);
	curArgs++;
	if (cur_tok == COMMA) {
		match(COMMA);
		ast->child0 = formals();
	}
	return ast;
}

struct ASTnode *opt_stmt_list() {
	//first: <epsilon> id, follow: RBRACE

	//opt_stmt_list : stmt opt_stmt_list
    //| /* epsilon */

	struct ASTnode *ast = NULL;
	if (cur_tok == ID || cur_tok == kwWHILE || cur_tok == kwIF || cur_tok == kwRETURN || cur_tok == LBRACE || cur_tok == SEMI) {
		ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
		ast->ntype = STMT_LIST;
		ast->child0 = stmt();
		ast->child1 = opt_stmt_list();
	}
	return ast;
}

void opt_var_decls() {
	//first: <epsilon>, follow: id RBRACE
	while (cur_tok == kwINT) {
		var_decl();
	}
	return;
	//opt_var_decls : /* epsilon */
}

void prog() {
	scope = 0;
	//first: <epsilon> kwINT, follow: <EOF>

	//prog : func_defn   prog
    // | /* epsilon */

	while (cur_tok == kwINT) {
		//func_defn or var_decl
		//func_defn -> kwINT, ID, LPAREN
		//var_decl -> kwINT, ID, SEMI/COMMA
		//func_defn();
		match(kwINT);
		decl_or_func();
	}
	return;
}

void decl_or_func() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	if (chk_decl_flag == 1) {
		idCheck();
	}
	char* thisLexeme = lexeme;
	match(ID);
	if (cur_tok == LPAREN) {
		//function
		//symtab_insert(...)
		ast->ntype = FUNC_DEF;
		ast->name = thisLexeme;
		if (chk_decl_flag == 1) {
			set_func(thisLexeme, 1, 0);
		}
		match(LPAREN);
		scope = 1;
		curArgs = 0;
		ast->child1 = opt_formals();
		if (chk_decl_flag == 1) {
			set_num_args(thisLexeme);
		}
		ast->num = curArgs;
		curArgs = 0;
		match(RPAREN);
		match(LBRACE);
		opt_var_decls();
		ast->child0 = opt_stmt_list();
		match(RBRACE);
		//reset_symtab(...)
		//but not the one in the ast! create a copy
		struct Symbol* curHead = NULL;
		if (localHead != NULL) {
			curHead = (struct Symbol*)malloc(sizeof(struct Symbol));
			curHead->id = localHead->id;
			curHead->type = localHead->type;
			curHead->numArgs = localHead->numArgs;
			curHead->isFunc = localHead->isFunc;
			curHead->next = localHead->next;
			curHead->fp_offset = localHead->fp_offset;
		}
		ast->st_ref = curHead;
		if (print_ast_flag == 1) {
			//print the AST
			print_ast(ast);
		}
		if (gen_code_flag == 1) {
			//create 3 address
			create_three_address_code(ast);
		}
		localHead = NULL;
		scope = 0;
	} else {
		//var decl
		if (chk_decl_flag == 1) {
			check_var_use_declared_func(thisLexeme);
		}
		if (gen_code_flag == 1) {
			//create 3 address
			ast->ntype = IDENTIFIER;
			ast->name = thisLexeme;
			create_three_address_code(ast);
		}
		while (cur_tok == COMMA) {
			struct ASTnode *astTwo = (struct ASTnode*)malloc(sizeof(struct ASTnode));
			match(COMMA);
			thisLexeme = lexeme;
			if (chk_decl_flag == 1) {
				idCheck();
				set_func(lexeme, 0, scope);
				check_var_use_declared_func(lexeme);
			}
			if (gen_code_flag == 1) {
				//create 3 address
				astTwo->ntype = IDENTIFIER;
				astTwo->name = thisLexeme;
				create_three_address_code(astTwo);
			}
			match(ID);
		}
		match(SEMI);
	}
}

void id_list() {
	if (chk_decl_flag == 1) {
		idCheck();
		set_func(lexeme, 0, scope);
		check_var_use_declared_func(lexeme);
	}
	match(ID);
	while (cur_tok == COMMA) {
		match(COMMA);
		if (chk_decl_flag == 1) {
			idCheck();
			set_func(lexeme, 0, scope);
			check_var_use_declared_func(lexeme);
		}
		match(ID);
	}
	return;
}

struct ASTnode *stmt() {
	//first: id, follow: id RBRACE

	//stmt : fn_call SEMI
	//construct an AST and return it, will be child0
	if (cur_tok == ID) {
		//fn_call or assg_stmt
		//fn_call: ID LPAREN opt_expr_list RPAREN
		//assg_stmt: ID opASSG arith_exp SEMI
		return fn_call_or_assg_stmt();
	} else if (cur_tok == kwWHILE) {
		//while_stmt
		return while_stmt();
	} else if (cur_tok == kwIF) {
		//if_stmt
		return if_stmt();
	} else if (cur_tok == kwRETURN) {
		//return_stmt
		return return_stmt();
	} else if (cur_tok == LBRACE) {
		struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
		match(LBRACE);
		ast = opt_stmt_list();
		match(RBRACE);
		return ast;
	} else {
		match(SEMI);
		return NULL;
	}
}

struct ASTnode *fn_call_or_assg_stmt() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	char* thisLexeme = lexeme;
	if (chk_decl_flag == 1) {
		check_correct_type(1, lexeme);
	}
	match(ID);
	if (cur_tok == LPAREN) {
		//fn_call
		if (chk_decl_flag == 1) {
			if (prevDeclared(localHead, thisLexeme) == 1) {
				//declared locally, check to make sure it isn't a variable
				if (check_declared_var(thisLexeme, 1) == 0) {
					//exit with error
					int line_num = get_line_num();
					printf("ERROR FN_CALL PREVIOUSLY DECLARED AS VAR ON LINE %d \n", line_num);
					exit(1);
				}
			}
			if (prevDeclared(globalHead, thisLexeme) == 1) {
				//declared globally, check to make sure it isn't a variable
				if (check_declared_var(thisLexeme, 0) == 0) {
					//exit with error
					int line_num = get_line_num();
					printf("ERROR FN_CALL PREVIOUSLY DECLARED AS VAR ON LINE %d \n", line_num);
					exit(1);
				}
			} else {
				//function hasn't been declared yet, exit with error
				int line_num = get_line_num();
				printf("ERROR FUNCTION NOT YET DECLARED ON LINE %d \n", line_num);
				exit(1);
			}
		}
		ast->ntype = FUNC_CALL;
		ast->name = thisLexeme;
		match(LPAREN);
		curArgs = 0;
		ast->child0 = opt_expr_list();
		if (chk_decl_flag == 1) {
			if (get_num_args(thisLexeme) != curArgs) {
				int line_num = get_line_num();
				printf("ERROR DIFFERENCE BETWEEN NUM ARGS BETWEEN FUNC DEF AND CALL ON LINE %d \n", line_num);
				exit(1);
			}
		}
		ast->num = curArgs;
		curArgs = 0;
		match(RPAREN);
		match(SEMI);
	} else {
		//assg_stmt
		if (chk_decl_flag == 1) {
			if (prevDeclared(localHead, thisLexeme) == 0) {
				if (prevDeclared(globalHead, thisLexeme) == 0) {
					//hasn't been declared, exit with error
					int line_num = get_line_num();
					printf("ERROR ASSIGNMENT VAR HASNT BEEN DECLARED ON LINE %d \n", line_num);
					exit(1);
				}
			}
			check_var_use_declared_func(thisLexeme);
		}
		ast->ntype = ASSG;
		struct Symbol *sym_of_func = get_symbol_from_id(thisLexeme, localHead);
		if (sym_of_func == NULL) {
			sym_of_func = get_symbol_from_id(thisLexeme, globalHead);
		}
		struct ASTnode *lhs = (struct ASTnode*)malloc(sizeof(struct ASTnode));
		lhs->ntype = IDENTIFIER;
		lhs->name = thisLexeme;
		ast->child0 = lhs;
		match(opASSG);
		ast->child1 = arith_exp();
		match(SEMI);
	}
	return ast;
}

void check_var_use_declared_func(char* thisLexeme) {
	if (prevDeclared(localHead, thisLexeme) == 1) {
		//has been declared locally, check to make sure it hasn't been declared as a func
		if (check_declared_var(thisLexeme, 1) == 1) {
			//exit with error
			int line_num = get_line_num();
			printf("ERROR ASSG HAS PREVIOUSLY BEEN DECLARED AS FUNC ON LINE %d \n", line_num);
			exit(1);
		}
	} else {
		//not declared locally, check global
		if (prevDeclared(globalHead, thisLexeme) == 1) {
			//declared globally, check to make sure it hasn't been declared as a func
			if (check_declared_var(thisLexeme, 0) == 1) {
				//exit with error
				int line_num = get_line_num();
				printf("ERROR ASSG HAS PREVIOUSLY BEEN DECLARED AS FUNC ON LINE %d \n", line_num);
				exit(1);
			}
		} else {
			//not been declared anywhere, return
			return;
		}
	}
}

struct ASTnode *if_stmt() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	ast->ntype = IF;
	match(kwIF);
	match(LPAREN);
	ast->child0 = bool_exp();
	match(RPAREN);
	ast->child1 = stmt();
	if (cur_tok == kwELSE) {
		match(kwELSE);
		ast->child2 = stmt();
	}
	return ast;
}

struct ASTnode *while_stmt() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	ast->ntype = WHILE;
	match(kwWHILE);
	match(LPAREN);
	ast->child0 = bool_exp();
	match(RPAREN);
	ast->child1 = stmt();
	return ast;
}

struct ASTnode *return_stmt() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	ast->ntype = RETURN;
	match(kwRETURN);
	if (cur_tok == SEMI) {
		match(SEMI);
	} else {
		ast->child0 = arith_exp();
		match(SEMI);
	}
	return ast;
}


struct ASTnode *expr_list() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	curArgs++;
	ast->ntype = EXPR_LIST;
	ast->child0 = arith_exp();
	if (cur_tok == COMMA) {
		match(COMMA);
		ast->child1 = expr_list();
	}
	return ast;
}

struct ASTnode *bool_exp() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	ast->child0 = arith_exp();
	ast->ntype = relop();
	ast->child1 = arith_exp();
	return ast;
}

struct ASTnode *arith_exp() {
	struct ASTnode *ast = (struct ASTnode*)malloc(sizeof(struct ASTnode));
	if (cur_tok == ID) {
		if (chk_decl_flag == 1) {
			if (!prevDeclared(localHead, lexeme)) {
				if (!prevDeclared(globalHead, lexeme)) {
					int line_num = get_line_num();
					printf("ERROR VARIABLE NOT PREVIOUSLY DEFINED ON LINE %d \n", line_num);
					exit(1);
				}
			}
		}
		if (chk_decl_flag == 1) {
			check_correct_type(0, lexeme);
			check_var_use_declared_func(lexeme);
		}
		ast->ntype = IDENTIFIER;
		ast->name = lexeme;
		match(ID);
	} else {
		ast->ntype = INTCONST;
		ast->num = atoi(lexeme);
		match(INTCON);
	}
	return ast;
}

NodeType relop() {
	if (cur_tok == opEQ) {
		match(opEQ);
		return EQ;
	} else if (cur_tok == opNE) {
		match(opNE);
		return NE;
	} else if (cur_tok == opLE) {
		match(opLE);
		return LE;
	} else if (cur_tok == opLT) {
		match(opLT);
		return LT;
	} else if (cur_tok == opGE) {
		match(opGE);
		return GE;
	} else {
		match(opGT);
		return GT;
	}
	return DUMMY;
}

void type() {
	//first: kwINT, follow: id

	//type : kwINT

	match(kwINT);
}




//Start of AST functions




NodeType ast_node_type(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->ntype;
}

char *func_def_name(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->name;
}

int func_def_nargs(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->num;
}

void *func_def_body(void *ptr) {
	struct ASTnode *ast = ptr;
	//returns a pointer to the AST of the body of the function.
	return ast->child0;
}

char *func_def_argname(void *ptr, int n) {
	struct ASTnode *ast = ptr;
	//get to first formal
	ast = ast->child1;
	//returns a pointer to the name (a string) of the nth formal parameter for that 
	//function. The firstformal parameter corresponds to n = 1
	int i = n-1;
	while (i > 0) {
		ast = ast->child0;
		i--;
	}
	return ast->name;
}

char *func_call_callee(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->name;
}

void *func_call_args(void *ptr) {
	//returns a pointer to the AST for the list of arguments to the call
	struct ASTnode *ast = ptr;
	return ast->child0;
}

void *stmt_list_head(void *ptr) {
	//returns a pointer to the AST of the statement at the beginning ofthis list
	struct ASTnode *ast = ptr;
	return ast->child0;
}

void *stmt_list_rest(void *ptr) {
	//returns a pointer to the AST of the rest of this list (i.e., to the nextnode in the list).
	struct ASTnode *ast = ptr;
	return ast->child1;
}

void *expr_list_head(void *ptr) {
	//returns a pointer to the AST of the expression at the beginning ofthis list
	struct ASTnode *ast = ptr;
	return ast->child0;
}

void *expr_list_rest(void *ptr) {
	//returns a pointer to the AST of the rest of this list (i.e., to the nextnode in the list)
	struct ASTnode *ast = ptr;
	return ast->child1;
}

char *expr_id_name(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->name;
}

int expr_intconst_val(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->num;
}

void *expr_operand_1(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child0;
}

void *expr_operand_2(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child1;
}

void *stmt_if_expr(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child0;
}

void *stmt_if_then(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child1;
}

void *stmt_if_else(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child2;
}

char *stmt_assg_lhs(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child0->name;
}

void *stmt_assg_rhs(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child1;
}

void *stmt_while_expr(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child0;
}

void *stmt_while_body(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child1;
}

void *stmt_return_expr(void *ptr) {
	struct ASTnode *ast = ptr;
	return ast->child0;
}