
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

#include "generate_msg.h"
#include "defs.h"
#include "olsr.h"
#include "build_msg.h"
#include "packet.h"

/*
 * Infomation repositiries
 */
#include "hna_set.h"
#include "mid_set.h"
#include "tc_set.h"
#include "mpr_selector_set.h"
#include "duplicate_set.h"
#include "neighbor_table.h"
#include "link_set.h"
#include "two_hop_neighbor_table.h"
#include "net_olsr.h"

static char pulsedata[] = { '\\', '|', '/', '-' };

#define PULSE_MAX 4
static uint8_t pulse_state = 0;

void
generate_hello(void *p)
{
  struct hello_message hellopacket;
  struct interface *ifn = (struct interface *)p;

  olsr_build_hello_packet(&hellopacket, ifn);

  if (queue_hello(&hellopacket, ifn))
    net_output(ifn);

  olsr_free_hello_packet(&hellopacket);// generate _ hello�������ڲ���һ����Ϣ���ٵ��� queue_  hello����
//�����ɴ��в���hellopacket���ݵ�HELLO����ifn�Ƿ�����Ϣ�Ľӿڣ���������ɹ���
//��ͨ�� net _output�������˰�ͨ�������ӿ� ifn���ͳ�ȥ�������ú���
//�ͷ���Ϣ��



}

void
generate_tc(void *p)
{
  struct tc_message tcpacket;
  struct interface *ifn = (struct interface *)p;

  olsr_build_tc_packet(&tcpacket);

  if (queue_tc(&tcpacket, ifn) && TIMED_OUT(ifn->fwdtimer)) {
    set_buffer_timer(ifn);
  }

  olsr_free_tc_packet(&tcpacket);
}//MID��Ϣͨ��olsr  _build  _tc _ packet?  ?��������֮�����MID��
//���С���ʱ���������ʱ�򣬵���set _buffer  _timer?   ?���ö�ʱ�������Ӹ���
//�ӿ�ifn�ͷ���Ϣ��ͬʱ����olsr  _  free _tc _ packet?  ?�ͷ���ռ�ڴ档


void
generate_mid(void *p)
{
  struct interface *ifn = (struct interface *)p;

  if (queue_mid(ifn) && TIMED_OUT(ifn->fwdtimer)) {
    set_buffer_timer(ifn);//����MID��Ϣ�Ľڵ㽫���ɵ�MID��Ϣ���Ž�һ�������У���
//����Ϣ�Ľڵ����ζ�ȡ�����е� MID��Ϣ������ȡ��ĳһ��Ϣʱ�������жϸ�
//��Ϣ�Ƿ���ڣ������ڣ������ȡ��ֱ�Ӷ��������ݰ���

  }

}

void
generate_hna(void *p)
{
  struct interface *ifn = (struct interface *)p;

  if (queue_hna(ifn) && TIMED_OUT(ifn->fwdtimer)) {
    set_buffer_timer(ifn);
        //���� HNA��Ϣ�Ľڵ㽫 HNA��Ϣ���ڶ����У�����ȡ������Ϣ
//ʱ���жϸ���Ϣ�Ƿ���ڣ���δ���ڣ����ȡ��Ϣ���ݡ�


  }
}

void
generate_stdout_pulse(void *foo __attribute__ ((unused)))
{
  if (olsr_cnf->debug_level == 0)
    return;

  pulse_state = pulse_state == 3 ? 0 : pulse_state + 1;

  printf("%c\r", pulsedata[pulse_state]);

}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
