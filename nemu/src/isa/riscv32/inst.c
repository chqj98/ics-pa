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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

#define X(i) gpr(i)
#define XLEN 32
#define has_S(i) (int32_t)i 
#define nu_S(i) (uint32_t)i 
#define Mr vaddr_read
#define Mw vaddr_write

int cnt = 0;

enum {
  TYPE_I, TYPE_U, TYPE_S, TYPE_J, TYPE_R, TYPE_B,
  TYPE_N, // none
};

#define src1R() do { *src1 = X(rs1); } while (0)
#define src2R() do { *src2 = X(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
#define immJ() do { *imm = (SEXT(BITS(i, 31, 31), 1) << 20) | BITS(i, 30, 21) << 1 | BITS(i, 19, 12) << 12 | BITS(i, 20, 20) << 11; } while (0)
#define immB() do { *imm = (SEXT(BITS(i, 31, 31), 1) << 12) | BITS(i, 11, 8) << 1 | BITS(i, 30, 25)<< 5 | BITS(i, 7, 7) << 11; } while (0)
#define show_info() do { printf("[imm] 0x%X \n", *imm); } while (0)
static void decode_operand(Decode *s, int *dest, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst.val;
  int rd  = BITS(i, 11, 7);
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *dest = rd;
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_J:                   immJ(); break;
    case TYPE_R: src1R(); src2R();         break;
    case TYPE_B: src1R(); src2R(); immB(); break;
  }
}

static int decode_exec(Decode *s) {
  int dest = 0;
  word_t src1 = 0, src2 = 0, imm = 0;
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  decode_operand(s, &dest, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  INSTPAT_START();
  //------"31.........20 19.15 14.2 11..7 6......0"
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, X(dest) = imm;);
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, X(dest) = s->pc + imm;); // PC加立即数
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, X(dest) = Mr(src1 + imm, 4););// 字加载
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, X(dest) = (nu_S(src1) < nu_S(imm))?1:0;); // 无符号小于立即数则置位
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli   , I, if(BITS(imm, 5, 5)==0){X(dest) = (src1 << BITS(imm, 5, 0));};); // 逻辑立即左移
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, X(dest) = has_S(src1) + has_S(imm);); // 加立即数
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai   , I, if(BITS(imm, 5, 5)==0){X(dest) = src1 >> BITS(imm, 5, 0);};); // 立即数算术右移
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli   , I, if(BITS(imm, 5, 5)==0){X(dest) = src1 >> BITS(imm, 5, 0);};); // 立即数逻辑右移
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, X(dest) = Mr(src1 + imm, 1);); // 无符号字节加载
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, X(dest) = src1 & imm;); // 与立即数
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, X(dest) = src1 ^ imm;); // 立即数异或
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, int32_t t = s->pc+4; s->dnpc = (src1 + imm)&(~1); X(dest) = t;); // 跳转并寄存器链接
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh     , I, X(dest) = SEXT(Mr(src1 + imm, 2), 16);); // 半字加载
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu    , I, X(dest) = Mr(src1 + imm, 2);); // 无符号半字加载
  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B, (has_S(src1) == has_S(src2))?(s->dnpc = s->pc + imm):(s->pc);); // 相等时分支
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, (has_S(src1) != has_S(src2))?(s->dnpc = s->pc + imm):(s->pc);); // 不相等时分支
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, (has_S(src1) < has_S(src2))?(s->dnpc = s->pc + imm):(s->pc);); // 小于时分支
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, (has_S(src1) >= has_S(src2))?(s->dnpc = s->pc + imm):(s->pc);); // 大于等于时分支
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu   , B, (nu_S(src1) < nu_S(src2))?(s->dnpc = s->pc + imm):(s->pc);); // 无符号小于时分支
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, (nu_S(src1) >= nu_S(src2))?(s->dnpc = s->pc + imm):(s->pc);); // 无符号大于等于时分支
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw(src1 + imm, 4, src2);); // 存字，将x[rs2]的低位4个字节存入内存地址x[rs1]+sext(offset)
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw(src1 + imm, 2, BITS(src2, 15, 0)););// 存半字（字节），将x[rs2]的低位2个字节存入内存地址x[rs1]+sext(offset)
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, BITS(src2, 7, 0)););// 存字节（字节），将x[rs2]的低位字节存入内存地址x[rs1]+sext(offset)
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, X(dest) = s->pc + 4; s->dnpc += imm - 4;); //跳转并链接
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt    , R, X(dest) = (has_S(src1) < has_S(src2)?1:0);); // 小于则置位
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, X(dest) = (nu_S(src1) < nu_S(src2))?1:0;); // 无符号小于则置位
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, X(dest) = has_S(src1) + has_S(src2);); // X[rd] = X[rs1] + X[rs2], 忽略算术溢出
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, X(dest) = has_S(src1) - has_S(src2);); // X[rd] = X[rs1] - X[rs2], 忽略算术溢出
  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul    , R, X(dest) = has_S(src1) * has_S(src2);); // X[rd] = X[rs1] * X[rs2], 忽略算术溢出
  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div    , R, X(dest) = has_S(src1) / has_S(src2);); // X[rd] = X[rs1] / X[rs2], 忽略算术溢出
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem    , R, X(dest) = has_S(src1) % has_S(src2);); // X[rd] = X[rs1] % X[rs2]
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, X(dest) = src1 ^ src2); // X[rd] = X[rs1] ^ X[rs2]
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, X(dest) = src1 | src2); // X[rd] = X[rs1] | X[rs2]
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, X(dest) = src1 << src2); // X[rd] = X[rs1] << X[rs2]
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, X(dest) = src1 & src2); // X[rd] = X[rs1] & X[rs2]
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, X(dest) = has_S(src1) >> has_S(src2)); // 算术右移
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, X(dest) = nu_S(src1) >> nu_S(src2)); // 逻辑右移
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11", mulh   , R, int64_t rs1 = SEXT(src1, 64); int64_t rs2 = SEXT(src2, 64); X(dest) = ((rs1 * rs2) >> XLEN);); // 高位乘

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, X(10))); // X(10) is $a0
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT_END();

  X(0) = 0; // reset $zero to 0

  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst.val = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}

/*
[         movsx] FAIL!
[        string] FAIL!
[     hello-str] FAIL!
*/