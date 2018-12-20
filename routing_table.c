
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004, Andreas Tonnesen(andreto@olsr.org)
 * RIB implementation (c) 2007, Hannes Gredler (hannes@gredler.at)
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

#include "routing_table.h"
#include "ipcalc.h"
#include "defs.h"
#include "two_hop_neighbor_table.h"
#include "tc_set.h"
#include "mid_set.h"
#include "neighbor_table.h"
#include "olsr.h"
#include "link_set.h"
#include "common/avl.h"
#include "olsr_spf.h"
#include "net_olsr.h"

#include <assert.h>

/* Cookies */
struct olsr_cookie_info *rt_mem_cookie = NULL;
struct olsr_cookie_info *rtp_mem_cookie = NULL;

/*
 * Sven-Ola: if the current internet gateway is switched, the
 * NAT connection info is lost for any active TCP/UDP session.
 * For this reason, we do not want to switch if the advantage
 * is only minimal (cost of loosing all NATs is too high).
 * The following rt_path keeps track of the current inet gw.
 */
static struct rt_path *current_inetgw = NULL;

/* Root of our RIB */
struct avl_tree routingtree;

/*
 * Keep a version number for detecting outdated elements
 * in the per rt_entry rt_path subtree.
 */
unsigned int routingtree_version;

/**
 * Bump the version number of the routing tree.
 *
 * After route-insertion compare the version number of the routes
 * against the version number of the table.
 * This is a lightweight detection if a node or prefix went away,
 * rather than brute force old vs. new rt_entry comparision.
 */
unsigned int
olsr_bump_routingtree_version(void)
{
  return routingtree_version++;
}

/**
 * avl_comp_ipv4_prefix
 *
 * compare two ipv4 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv4_prefix(const void *prefix1, const void *prefix2)
{
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;
  const uint32_t addr1 = ntohl(pfx1->prefix.v4.s_addr);
  const uint32_t addr2 = ntohl(pfx2->prefix.v4.s_addr);

  /* prefix */
  if (addr1 < addr2) {
    return -1;
  }
  if (addr1 > addr2) {
    return +1;
  }

  /* prefix length */
  if (pfx1->prefix_len < pfx2->prefix_len) {
    return -1;
  }
  if (pfx1->prefix_len > pfx2->prefix_len) {
    return +1;
  }

  return 0;
}

/**
 * avl_comp_ipv6_prefix
 *
 * compare two ipv6 prefixes.
 * first compare the prefixes, then
 *  then compare the prefix lengths.
 *
 * return 0 if there is an exact match and
 * -1 / +1 depending on being smaller or bigger.
 */
int
avl_comp_ipv6_prefix(const void *prefix1, const void *prefix2)
{
  int res;
  const struct olsr_ip_prefix *pfx1 = prefix1;
  const struct olsr_ip_prefix *pfx2 = prefix2;

  /* prefix */
  res = memcmp(&pfx1->prefix.v6, &pfx2->prefix.v6, 16);
  if (res != 0) {
    return res;
  }
  /* prefix length */
  if (pfx1->prefix_len < pfx2->prefix_len) {
    return -1;
  }
  if (pfx1->prefix_len > pfx2->prefix_len) {
    return +1;
  }

  return 0;
}

/**
 * Initialize the routingtree and kernel change queues.
 */
void
olsr_init_routing_table(void)
{
  OLSR_PRINTF(5, "RIB: init routing tree\n");

  /* the routing tree */
  avl_init(&routingtree, avl_comp_prefix_default);
  routingtree_version = 0;//调用    avl _init() 初始化一个     avl 树。维护一个版本号
routingtree _version用以检测每一个  rt _ entry和 rt _ path子树中过时的信息,并
在此处将该值初始化为0。


  /*
   * Get some cookies for memory stats and memory recycling.
   */
  rt_mem_cookie = olsr_alloc_cookie("rt_entry", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(rt_mem_cookie, sizeof(struct rt_entry));//为rt _entry和rt    _ path分配内存，创建相应的cookie。

  rtp_mem_cookie = olsr_alloc_cookie("rt_path", OLSR_COOKIE_TYPE_MEMORY);
  olsr_cookie_set_memory_size(rtp_mem_cookie, sizeof(struct rt_path));
}

/**
 * Look up a maxplen entry (= /32 or /128) in the routing table.
 *
 * @param dst the address of the entry
 *
 * @return a pointer to a rt_entry struct
 * representing the route entry.
 */
struct rt_entry *
olsr_lookup_routing_table(const union olsr_ip_addr *dst)//函数功能：从avl树里面找到一个地址的路由表条目，根据参数地址并配上
设置里的最长前缀长度在  avl树里调用  avl _ find函数从  routingtree里面找到该
地址的rt _entry。

{
  struct avl_node *rt_tree_node;
  struct olsr_ip_prefix prefix;

  prefix.prefix = *dst;
  prefix.prefix_len = olsr_cnf->maxplen;//配置要查找的变量；

  rt_tree_node = avl_find(&routingtree, &prefix);

  return rt_tree_node ? rt_tree2rt(rt_tree_node) : NULL;//从在routingtree里找到该地址的位置并返回该节点，如果节点不
为空则调用rt _tree2rt函数把返回节点转化为et    _entry类型，否则返回空。

}

/**
 * Update gateway/interface/etx/hopcount and the version for a route path.
 */
void
olsr_update_rt_path(struct rt_path *rtp, struct tc_entry *tc, struct link_entry *link)
{

  rtp->rtp_version = routingtree_version;

  /* gateway */
  rtp->rtp_nexthop.gateway = link->neighbor_iface_addr;

  /* interface */
  rtp->rtp_nexthop.iif_index = link->inter->if_index;

  /* metric/etx */
  rtp->rtp_metric.hops = tc->hops;
  rtp->rtp_metric.cost = tc->path_cost;//每条路径都是周期性更新的。修改所维护的routingtree _version
值，并将相应的网关地址、接口地址、跳数和路径花销置为新接收到的值。此函
数用于为一条给定的rt _ path创建一个route   entry并将其插入到 RIB树中和计算
路由表。

}

/**
 * Alloc and key a new rt_entry.
 */
static struct rt_entry *
olsr_alloc_rt_entry(struct olsr_ip_prefix *prefix)
{
  struct rt_entry *rt = olsr_cookie_malloc(rt_mem_cookie);
  if (!rt) {
    return NULL;
  }//申请内存空间并把空间内清零

  memset(rt, 0, sizeof(*rt));

  /* Mark this entry as fresh (see process_routes.c:512) */
  rt->rt_nexthop.iif_index = -1;

  /* set key and backpointer prior to tree insertion */
  rt->rt_dst = *prefix;//标识该入口为新分配入口并把该入口的目的地址设置成为参数
提供的入口地址；


  rt->rt_tree_node.key = &rt->rt_dst;
  avl_insert(&routingtree, &rt->rt_tree_node, AVL_DUP_NO);

  /* init the originator subtree */
  avl_init(&rt->rt_path_tree, avl_comp_default);//把入口的树节点插入到整个路由表中并初始化树。

  return rt;//函数功能：创建一个可用的路由条目，对于提供的参数 IP前缀分配一个路
由条目空间，并做一些相应的初始化并把该入口插入到avl树里。

}

/**
 * Alloc and key a new rt_path.
 */
static struct rt_path *
olsr_alloc_rt_path(struct tc_entry *tc, struct olsr_ip_prefix *prefix, uint8_t origin)
{
  struct rt_path *rtp = olsr_cookie_malloc(rtp_mem_cookie);

  if (!rtp) {
    return NULL;
  }

  memset(rtp, 0, sizeof(*rtp));

  rtp->rtp_dst = *prefix;

  /* set key and backpointer prior to tree insertion */
  rtp->rtp_prefix_tree_node.key = &rtp->rtp_dst;

  /* insert to the tc prefix tree */
  avl_insert(&tc->prefix_tree, &rtp->rtp_prefix_tree_node, AVL_DUP_NO);
  olsr_lock_tc_entry(tc);

  /* backlink to the owning tc entry */
  rtp->rtp_tc = tc;

  /* store the origin of the route */
  rtp->rtp_origin = origin;

  return rtp;
}

/**
 * Create a route entry for a given rt_path and
 * insert it into the global RIB tree.
 */
void
olsr_insert_rt_path(struct rt_path *rtp, struct tc_entry *tc, struct link_entry *link)//函数功能：对于每个给予的rt _ path
//创建一个路由入口并且把它加入到全局的 RIB树里面。

{
  struct rt_entry *rt;
  struct avl_node *node;

  /*
   * no unreachable routes please.
   */
  if (tc->path_cost == ROUTE_COST_BROKEN) {
    return;
  }

  /*
   * No bogus prefix lengths.
   */
  if (rtp->rtp_dst.prefix_len > olsr_cnf->maxplen) {
    return;// 创建参数变量并检查参数是否符合要求，如果传入的 tc _entry
为 ROUTE _COST _ BROKEN或者传入的  rtp的目的地址长度大于所设置的最大
地址长度则直接返回；

  }

  /*
   * first check if there is a route_entry for the prefix.
   */
  node = avl_find(&routingtree, &rtp->rtp_dst);

  if (!node) {

    /* no route entry yet */
    rt = olsr_alloc_rt_entry(&rtp->rtp_dst);

    if (!rt) {
      return;
    }

  } else {
    rt = rt_tree2rt(node);
  }

  /* Now insert the rt_path to the owning rt_entry tree */
  rtp->rtp_originator = tc->addr;

  /* set key and backpointer prior to tree insertion */
  rtp->rtp_tree_node.key = &rtp->rtp_originator;

  /* insert to the route entry originator tree */
  avl_insert(&rt->rt_path_tree, &rtp->rtp_tree_node, AVL_DUP_NO);

  /* backlink to the owning route entry */// 调用avl _ find   函数检查传入的 rtp的节点是否在路由表中，
如果节点不在路由表的  avl树中则重新分配一个节点，如果在就把节点类型从
avl _ node类型转化成rt  _entry类型；

  rtp->rtp_rt = rt;

  /* update the version field and relevant parameters */
  olsr_update_rt_path(rtp, tc, link);//把新节点添加进avl树里，然后再改变相应的参数，更新整个路
由表。

}

/**
 * Unlink and free a rt_path.
 */
void
olsr_delete_rt_path(struct rt_path *rtp)
{

  /* remove from the originator tree */
  if (rtp->rtp_rt) {
    avl_delete(&rtp->rtp_rt->rt_path_tree, &rtp->rtp_tree_node);
    rtp->rtp_rt = NULL;//把 rtp所指向的树节点从所在的树里删除并把指向的树的根置
空；

  }

  /* remove from the tc prefix tree */
  if (rtp->rtp_tc) {
    avl_delete(&rtp->rtp_tc->prefix_tree, &rtp->rtp_prefix_tree_node);
    olsr_unlock_tc_entry(rtp->rtp_tc);
    rtp->rtp_tc = NULL;//将其从前缀数中移除，并解锁相应的tc _entry。
  }

  /* no current inet gw if the rt_path is removed */
  if (current_inetgw == rtp) {
    current_inetgw = NULL;//把rtp所指向的树节点再从  TC树里删除，释放 cookie所占用的
内存。

  }

  olsr_cookie_free(rtp_mem_cookie, rtp);//函数功能：从路由表中删除 rtp树并把它从TC树里删除然后改变路由表版
本。

}

/**
 * Check if there is an interface or gateway change.
 */
bool
olsr_nh_change(const struct rt_nexthop *nh1, const struct rt_nexthop *nh2)
{
  if (!ipequal(&nh1->gateway, &nh2->gateway) || (nh1->iif_index != nh2->iif_index)) {
    return true;
  }
  return false;
}

/**
 * Check if there is a hopcount change.
 */
bool
olsr_hopcount_change(const struct rt_metric * met1, const struct rt_metric * met2)
{
  return (met1->hops != met2->hops);
}

/**
 * Depending if flat_metric is configured and the kernel fib operation
 * return the hopcount metric of a route.
 * For adds this is the metric of best rour after olsr_rt_best() election,
 * for deletes this is the metric of the route that got stored in the rt_entry,
 * during route installation.
 */
uint8_t
olsr_fib_metric(const struct rt_metric * met)
{
  if (FIBM_CORRECT == olsr_cnf->fib_metric) {
    return met->hops;
  }
  return RT_METRIC_DEFAULT;
}

/**
 * depending on the operation (add/chg/del) the nexthop
 * field from the route entry or best route path shall be used.
 */
const struct rt_nexthop *
olsr_get_nh(const struct rt_entry *rt)
{

  if (rt->rt_best) {

    /* this is a route add/chg - grab nexthop from the best route. */
    return &rt->rt_best->rtp_nexthop;
  }

  /* this is a route deletion - all routes are gone. */
  return &rt->rt_nexthop;
}

/**
 * compare two route paths.
 *
 * returns TRUE if the first path is better
 * than the second one, FALSE otherwise.
 */
static bool
olsr_cmp_rtp(const struct rt_path *rtp1, const struct rt_path *rtp2, const struct rt_path *inetgw)
{//函数功能：比较两个路由路径，如果第一个路径更好的话返回真值  true  ；
  olsr_linkcost etx1 = rtp1->rtp_metric.cost;
  olsr_linkcost etx2 = rtp2->rtp_metric.cost;
  if (inetgw == rtp1)
    etx1 *= olsr_cnf->lq_nat_thresh;
  if (inetgw == rtp2)
    etx2 *= olsr_cnf->lq_nat_thresh;

  /* etx comes first *///比较两个路由路径的花销，花销小的路径更优；
  if (etx1 < etx2) {
    return true;
  }
  if (etx1 > etx2) {
    return false;
  }

  /* hopcount is next tie breaker *///如果两个路由路径花销相同，则比较两跳路径的跳数，跳数少
的更优。

  if (rtp1->rtp_metric.hops < rtp2->rtp_metric.hops) {
    return true;
  }
  if (rtp1->rtp_metric.hops > rtp2->rtp_metric.hops) {
    return false;
  }

  /* originator (which is guaranteed to be unique) is final tie breaker *///如果两个路由路径的花销和跳数都相同，则比较两条路由的源
地址，地址小的更优。

  if (memcmp(&rtp1->rtp_originator, &rtp2->rtp_originator, olsr_cnf->ipsize) < 0) {
    return true;
  }

  return false;
}

/**
 * compare the best path of two route entries.
 *
 * returns TRUE if the first entry is better
 * than the second one, FALSE otherwise.
 */
bool
olsr_cmp_rt(const struct rt_entry * rt1, const struct rt_entry * rt2)
{
  return olsr_cmp_rtp(rt1->rt_best, rt2->rt_best, NULL);
}

/**
 * run best route selection among a
 * set of identical prefixes.
 */
void
olsr_rt_best(struct rt_entry *rt)//运行最优路径,首先得到第一个条目然后遍历所有条目,找到一条最优路径
并修改当前网关路径到最佳路径。

{
  /* grab the first entry *///通过调用 avl _ walk _ first函数从  rt  rt _ path _tress里找到树
里的第一个条目并保证不为零，然后把节点类型转化为rt _entry类型

  struct avl_node *node = avl_walk_first(&rt->rt_path_tree);

  assert(node != 0);            /* should not happen */

  rt->rt_best = rtp_tree2rtp(node);

  /* walk all remaining originator entries *///遍历整棵树并比较当前路径与最佳路径，如果当前路径比最佳
路径还要好则把当前路径记录为最佳路径。

  while ((node = avl_walk_next(node))) {
    struct rt_path *rtp = rtp_tree2rtp(node);

    if (olsr_cmp_rtp(rtp, rt->rt_best, current_inetgw)) {
      rt->rt_best = rtp;
    }
  }

  if (0 == rt->rt_dst.prefix_len) {
    current_inetgw = rt->rt_best;
  }
}

/**
 * Insert a prefix into the prefix tree hanging off a lsdb (tc) entry.
 *
 * Check if the candidate route (we call this a rt_path) is known,
 * if not create it.
 * Upon post-SPF processing (if the node is reachable) the prefix
 * will be finally inserted into the global RIB.
 *
 *@param dst the destination
 *@param plen the prefix length
 *@param originator the originating router
 *
 *@return the new rt_path struct
 */
struct rt_path *
olsr_insert_routing_table(union olsr_ip_addr *dst, int plen, union olsr_ip_addr *originator, int origin)
{
#ifdef DEBUG
  struct ipaddr_str dstbuf, origbuf;
#endif
  struct tc_entry *tc;
  struct rt_path *rtp;
  struct avl_node *node;
  struct olsr_ip_prefix prefix;

  /*
   * No bogus prefix lengths.
   */
  if (plen > olsr_cnf->maxplen) {
    return NULL;
  }

  /*
   * For all routes we use the tc_entry as an hookup point.
   * If the tc_entry is disconnected, i.e. has no edges it will not
   * be explored during SPF run.
   */
  tc = olsr_locate_tc_entry(originator);//先调用olsr _ locate_ tc_ entry函数根据源地址判断该 tc _entry是否可
连接，因为tc _entry作为所有路由的连接点，如果它是不可连接的，则在最短路
径优先计算时它不会被考虑。


  /*
   * first check if there is a rt_path for the prefix.
   */
  prefix.prefix = *dst;
  prefix.prefix_len = plen;

  node = avl_find(&tc->prefix_tree, &prefix);

  if (!node) {

    /* no rt_path for this prefix yet */
    rtp = olsr_alloc_rt_path(tc, &prefix, origin);

    if (!rtp) {
      return NULL;
    }//首先检查该前缀是否已经存在一条路径。如果没有的话，则调
用olsr _ alloc _ rt _ path函数创建一条该前缀和源地址的路径。

#ifdef DEBUG
    OLSR_PRINTF(1, "RIB: add prefix %s/%u from %s\n", olsr_ip_to_string(&dstbuf, dst), plen,
                olsr_ip_to_string(&origbuf, originator));
#endif

    /* overload the hna change bit for flagging a prefix change */
    changes_hna = true;

  } else {
    rtp = rtp_prefix_tree2rtp(node);
  }

  return rtp;
}

/**
 * Delete a prefix from the prefix tree hanging off a lsdb (tc) entry.
 */
void
olsr_delete_routing_table(union olsr_ip_addr *dst, int plen, union olsr_ip_addr *originator)
{
#ifdef DEBUG
  struct ipaddr_str dstbuf, origbuf;
#endif

  struct tc_entry *tc;
  struct rt_path *rtp;
  struct avl_node *node;
  struct olsr_ip_prefix prefix;

  /*
   * No bogus prefix lengths.
   */
  if (plen > olsr_cnf->maxplen) {
    return;
  }

  tc = olsr_lookup_tc_entry(originator);
  if (!tc) {
    return;
  }

  /*
   * Grab the rt_path for the prefix.
   */
  prefix.prefix = *dst;
  prefix.prefix_len = plen;

  node = avl_find(&tc->prefix_tree, &prefix);

  if (node) {
    rtp = rtp_prefix_tree2rtp(node);
    olsr_delete_rt_path(rtp);

#ifdef DEBUG
    OLSR_PRINTF(1, "RIB: del prefix %s/%u from %s\n", olsr_ip_to_string(&dstbuf, dst), plen,
                olsr_ip_to_string(&origbuf, originator));
#endif

    /* overload the hna change bit for flagging a prefix change */
    changes_hna = true;
  }
}

/**
 * format a route entry into a buffer
 */
char *
olsr_rt_to_string(const struct rt_entry *rt)
{
  static char buff[128];
  struct ipaddr_str prefixstr, gwstr;

  snprintf(buff, sizeof(buff), "%s/%u via %s", olsr_ip_to_string(&prefixstr, &rt->rt_dst.prefix), rt->rt_dst.prefix_len,
           olsr_ip_to_string(&gwstr, &rt->rt_nexthop.gateway));

  return buff;
}

/**
 * format a route path into a buffer
 */
char *
olsr_rtp_to_string(const struct rt_path *rtp)
{
  static char buff[128];
  struct ipaddr_str prefixstr, origstr, gwstr;
  struct rt_entry *rt = rtp->rtp_rt;
  struct lqtextbuffer lqbuffer;

  snprintf(buff, sizeof(buff), "%s/%u from %s via %s, " "cost %s, metric %u, v %u",
           olsr_ip_to_string(&prefixstr, &rt->rt_dst.prefix), rt->rt_dst.prefix_len, olsr_ip_to_string(&origstr,
                                                                                                       &rtp->rtp_originator),
           olsr_ip_to_string(&gwstr, &rtp->rtp_nexthop.gateway), get_linkcost_text(rtp->rtp_metric.cost, true, &lqbuffer),
           rtp->rtp_metric.hops, rtp->rtp_version);

  return buff;
}

/**
 * Print the routingtree to STDOUT
 *
 */
void
olsr_print_routing_table(struct avl_tree *tree)
{
#ifndef NODEBUG
  /* The whole function makes no sense without it. */
  struct avl_node *rt_tree_node;
  struct lqtextbuffer lqbuffer;

  OLSR_PRINTF(6, "ROUTING TABLE\n");

  for (rt_tree_node = avl_walk_first(tree); rt_tree_node != NULL; rt_tree_node = avl_walk_next(rt_tree_node)) {
    struct avl_node *rtp_tree_node;
    struct ipaddr_str prefixstr, origstr, gwstr;
    struct rt_entry *rt = rt_tree2rt(rt_tree_node);

    /* first the route entry */
    OLSR_PRINTF(6, "%s/%u, via %s, best-originator %s\n", olsr_ip_to_string(&prefixstr, &rt->rt_dst.prefix), rt->rt_dst.prefix_len,
                olsr_ip_to_string(&origstr, &rt->rt_nexthop.gateway), olsr_ip_to_string(&gwstr, &rt->rt_best->rtp_originator));

    /* walk the per-originator path tree of routes */
    for (rtp_tree_node = avl_walk_first(&rt->rt_path_tree); rtp_tree_node != NULL; rtp_tree_node = avl_walk_next(rtp_tree_node)) {
      struct rt_path *rtp = rtp_tree2rtp(rtp_tree_node);
      OLSR_PRINTF(6, "\tfrom %s, cost %s, metric %u, via %s, %s, v %u\n", olsr_ip_to_string(&origstr, &rtp->rtp_originator),
                  get_linkcost_text(rtp->rtp_metric.cost, true, &lqbuffer), rtp->rtp_metric.hops, olsr_ip_to_string(&gwstr,
                                                                                                                    &rtp->
                                                                                                                    rtp_nexthop.
                                                                                                                    gateway),
                  if_ifwithindex_name(rt->rt_nexthop.iif_index), rtp->rtp_version);
    }
  }
#endif
  tree = NULL;                  /* squelch compiler warnings */
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
