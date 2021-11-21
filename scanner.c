#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "scanner.h"

/**
 * Scanner.c
 * By: Matthew Yungbluth
 * Class: CSc 453
 * Assignment 1 Milestone 1
 * This program reads in a string from stdin and outputs that string into its different parts with the "Token: Lexeme" format
 * Requires scanner-driver.c and scanner.h to work
 */
char *lexeme;
int keyword_or_id(char* buf);
int get_token();
int get_line_num();
int line_num = 1;

int get_line_num() {
  return line_num;
}

int keyword_or_id(char* buf) {
  char* keywordInt = "int";
  char* keywordIf = "if";
  char* keywordElse = "else";
  char* keywordWhile = "while";
  char* keywordReturn = "return";
  if (strcmp(buf, keywordInt) == 0) {
    return kwINT;
  }
  if (strcmp(buf, keywordIf) == 0) {
    return kwIF;
  }
  if (strcmp(buf, keywordElse) == 0) {
    return kwELSE;
  }
  if (strcmp(buf, keywordWhile) == 0) {
    return kwWHILE;
  }
  if (strcmp(buf, keywordReturn) == 0) {
    return kwRETURN;
  }
  //not a keyword, return as ID
  return ID;
}


int get_token() {
    int ch;
    ch = getchar();
    switch(ch) {
      //LARGE switch statment, used for all tokens except ID, keywords, and digits (aka anything that can start with multiple characters)
      case '\n':
        line_num = line_num + 1;
        return get_token();
        break;
      case '(':
        lexeme = "(";
        return LPAREN;
        break;
      case ')':
        lexeme = ")";
        return RPAREN;
        break;
      case '{':
        lexeme = "{";
        return LBRACE;
        break;
      case '}':
        lexeme = "}";
        return RBRACE;
        break;
      case ',':
        lexeme = ",";
        return COMMA;
        break;
      case ';':
        lexeme = ";";
        return SEMI;
        break;
      case '=':
        //opASSG or opEQ
        ch = getchar();
        if (ch == '=') {
          lexeme = "==";
          return opEQ;
        } else {
          ungetc(ch, stdin);
          lexeme = "=";
          return opASSG;
        }
        break;
      case '+':
        lexeme = "+";
        return opADD;
        break;
      case '-':
        lexeme = "-";
        return opSUB;
        break;
      case '*':
        lexeme = "*";
        return opMUL;
        break;
      case '/':
        ch = getchar();
        if (ch == '*') {
          //comment, ignore
          while (1) {
            //loop through entire comment until get to end of comment
            ch = getchar();
            if (ch == '\n') {
              line_num = line_num + 1;
            }
            if (ch == '*') {
              ch = getchar();
              if (ch == '/') {
                return get_token();
              } else {
                ungetc(ch, stdin);
              }
            }
          }
        } else {
          ungetc(ch, stdin);
          lexeme = "/";
          return opDIV;
        }
        break;
      case '!':
        ch = getchar();
        if (ch == '=') {
          lexeme = "!=";
          return opNE;
        }
        break;
      case '>':
        //opGT or opGE
        ch = getchar();
        if (ch == '=') {
          lexeme = ">=";
          return opGE;
        } else {
          ungetc(ch, stdin);
          lexeme = ">";
          return opGT;
        }
        break;
      case '<':
        //opLT or opLE
        ch = getchar();
        if (ch == '=') {
          lexeme = "<=";
          return opLE;
        } else {
          ungetc(ch, stdin);
          lexeme = "<";
          return opLT;
        }
        break;
      case '&':
        ch = getchar();
        if (ch == '&') {
          lexeme = "&&";
          return opAND;
        }
        break;
      case '|':
        ch = getchar();
        if (ch == '|') {
          lexeme = "||";
          return opOR;
        }
        break;
      case ' ':
        //whitespace, ignore
        return get_token();
        break;
      case '\t':
        return get_token();
        break;
    }
    //End of large switch statement

    if (isdigit(ch)) {
      //INTCON
      //modified code taken from slides
      char* buf = malloc(10000);
      char* ptr = malloc(10000);
      ptr = buf;
      *ptr++ = ch;
      ch = getchar();
      while (isdigit(ch)) {
        *ptr++ = ch;
        ch = getchar();
      }
      *ptr = '\0';
      if (ch != EOF) {
        ungetc(ch, stdin);
      }
      lexeme = buf;
      return INTCON;
    }
    if (isalpha(ch)) {
      //ID or KW
      //modified code taken from slides
      char * buf = malloc(10000);
      char* ptr = malloc(10000);
      ptr = buf;
      *ptr++ = ch;
      ch = getchar();
      while (isalnum(ch) || ch == '_') {
        *ptr++ = ch;
        ch = getchar();
      }
      *ptr = '\0';
      if (ch != EOF) {
        ungetc(ch, stdin);
      }
      lexeme = buf;
      return keyword_or_id(buf);
    }
    //End of File
    if (ch == EOF) {
      lexeme = '\0';
      return EOF;
    }
    //if it falls through, send back UNDEF
    //lexeme = '\0';
    //return UNDEF;
    return get_token();
}