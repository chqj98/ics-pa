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

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <locale.h>

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INST_TO_PRINT 10
#define RING_DEPTH 11

struct cricular_linked_list {
  struct cricular_linked_list* before;
  struct cricular_linked_list* next;
  char* disassemble_log;
} ring_fifo;

static struct cricular_linked_list* ring_head;
static struct cricular_linked_list* ring_end;
static struct cricular_linked_list* ring_using;

CPU_state cpu = {};
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us
static bool g_print_step = false;

void init_ring_buf() {
  struct cricular_linked_list* forward_ptr = (struct cricular_linked_list*)malloc(sizeof(ring_fifo));
  forward_ptr->disassemble_log = NULL;
  forward_ptr->next = NULL;
  forward_ptr->before = NULL;
  ring_head = forward_ptr;
  ring_using = forward_ptr;
  for (int i=1; i<RING_DEPTH; i++) {
    struct cricular_linked_list* ptr = (struct cricular_linked_list*)malloc(sizeof(ring_fifo));
    ptr->disassemble_log = NULL;
    ptr->next = NULL;
    ptr->before = forward_ptr;
    forward_ptr->next = ptr;
    forward_ptr = ptr;
  }
  ring_end = forward_ptr;
  ring_end->next = ring_head;
  ring_head->before = ring_end;
  
}

static void ring_buf_push_back (char* buf) {
  if(ring_using->disassemble_log != NULL) {
    free(ring_using->disassemble_log);
  }
  ring_using->disassemble_log = buf;
  ring_using = ring_using->next;
}

static void show_ring_buf() {
  struct cricular_linked_list* ring_using_ptr = ring_using;
  char* empty = "empty...... \n";
  do {
    printf("      %s", (ring_using_ptr->disassemble_log==NULL)?empty:ring_using_ptr->disassemble_log);
    ring_using_ptr = ring_using_ptr->next;
  } while (ring_using_ptr != ring_using->before);
  printf(" ---> %s", (ring_using_ptr->disassemble_log==NULL)?empty:ring_using_ptr->disassemble_log);
}

void device_update();

static void trace_and_difftest(Decode *_this, vaddr_t dnpc) {
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) { log_write("%s\n", _this->logbuf); }
#endif
  if (g_print_step) { IFDEF(CONFIG_ITRACE, puts(_this->logbuf)); }
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));
}

static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc;
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", pc);
  int ilen = s->snpc - pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst.val;
  for (i = ilen - 1; i >= 0; i --) {
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_riscv32, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;
  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
      MUXDEF(CONFIG_ISA_riscv32, s->snpc-4, s->pc), (uint8_t *)&s->isa.inst.val, ilen);
  char* log_buf = (char*)malloc(128*sizeof(char));
  memset(log_buf, '\0', 128);
  snprintf(log_buf, 127, "0x%08X: [%02X %02X %02X %02X]        %s\n", pc, inst[3], inst[2], inst[1], inst[0], p);
  ring_buf_push_back(log_buf);
#endif
}

static void execute(uint64_t n) {
  Decode s;
  for (;n > 0; n --) {
    exec_once(&s, cpu.pc);
    g_nr_guest_inst ++;
    trace_and_difftest(&s, cpu.pc);
    if (nemu_state.state != NEMU_RUNNING) break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}

static void statistic() {
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0) Log("simulation frequency = " NUMBERIC_FMT " inst/s", g_nr_guest_inst * 1000000 / g_timer);
  else Log("Finish running in less than 1 us and can not calculate the simulation frequency");
  if(nemu_state.state == NEMU_ABORT || nemu_state.halt_ret != 0) {
    show_ring_buf();
  }
}

void assert_fail_msg() {
  isa_reg_display();
  statistic();
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  g_print_step = (n < MAX_INST_TO_PRINT);
  switch (nemu_state.state) {
    case NEMU_END: case NEMU_ABORT:
      printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
      return;
    default: nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  execute(n);

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;

  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: case NEMU_ABORT:
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);
      // fall through
    case NEMU_QUIT: statistic();
  }
}
