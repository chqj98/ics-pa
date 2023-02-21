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
#include "local-include/reg.h"

struct map_reg {
  const int no;
  const char str[6];
} regs[] = {
 {0, "$0"}, {1, "ra"}, {2, "sp"}, {3, "gp"},
 {4, "tp"}, {5, "t0"}, {6, "t1"}, {7, "t2"},
 {8, "s0"}, {9, "s1"}, {10, "a0"}, {11, "a1"},
 {12, "a2"}, {13, "a3"}, {14, "a4"}, {15, "a5"},
 {16, "a6"}, {17, "a7"}, {18, "s2"}, {19, "s3"},
 {20, "s4"}, {21, "s5"}, {22, "s6"}, {23, "s7"},
 {24, "s8"}, {25, "s9"}, {26, "s10"}, {27, "s11"},
 {28, "t3"}, {29, "t4"}, {30, "t5"}, {31, "t6"}
};

int isa_reg_str_to_index(const char *s) {
  for(int i=0; i<32; i++) {
    if(strcmp(s, regs[i].str) == 0) {
      return i;
    }
  }
  return -1;
}

void isa_reg_index_to_str(int i, char* str) {
  strcpy(str, regs[i].str);
}

const char* reg_display_list[] = {
  "$0", "ra", "sp", "gp", 
  "tp", "t0", "t1", "t2",
  "a0", "a1", "a2", "a3", 
  "a4", "a5", "a6", "a7", 
  "s0", "s1", "s2", "s3", 
  "s4", "s5", "s6", "s7", 
  "s8", "s9", "s10", "s11", 
  "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
  printf("[ pc]: 0x%08X \n", cpu.pc);
  for(int i=0; i<32; i+=4) {
    printf("[%3s]: 0x%08X | [%3s]: 0x%08X | [%3s]: 0x%08X | [%3s]: 0x%08X \n"
      ,reg_display_list[i] ,cpu.gpr[isa_reg_str_to_index(reg_display_list[i])] 
      ,reg_display_list[i+1] ,cpu.gpr[isa_reg_str_to_index(reg_display_list[i+1])] 
      ,reg_display_list[i+2] ,cpu.gpr[isa_reg_str_to_index(reg_display_list[i+2])] 
      ,reg_display_list[i+3] ,cpu.gpr[isa_reg_str_to_index(reg_display_list[i+3])] );
  }
}

