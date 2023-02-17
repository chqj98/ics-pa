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

#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include <isa.h>

#define PMEM_RING_DEPTH 11

#if   defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else // CONFIG_PMEM_GARRAY
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

struct pmem_cricular_linked_list {
  struct pmem_cricular_linked_list* before;
  struct pmem_cricular_linked_list* next;
  char* disassemble_log;
} pmem_ring_fifo;

static struct pmem_cricular_linked_list* pmem_ring_head;
static struct pmem_cricular_linked_list* pmem_ring_end;
static struct pmem_cricular_linked_list* pmem_ring_using;

void init_pmem_ring_buf() {
  struct pmem_cricular_linked_list* forward_ptr = (struct pmem_cricular_linked_list*)malloc(sizeof(pmem_ring_fifo));
  forward_ptr->disassemble_log = NULL;
  forward_ptr->next = NULL;
  forward_ptr->before = NULL;
  pmem_ring_head = forward_ptr;
  pmem_ring_using = forward_ptr;
  for (int i=1; i<PMEM_RING_DEPTH; i++) {
    struct pmem_cricular_linked_list* ptr = (struct pmem_cricular_linked_list*)malloc(sizeof(pmem_ring_fifo));
    ptr->disassemble_log = NULL;
    ptr->next = NULL;
    ptr->before = forward_ptr;
    forward_ptr->next = ptr;
    forward_ptr = ptr;
  }
  pmem_ring_end = forward_ptr;
  pmem_ring_end->next = pmem_ring_head;
  pmem_ring_head->before = pmem_ring_end;
  
}

static void pmem_ring_buf_push_back (char* buf) {
  if(pmem_ring_using->disassemble_log != NULL) {
    free(pmem_ring_using->disassemble_log);
  }
  pmem_ring_using->disassemble_log = buf;
  pmem_ring_using = pmem_ring_using->next;
}

void pmem_show_ring_buf() {
  struct pmem_cricular_linked_list* ring_using_ptr = pmem_ring_using;
  char* empty = "empty...... \n";
  do {
    printf("      %s", (ring_using_ptr->disassemble_log==NULL)?empty:ring_using_ptr->disassemble_log);
    ring_using_ptr = ring_using_ptr->next;
  } while (ring_using_ptr != pmem_ring_using->before);
  printf(ANSI_FG_RED" ---> %s"ANSI_NONE, (ring_using_ptr->disassemble_log==NULL)?empty:ring_using_ptr->disassemble_log);
}


uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
paddr_t host_to_guest(uint8_t *haddr) { return haddr - pmem + CONFIG_MBASE; }

static word_t pmem_read(paddr_t addr, int len) {
  word_t ret = host_read(guest_to_host(addr), len);
  return ret;
}

static void pmem_write(paddr_t addr, int len, word_t data) {
  host_write(guest_to_host(addr), len, data);
}

static void out_of_bound(paddr_t addr) {
  pmem_show_ring_buf();
  panic("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
      addr, PMEM_LEFT, PMEM_RIGHT, cpu.pc);
}

void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
#endif
#ifdef CONFIG_MEM_RANDOM
  uint32_t *p = (uint32_t *)pmem;
  int i;
  for (i = 0; i < (int) (CONFIG_MSIZE / sizeof(p[0])); i ++) {
    p[i] = rand();
  }
#endif
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
  init_pmem_ring_buf();
}

word_t paddr_read(paddr_t addr, int len, int type) {
  if(type == 1) {
    char* log_buf = (char*)malloc(128);
    memset(log_buf, '\0', 128);
    snprintf(log_buf, 127, "[paddr_read] start[0x%08X], len[%d].\n", addr, len);
    pmem_ring_buf_push_back(log_buf);
  }

  if (likely(in_pmem(addr))) return pmem_read(addr, len);
  IFDEF(CONFIG_DEVICE, return mmio_read(addr, len));
  out_of_bound(addr);
  return 0;
}

void paddr_write(paddr_t addr, int len, word_t data) {
  char* log_buf = (char*)malloc(128);
  memset(log_buf, '\0', 128);
  snprintf(log_buf, 127, "[paddr_write] start[0x%08X], len[%d], data[0x%08X].\n", addr, len, data);
  pmem_ring_buf_push_back(log_buf);

  if (likely(in_pmem(addr))) { pmem_write(addr, len, data); return; }
  IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); return);
  out_of_bound(addr);
}
