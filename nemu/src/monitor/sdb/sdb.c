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
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"


#define Mr vaddr_read

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();
void init_ring_buf();
/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_si(char *args) {
  if(args == NULL) {
    cpu_exec(1);
    return 0;
  }
  int n = atoi(args);
  if(n <= 0) {
    Log("illegal input");
    return 0;
  }
  cpu_exec(n);
  isa_reg_display();
  return 0;
}

static int cmd_info(char *args) {
  if(args == NULL) {
    Log("illegal input, eg: info r/w");
    return 0;
  }
  if(strcmp(args, INFO_SUBCMD_R) == 0) {
    isa_reg_display();
    return 0;
  }
  else if(strcmp(args, INFO_SUBCMD_W) == 0) {

    return -1;
  }
  else {
    Log("unknown sub-cmd");
    return 0;
  }
}

static int cmd_x(char *args) {
  char* arg1 = NULL;
  char* arg2 = NULL;
  int arg1_i, arg2_i, threshold = 0;
  if(args == NULL) {
    Log("illegal input, eg: x N EXPR(10 $esp)");
    return 0;
  }

  arg2 = (char*) memchr(args, ' ', strlen(args)) + 1;
  arg1 = (char*) malloc(strlen(args) - strlen(arg2));
  memset(arg1, '\0', strlen(args) - strlen(arg2));
  memcpy(arg1, args, strlen(args) - strlen(arg2) - 1);
  sscanf(arg1, "%d", &arg1_i);  
  sscanf(arg2, "%X", &arg2_i);  
  free(arg1);
  
  threshold = arg1_i/4 + 1;
  for(int i=0; i<threshold; i++) {
    printf("[0x%08X] 0x%08X \n",arg2_i ,Mr(arg2_i, 4));
    arg2_i += 0x4;
  }
  return 0;
}

static int cmd_p(char *args) {
  bool result = false;
  if(args == NULL) {
    Log("illegal input, eg: p EXPR($eax + 1)");
    return 0;
  }
  expr(args, &result);
  return 0;
}

static int cmd_w(char *args) {
  
  return 0;
}

static int cmd_d(char *args) {
  return 0;
}

static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Execute N instructions in a single step, eg: si [N](default:1)", cmd_si },
  { "x", "do scanning the memory form EXPR, eg: x N EXPR(10 $esp)", cmd_x },
  { "p", "evaluate the expression, eg: p EXPR($eax + 1)", cmd_p },
  { "w", "add a new watchpoints, eg: w EXPR($eax)", cmd_w },
  { "d", "delete waitchpoint, eg: d N(2)", cmd_d },
  { "info", "Get more info from nemu what you need, eg: info r/w", cmd_info },

  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();

  /* Initialize the ring buf*/
  init_ring_buf();
}
