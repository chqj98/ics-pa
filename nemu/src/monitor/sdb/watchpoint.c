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

#include "sdb.h"

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;
void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

WP* new_wp() {
  if(free_ == NULL) {
    assert(0);
  }
  WP *head_tmp = head;
  if(head == NULL) {
    head = free_;
    free_ = free_->next;
    head->next = NULL;
    return head;
  }
  else {
    head_tmp = free_;
    free_ = free_->next;
    head_tmp->next = head;
    head = head_tmp;
    return head;
  }
}

void free_wp(WP *wp) {
  if(head == NULL) { 
    assert(0);
  }
  WP *head_tmp = head;
  WP *head_forward_tmp = head;
  do {
    if(wp->NO == head_tmp->NO) { // find it, need free
      if(head_forward_tmp == head_tmp) {
        head = head_tmp->next;
        head_tmp->next = free_;
        free_ = head_tmp;
      }
      else {
        head_forward_tmp->next = head_tmp->next;
        head_tmp->next = free_;
        free_ = head_tmp;
      }
    }
    head_forward_tmp = head_tmp;
    head_tmp = head_tmp->next;
  } 
  while(head_tmp->next != NULL);
}

void show_head() {
  WP *head_tmp = head;
  if(head_tmp == NULL) {
    printf("head is empty.\n");
    return;
  }
  printf("head: \n");
  while(head_tmp != NULL) {
    printf("[%d]->", head_tmp->NO);
    head_tmp = head_tmp->next;
  }
  printf("end \n");
  return;
}

void show_free() {
  WP *head_tmp = free_;
  if(head_tmp == NULL) {
    printf("free is empty.\n");
    return;
  }
  printf("free: \n");
  while(head_tmp != NULL) {
    printf("[%d]->", head_tmp->NO);
    head_tmp = head_tmp->next;
  }
  printf("end \n");
  return;
}
/* TODO: Implement the functionality of watchpoint */

