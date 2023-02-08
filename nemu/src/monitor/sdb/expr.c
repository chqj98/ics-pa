/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NUMBER, TK_UNEQ, TK_AND, TK_HEX_NUMBER, TK_REG_NAME

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  {" +", TK_NOTYPE},                      // spaces
  {"\\+", '+'},                           // plus
  {"==", TK_EQ},                          // equal
  {"!=", TK_UNEQ},                        // unequal
  {"&&", TK_AND},                         // and
  {"\\-", '-'},                           // minus
  {"\\*", '*'},                           // multiply
  {"\\/", '/'},                           // division
  {"[0-9]+", TK_NUMBER},                  // number
  {"\\$[a-zA-Z0-9\\{\\}]+", TK_REG_NAME}, // reg_name
  {"0x[0-9]+", TK_HEX_NUMBER},            // hex number
  {"\\(", '('},                           // left brackets
  {"\\)", ')'},                           // right brackets
  {"\\,", ','}                            // comma
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        switch (rules[i].token_type) {
          case TK_NOTYPE: {break;};
          case '+': {tokens[nr_token].type = '+';strcpy(tokens[nr_token].str, "+");nr_token++;break;};
          case '-': {tokens[nr_token].type = '-';strcpy(tokens[nr_token].str, "-");nr_token++;break;};
          case '*': {tokens[nr_token].type = '*';strcpy(tokens[nr_token].str, "*");nr_token++;break;};
          case '/': {tokens[nr_token].type = '/';strcpy(tokens[nr_token].str, "/");nr_token++;break;};
          case '(': {tokens[nr_token].type = '(';strcpy(tokens[nr_token].str, "(");nr_token++;break;};
          case ')': {tokens[nr_token].type = ')';strcpy(tokens[nr_token].str, ")");nr_token++;break;};
          case ',': {tokens[nr_token].type = ',';strcpy(tokens[nr_token].str, ",");nr_token++;break;};
          case TK_EQ: {tokens[nr_token].type = TK_EQ;strcpy(tokens[nr_token].str, "==");nr_token++;break;};
          case TK_NUMBER: {tokens[nr_token].type = TK_NUMBER;strncpy(tokens[nr_token].str, substr_start, substr_len);nr_token++;break;};
          case TK_UNEQ: {tokens[nr_token].type = TK_UNEQ;strcpy(tokens[nr_token].str, "!=");nr_token++;break;};
          case TK_AND: {tokens[nr_token].type = TK_AND;strcpy(tokens[nr_token].str, "&&");nr_token++;break;};
          case TK_REG_NAME: {tokens[nr_token].type = TK_REG_NAME;strncpy(tokens[nr_token].str, substr_start, substr_len);nr_token++;break;};
          case TK_HEX_NUMBER: {tokens[nr_token].type = TK_HEX_NUMBER;strncpy(tokens[nr_token].str, substr_start, substr_len);nr_token++;break;};
          default: TODO();
        }
        
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static int check_parentheses(int start_pos, int end_pos) {
  if(tokens[start_pos].type == '(' && tokens[end_pos].type == ')') {
    return 0; 
  }
  else if(tokens[start_pos].type == '(' && tokens[end_pos].type != ')') {
    return -2; 
  }
  else if(tokens[start_pos].type == '(' && tokens[end_pos].type != ')') {
    return -2; 
  }
  else {
    return -1; 
  }
}

static int find_op_pos(int start_pos, int end_pos) {
  bool dis_find_process = false;
  int op_pos = start_pos;
  for(int i = start_pos; i<end_pos; i++) {// find the main operator in the token expression first;
    if(tokens[i].type == '(') {
      dis_find_process = true;
    }
    if(tokens[i].type == ')' && dis_find_process == true) {
      dis_find_process = false;
    }
    if((tokens[i].type == '*' || tokens[i].type == '/') && (tokens[op_pos].type != '+' && tokens[op_pos].type != '-') && dis_find_process == false) {
      op_pos = i;
    }
    if((tokens[i].type == '+' || tokens[i].type == '-') && dis_find_process == false) {
      op_pos = i;
    }
  }
  return op_pos;
}

static int eval(int start_pos, int end_pos) {
  int re = 0;
  if(start_pos > end_pos) { // Bad expression
      Log("Bad expression");
      return 0;
  }
  else if(start_pos == end_pos) {
    if(tokens[start_pos].type == TK_NUMBER) {
      sscanf(tokens[start_pos].str, "%d", &re);
      return re;
    }
    else {
      Log("Bad expression");
      return 0;
    }
  }
  else {
    re = check_parentheses(start_pos, end_pos);
    if(re == 0) {
      return eval(start_pos + 1, end_pos - 1);
    }
    else if(re == -2) {
      Log("Bad expression");
      return 0;
    }
    else {
      int op_pos = start_pos;
      op_pos = find_op_pos(start_pos, end_pos);
      int lval = eval(start_pos, op_pos - 1);
      int rval = eval(op_pos + 1, end_pos);
      switch(tokens[op_pos].type) {
        case '+': return (lval+rval);
        case '-': return (lval-rval);
        case '*': return (lval*rval);
        case '/': return (lval/rval);
        default: TODO();
      }
    }
  }
}

word_t expr(char *e, bool *success) {
  int val;
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  val = eval(0, nr_token-1);
  printf("val: %d \n", val);
  return val;
}
