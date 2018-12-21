
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include "ipcalc.h"
#include "defs.h"
#include "mpr.h"
#include "two_hop_neighbor_table.h"
#include "olsr.h"
#include "neighbor_table.h"
#include "scheduler.h"
#include "net_olsr.h"

/* Begin:
 * Prototypes for internal functions
 */

static uint16_t add_will_always_nodes(void);

static void olsr_optimize_mpr_set(void);

static void olsr_clear_mprs(void);

static void olsr_clear_two_hop_processed(void);

static struct neighbor_entry *olsr_find_maximum_covered(int);

static uint16_t olsr_calculate_two_hop_neighbors(void);

static int olsr_check_mpr_changes(void);

static int olsr_chosen_mpr(struct neighbor_entry *, uint16_t *);

static struct neighbor_2_list_entry *olsr_find_2_hop_neighbors_with_1_link(int);

/* End:
 * Prototypes for internal functions
 */

/**
 *Find all 2 hop neighbors with 1 link
 *connecting them to us trough neighbors
 *with a given willingness.
 *
 *@param willingness the willigness of the neighbors
 *
 *@return a linked list of allocated neighbor_2_list_entry structures
 */
static struct neighbor_2_list_entry *//函数功能:查找存在一条连接的两跳邻居节点链表。
//定义了两个两跳邻居节点链表  two _ hop _temp,two _ hop _list  ，前者用来作
//为函数的返回值。 dup _ neighbor用来记录在邻居节点集合中已经存在的邻居节
//点，而two _ hop _ neighbor是用来记录一个两跳邻居节点的局部变量。

olsr_find_2_hop_neighbors_with_1_link(int willingness)
{

  uint8_t idx;
  struct neighbor_2_list_entry *two_hop_list_tmp = NULL;
  struct neighbor_2_list_entry *two_hop_list = NULL;
  struct neighbor_entry *dup_neighbor;
  struct neighbor_2_entry *two_hop_neighbor = NULL;

  for (idx = 0; idx < HASHSIZE; idx++) {

    for (two_hop_neighbor = two_hop_neighbortable[idx].next; two_hop_neighbor != &two_hop_neighbortable[idx];
         two_hop_neighbor = two_hop_neighbor->next) {

      //two_hop_neighbor->neighbor_2_state=0;
      //two_hop_neighbor->mpr_covered_count = 0;

      dup_neighbor = olsr_lookup_neighbor_table(&two_hop_neighbor->neighbor_2_addr);

      if ((dup_neighbor != NULL) && (dup_neighbor->status != NOT_SYM)) {

        //OLSR_PRINTF(1, "(1)Skipping 2h neighbor %s - already 1hop\n", olsr_ip_to_string(&buf, &two_hop_neighbor->neighbor_2_addr));

        continue;//遍历两跳邻居表two _ hop _ neighbortable；
      }

      if (two_hop_neighbor->neighbor_2_pointer == 1) {
        if ((two_hop_neighbor->neighbor_2_nblist.next->neighbor->willingness == willingness)
            && (two_hop_neighbor->neighbor_2_nblist.next->neighbor->status == SYM)) {
          two_hop_list_tmp = olsr_malloc(sizeof(struct neighbor_2_list_entry), "MPR two hop list");//寻找在邻居表中已经存在的邻居地址neighbor _ 2 _ addr；

          //OLSR_PRINTF(1, "ONE LINK ADDING %s\n", olsr_ip_to_string(&buf, &two_hop_neighbor->neighbor_2_addr));//如果存在，并且该邻居节点与本节点不是非对称的节点忽略；

          /* Only queue one way here */
          two_hop_list_tmp->neighbor_2 = two_hop_neighbor;

          two_hop_list_tmp->next = two_hop_list;

          two_hop_list = two_hop_list_tmp;
        }
      }

    }

  }

  return (two_hop_list_tmp);//如果不存在并且该两跳邻居节点只有一个邻居节点，那么将两
//跳邻居节点添加到两跳邻居节点链表two _ hop _list中。

}

/**
 *This function processes the chosen MPRs and updates the counters
 *used in calculations
 */
static int//函数功能：用来处理已经选定的MPR节点，以及对MPR的计时器的更新，
//函数的返回值是一条邻居中MPR的数量。

olsr_chosen_mpr(struct neighbor_entry *one_hop_neighbor, uint16_t * two_hop_covered_count)
{
  struct neighbor_list_entry *the_one_hop_list;
  struct neighbor_2_list_entry *second_hop_entries;
  struct neighbor_entry *dup_neighbor;
  uint16_t count;
  struct ipaddr_str buf;
  count = *two_hop_covered_count;

  OLSR_PRINTF(1, "Setting %s as MPR\n", olsr_ip_to_string(&buf, &one_hop_neighbor->neighbor_main_addr));

  //printf("PRE COUNT: %d\n\n", count);

  one_hop_neighbor->is_mpr = true;      //NBS_MPR;

  for (second_hop_entries = one_hop_neighbor->neighbor_2_list.next; second_hop_entries != &one_hop_neighbor->neighbor_2_list;//对second  _ hop _entries进行遍历；
       second_hop_entries = second_hop_entries->next) {
    dup_neighbor = olsr_lookup_neighbor_table(&second_hop_entries->neighbor_2->neighbor_2_addr);

    if ((dup_neighbor != NULL) && (dup_neighbor->status == SYM)) {
      //OLSR_PRINTF(7, "(2)Skipping 2h neighbor %s - already 1hop\n", olsr_ip_to_string(&buf, &second_hop_entries->neighbor_2->neighbor_2_addr));
      continue;
    }
    //      if(!second_hop_entries->neighbor_2->neighbor_2_state)
    //if(second_hop_entries->neighbor_2->mpr_covered_count < olsr_cnf->mpr_coverage)
    //{
    /*
       Now the neighbor is covered by this mpr
     */
    second_hop_entries->neighbor_2->mpr_covered_count++;
    the_one_hop_list = second_hop_entries->neighbor_2->neighbor_2_nblist.next;//将已经存在的于邻居节点的节点忽略；

    //OLSR_PRINTF(1, "[%s](%x) has coverage %d\n", olsr_ip_to_string(&buf, &second_hop_entries->neighbor_2->neighbor_2_addr), second_hop_entries->neighbor_2, second_hop_entries->neighbor_2->mpr_covered_count);
//将两跳邻居节点被覆盖的MPR的数量mpr   _covered  _count递增
//1，同时将两跳邻居节点的邻居节点赋给the _one _ hop _list；

    if (second_hop_entries->neighbor_2->mpr_covered_count >= olsr_cnf->mpr_coverage)
      count++;

    while (the_one_hop_list != &second_hop_entries->neighbor_2->neighbor_2_nblist) {
		//如果两跳节点的 MPR的数量多于全局变量的  mpr _coverage，
//那么就要将count的数量增加1。
//函数的最后返回count值。

      if ((the_one_hop_list->neighbor->status == SYM)) {
        if (second_hop_entries->neighbor_2->mpr_covered_count >= olsr_cnf->mpr_coverage) {
          the_one_hop_list->neighbor->neighbor_2_nocov--;
        }
      }
      the_one_hop_list = the_one_hop_list->next;
    }

    //}
  }

  //printf("POST COUNT %d\n\n", count);

  *two_hop_covered_count = count;
  return count;

}

/**
 *Find the neighbor that covers the most 2 hop neighbors
 *with a given willingness
 *
 *@param willingness the willingness of the neighbor
 *
 *@return a pointer to the neighbor_entry struct
 */
static struct neighbor_entry *//找到能够覆盖到最多两跳节点的MPR。
olsr_find_maximum_covered(int willingness)
{
  uint16_t maximum;
  struct neighbor_entry *a_neighbor;
  struct neighbor_entry *mpr_candidate = NULL;

  maximum = 0;

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

#if 0
    printf("[%s] nocov: %d mpr: %d will: %d max: %d\n\n", olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr),
           a_neighbor->neighbor_2_nocov, a_neighbor->is_mpr, a_neighbor->willingness, maximum);
#endif

    if ((!a_neighbor->is_mpr) && (a_neighbor->willingness == willingness) && (maximum < a_neighbor->neighbor_2_nocov)) {
	//如果a _ neighbor不是MPR，同时覆盖两条节点数量值maximum
//小于 a _ neighbor邻居节点的该节点的数量，那么将   maximum值更新，并且将
//a _ neighbor作为候选的MPR节点。

      maximum = a_neighbor->neighbor_2_nocov;
      mpr_candidate = a_neighbor;
    }
  }
  OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  return mpr_candidate;
}

/**
 *Remove all MPR registrations
 */
static void//函数作用：将被选为MPR节点的记录清除。
olsr_clear_mprs(void)
{
  struct neighbor_entry *a_neighbor;
  struct neighbor_2_list_entry *two_hop_list;

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    /* Clear MPR selection. */
    if (a_neighbor->is_mpr) {
      a_neighbor->was_mpr = true;
      a_neighbor->is_mpr = false;
    }//如果节点a _ neighbor的is  _ mpr值为真的话，那么就将其置为假，
//代表其不是 MPR节点，同时将 was _mpr置为真，代表该邻居节点曾经被选为
//MPR。


    /* Clear two hop neighbors coverage count/ */
    for (two_hop_list = a_neighbor->neighbor_2_list.next; two_hop_list != &a_neighbor->neighbor_2_list;
         two_hop_list = two_hop_list->next) {
      two_hop_list->neighbor_2->mpr_covered_count = 0;
    }//遍历邻居节点a _ neighbor覆盖的两跳邻居节点的数量置为0。

  }
  OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);
}

/**
 *Check for changes in the MPR set
 *
 *@return 1 if changes occured 0 if not
 */
static int
olsr_check_mpr_changes(void)//通过遍历所有的MPR节点，返回值是1时代表节点是否是MPR
//的状态发生过变化，如果是0则代表没有发生变化

{
  struct neighbor_entry *a_neighbor;
  int retval;

  retval = 0;

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    if (a_neighbor->was_mpr) {
      a_neighbor->was_mpr = false;

      if (!a_neighbor->is_mpr) {
        retval = 1;//对于邻居节点a _ neighbor如果它曾经是MPR，但是现在又不是
//MPR，那么说明它的MPR的状态发生变化，将retval置为1，否则返回retval默
//认值0。

      }
    }
  }
  OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  return retval;
}

/**
 *Clears out proccess registration
 *on two hop neighbors
 */
static void
olsr_clear_two_hop_processed(void)
{
  int idx;

  for (idx = 0; idx < HASHSIZE; idx++) {
    struct neighbor_2_entry *neighbor_2;
    for (neighbor_2 = two_hop_neighbortable[idx].next; neighbor_2 != &two_hop_neighbortable[idx]; neighbor_2 = neighbor_2->next) {
      /* Clear */
      neighbor_2->processed = 0;
    }
  }

}

/**
 *This function calculates the number of two hop neighbors
 */
static uint16_t
olsr_calculate_two_hop_neighbors(void)
{
  struct neighbor_entry *a_neighbor, *dup_neighbor;
  struct neighbor_2_list_entry *twohop_neighbors;
  uint16_t count = 0;
  uint16_t n_count = 0;
  uint16_t sum = 0;

  /* Clear 2 hop neighs */
  olsr_clear_two_hop_processed();

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    if (a_neighbor->status == NOT_SYM) {
      a_neighbor->neighbor_2_nocov = count;
      continue;
    }

    for (twohop_neighbors = a_neighbor->neighbor_2_list.next; twohop_neighbors != &a_neighbor->neighbor_2_list;
         twohop_neighbors = twohop_neighbors->next) {

      dup_neighbor = olsr_lookup_neighbor_table(&twohop_neighbors->neighbor_2->neighbor_2_addr);

      if ((dup_neighbor == NULL) || (dup_neighbor->status != SYM)) {
        n_count++;
        if (!twohop_neighbors->neighbor_2->processed) {
          count++;
          twohop_neighbors->neighbor_2->processed = 1;
        }
      }
    }
    a_neighbor->neighbor_2_nocov = n_count;

    /* Add the two hop count */
    sum += count;

  }
  OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  OLSR_PRINTF(3, "Two hop neighbors: %d\n", sum);
  return sum;
}

/**
 * Adds all nodes with willingness set to WILL_ALWAYS
 */
static uint16_t//函数作用：添加willingness为WILL  _ ALWAYS的邻居节点到MPR集合中。
add_will_always_nodes(void)
{
  struct neighbor_entry *a_neighbor;
  uint16_t count = 0;

#if 0
  printf("\nAdding WILL ALWAYS nodes....\n");
#endif

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {
    struct ipaddr_str buf;
    if ((a_neighbor->status == NOT_SYM) || (a_neighbor->willingness != WILL_ALWAYS)) {
      continue;
    }
    olsr_chosen_mpr(a_neighbor, &count);

    OLSR_PRINTF(3, "Adding WILL_ALWAYS: %s\n", olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr));

  }
  OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

#if 0
  OLSR_PRINTF(1, "Count: %d\n", count);
#endif
  return count;//首先将非对称的邻居节点或是不是 WILL _ ALWAYS的邻居节点
//忽略，然后将剩余的节点  a _ neighbor添加到 mpr中，并返回添加节点的数量
//count。

}

/**
 *This function calculates the mpr neighbors
 *@return nada
 */
void
olsr_calculate_mpr(void)
{
  uint16_t two_hop_covered_count;
  uint16_t two_hop_count;
  int i;

  OLSR_PRINTF(3, "\n**RECALCULATING MPR**\n\n");

  olsr_clear_mprs();
  two_hop_count = olsr_calculate_two_hop_neighbors();
  two_hop_covered_count = add_will_always_nodes();

  /*
   *Calculate MPRs based on WILLINGNESS
   */

  for (i = WILL_ALWAYS - 1; i > WILL_NEVER; i--) {
    struct neighbor_entry *mprs;
    struct neighbor_2_list_entry *two_hop_list = olsr_find_2_hop_neighbors_with_1_link(i);

    while (two_hop_list != NULL) {
      struct neighbor_2_list_entry *tmp;
      //printf("CHOSEN FROM 1 LINK\n");
      if (!two_hop_list->neighbor_2->neighbor_2_nblist.next->neighbor->is_mpr)
        olsr_chosen_mpr(two_hop_list->neighbor_2->neighbor_2_nblist.next->neighbor, &two_hop_covered_count);
      tmp = two_hop_list;
      two_hop_list = two_hop_list->next;;
      free(tmp);
    }

    if (two_hop_covered_count >= two_hop_count) {
      i = WILL_NEVER;
      break;
    }
    //printf("two hop covered count: %d\n", two_hop_covered_count);

    while ((mprs = olsr_find_maximum_covered(i)) != NULL) {
      //printf("CHOSEN FROM MAXCOV\n");
      olsr_chosen_mpr(mprs, &two_hop_covered_count);

      if (two_hop_covered_count >= two_hop_count) {
        i = WILL_NEVER;
        break;
      }

    }
  }

  /*
     increment the mpr sequence number
   */
  //neighbortable.neighbor_mpr_seq++;

  /* Optimize selection */
  olsr_optimize_mpr_set();

  if (olsr_check_mpr_changes()) {
    OLSR_PRINTF(3, "CHANGES IN MPR SET\n");
    if (olsr_cnf->tc_redundancy > 0)
      signal_link_changes(true);
  }

}

/**
 *Optimize MPR set by removing all entries
 *where all 2 hop neighbors actually is
 *covered by enough MPRs already
 *Described in RFC3626 section 8.3.1
 *point 5
 *
 *@return nada
 */
static void
olsr_optimize_mpr_set(void)
{
  struct neighbor_entry *a_neighbor, *dup_neighbor;
  struct neighbor_2_list_entry *two_hop_list;
  int i, removeit;

#if 0
  printf("\n**MPR OPTIMIZING**\n\n");
#endif

  for (i = WILL_NEVER + 1; i < WILL_ALWAYS; i++) {

    OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

      if (a_neighbor->willingness != i) {
        continue;
      }

      if (a_neighbor->is_mpr) {
        removeit = 1;

        for (two_hop_list = a_neighbor->neighbor_2_list.next; two_hop_list != &a_neighbor->neighbor_2_list;
             two_hop_list = two_hop_list->next) {

          dup_neighbor = olsr_lookup_neighbor_table(&two_hop_list->neighbor_2->neighbor_2_addr);

          if ((dup_neighbor != NULL) && (dup_neighbor->status != NOT_SYM)) {
            continue;
          }
          //printf("\t[%s] coverage %d\n", olsr_ip_to_string(&buf, &two_hop_list->neighbor_2->neighbor_2_addr), two_hop_list->neighbor_2->mpr_covered_count);
          /* Do not remove if we find a entry which need this MPR */
          if (two_hop_list->neighbor_2->mpr_covered_count <= olsr_cnf->mpr_coverage) {
            removeit = 0;
          }
        }

        if (removeit) {
          struct ipaddr_str buf;
          OLSR_PRINTF(3, "MPR OPTIMIZE: removiong mpr %s\n\n", olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr));
          a_neighbor->is_mpr = false;
        }
      }
    } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);
  }
}

void
olsr_print_mpr_set(void)
{
#ifndef NODEBUG
  /* The whole function makes no sense without it. */
  struct neighbor_entry *a_neighbor;

  OLSR_PRINTF(1, "MPR SET: ");

  OLSR_FOR_ALL_NBR_ENTRIES(a_neighbor) {

    /*
     * Remove MPR settings
     */
    if (a_neighbor->is_mpr) {
      struct ipaddr_str buf;
      OLSR_PRINTF(1, "[%s] ", olsr_ip_to_string(&buf, &a_neighbor->neighbor_main_addr));
    }
  } OLSR_FOR_ALL_NBR_ENTRIES_END(a_neighbor);

  OLSR_PRINTF(1, "\n");
#endif
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
