/*
 * net/sched/sch_esfq.c	Extended Stochastic Fairness Queueing discipline.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Changes:	Alexander Atanasov, <alex@ssi.bg>
 *		Added dynamic depth,limit,divisor,hash_kind options.
 *		Added dst and src hashes.
 *
 * 		Alexander Clouter, <alex@digriz.org.uk>
 *		Ported ESFQ to Linux 2.6.
 *
 * 		Corey Hickey, <bugfood-c@fatooh.org>
 *		Maintenance of the Linux 2.6 port.
 *		Added fwmark hash (thanks to Robert Kurjata)
 *		Added direct hashing for src, dst, and fwmark.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <net/ip.h>
#include <linux/ipv6.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/pkt_sched.h>


/*	Stochastic Fairness Queuing algorithm.
	For more comments look at sch_sfq.c.
	The difference is that you can change limit, depth,
	hash table size and choose 7 hash types.

	classic:	same as in sch_sfq.c
	dst:		destination IP address
	src:		source IP address
	fwmark:         netfilter mark value
	dst_direct:
	src_direct:
	fwmark_direct:  direct hashing of the above sources

	TODO:
		make sfq_change work.
*/

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif

/* This type should contain at least SFQ_DEPTH*2 values */
typedef unsigned int esfq_index;

struct esfq_head
{
	esfq_index	next;
	esfq_index	prev;
};

struct esfq_sched_data
{
/* Parameters */
	int		perturb_period;
	unsigned	quantum;	/* Allotment per round: MUST BE >= MTU */
	int		limit;
	unsigned	depth;
	unsigned	hash_divisor;
	unsigned	hash_kind;
/* Variables */
	struct timer_list perturb_timer;
	int		perturbation;
	esfq_index	tail;		/* Index of current slot in round */
	esfq_index	max_depth;	/* Maximal depth */

	esfq_index	*ht;			/* Hash table */
	esfq_index	*next;			/* Active slots link */
	short		*allot;			/* Current allotment per slot */
	unsigned short	*hash;			/* Hash value indexed by slots */
	struct sk_buff_head	*qs;		/* Slot queue */
	struct esfq_head	*dep;		/* Linked list of slots, indexed by depth */
	unsigned	dyn_min;	/* For dynamic divisor adjustment; minimum value seen */
	unsigned	dyn_max;	/*                                 maximum value seen */
	unsigned	dyn_range;	/*	        		   saved range */
};

static __inline__ unsigned esfq_hash_u32(struct esfq_sched_data *q,u32 h)
{
	int pert = q->perturbation;

	if (pert)
		h = (h<<pert) ^ (h>>(0x1F - pert));

	h = ntohl(h) * 2654435761UL;
	return h & (q->hash_divisor-1);
}

/* Hash input values directly into the "nearest" slot, taking into account the
 * range of input values seen. This is most useful when the hash table is at
 * least as large as the range of possible values. */
static __inline__ unsigned esfq_hash_direct(struct esfq_sched_data *q, u32 h)
{
	/* adjust minimum and maximum */
	if (h < q->dyn_min || h > q->dyn_max) {
		q->dyn_min = h < q->dyn_min ? h : q->dyn_min;
		q->dyn_max = h > q->dyn_max ? h : q->dyn_max;

		/* find new range */
		if ((q->dyn_range = q->dyn_max - q->dyn_min) >= q->hash_divisor)
			printk(KERN_WARNING "ESFQ: (direct hash) Input range %u is larger than hash "
					"table. See ESFQ README for details.\n", q->dyn_range);
	}

	/* hash input values into slot numbers */
	if (q->dyn_min == q->dyn_max)
		return 0; /* only one value seen; avoid division by 0 */
	else
		return (h - q->dyn_min) * (q->hash_divisor - 1) / q->dyn_range;
}

static __inline__ unsigned esfq_fold_hash_classic(struct esfq_sched_data *q, u32 h, u32 h1)
{
	int pert = q->perturbation;

	/* Have we any rotation primitives? If not, WHY? */
	h ^= (h1<<pert) ^ (h1>>(0x1F - pert));
	h ^= h>>10;
	return h & (q->hash_divisor-1);
}

static unsigned esfq_hash(struct esfq_sched_data *q, struct sk_buff *skb)
{
	u32 h, h2;
	u32 hs;
	u32 nfm;

	switch (skb->protocol) {
	case __constant_htons(ETH_P_IP):
	{
		struct iphdr *iph = skb->nh.iph;
		h = iph->daddr;
		hs = iph->saddr;
		nfm = skb->nfmark;
		h2 = hs^iph->protocol;
		if (!(iph->frag_off&htons(IP_MF|IP_OFFSET)) &&
		    (iph->protocol == IPPROTO_TCP ||
		     iph->protocol == IPPROTO_UDP ||
		     iph->protocol == IPPROTO_SCTP ||
		     iph->protocol == IPPROTO_DCCP ||
		     iph->protocol == IPPROTO_ESP))
			h2 ^= *(((u32*)iph) + iph->ihl);
		break;
	}
	case __constant_htons(ETH_P_IPV6):
	{
		struct ipv6hdr *iph = skb->nh.ipv6h;
		h = iph->daddr.s6_addr32[3];
		hs = iph->saddr.s6_addr32[3];
		nfm = skb->nfmark;
		h2 = hs^iph->nexthdr;
		if (iph->nexthdr == IPPROTO_TCP ||
		    iph->nexthdr == IPPROTO_UDP ||
		    iph->nexthdr == IPPROTO_SCTP ||
		    iph->nexthdr == IPPROTO_DCCP ||
		    iph->nexthdr == IPPROTO_ESP)
			h2 ^= *(u32*)&iph[1];
		break;
	}
	default:
		h = (u32)(unsigned long)skb->dst;
		hs = (u32)(unsigned long)skb->sk;
		nfm = skb->nfmark;
		h2 = hs^skb->protocol;
	}
	switch(q->hash_kind)
	{
	case TCA_SFQ_HASH_CLASSIC:
		return esfq_fold_hash_classic(q, h, h2);
	case TCA_SFQ_HASH_DST:
		return esfq_hash_u32(q,h);
	case TCA_SFQ_HASH_DSTDIR:
		return esfq_hash_direct(q, ntohl(h));
	case TCA_SFQ_HASH_SRC:
		return esfq_hash_u32(q,hs);
	case TCA_SFQ_HASH_SRCDIR:
		return esfq_hash_direct(q, ntohl(hs));
#ifdef CONFIG_NETFILTER
	case TCA_SFQ_HASH_FWMARK:
		return esfq_hash_u32(q,nfm);
	case TCA_SFQ_HASH_FWMARKDIR:
		return esfq_hash_direct(q,nfm);
#endif
	default:
		if (net_ratelimit())
			printk(KERN_WARNING "ESFQ: Unknown hash method. Falling back to classic.\n");
	}
	return esfq_fold_hash_classic(q, h, h2);
}

static inline void esfq_link(struct esfq_sched_data *q, esfq_index x)
{
	esfq_index p, n;
	int d = q->qs[x].qlen + q->depth;

	p = d;
	n = q->dep[d].next;
	q->dep[x].next = n;
	q->dep[x].prev = p;
	q->dep[p].next = q->dep[n].prev = x;
}

static inline void esfq_dec(struct esfq_sched_data *q, esfq_index x)
{
	esfq_index p, n;

	n = q->dep[x].next;
	p = q->dep[x].prev;
	q->dep[p].next = n;
	q->dep[n].prev = p;

	if (n == p && q->max_depth == q->qs[x].qlen + 1)
		q->max_depth--;

	esfq_link(q, x);
}

static inline void esfq_inc(struct esfq_sched_data *q, esfq_index x)
{
	esfq_index p, n;
	int d;

	n = q->dep[x].next;
	p = q->dep[x].prev;
	q->dep[p].next = n;
	q->dep[n].prev = p;
	d = q->qs[x].qlen;
	if (q->max_depth < d)
		q->max_depth = d;

	esfq_link(q, x);
}

static int esfq_drop(struct Qdisc *sch)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	esfq_index d = q->max_depth;
	struct sk_buff *skb;
	unsigned int len;

	/* Queue is full! Find the longest slot and
	   drop a packet from it */

	if (d > 1) {
		esfq_index x = q->dep[d+q->depth].next;
		skb = q->qs[x].prev;
		len = skb->len;
		__skb_unlink(skb, &q->qs[x]);
		kfree_skb(skb);
		esfq_dec(q, x);
		sch->q.qlen--;
		sch->stats.drops++;
		return len;
	}

	if (d == 1) {
		/* It is difficult to believe, but ALL THE SLOTS HAVE LENGTH 1. */
		d = q->next[q->tail];
		q->next[q->tail] = q->next[d];
		q->allot[q->next[d]] += q->quantum;
		skb = q->qs[d].prev;
		len = skb->len;
		__skb_unlink(skb, &q->qs[d]);
		kfree_skb(skb);
		esfq_dec(q, d);
		sch->q.qlen--;
		q->ht[q->hash[d]] = q->depth;
		sch->stats.drops++;
		return len;
	}

	return 0;
}

static int
esfq_enqueue(struct sk_buff *skb, struct Qdisc* sch)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	unsigned hash = esfq_hash(q, skb);
	unsigned depth = q->depth;
	esfq_index x;

	x = q->ht[hash];
	if (x == depth) {
		q->ht[hash] = x = q->dep[depth].next;
		q->hash[x] = hash;
	}
	__skb_queue_tail(&q->qs[x], skb);
	esfq_inc(q, x);
	if (q->qs[x].qlen == 1) {		/* The flow is new */
		if (q->tail == depth) {	/* It is the first flow */
			q->tail = x;
			q->next[x] = x;
			q->allot[x] = q->quantum;
		} else {
			q->next[x] = q->next[q->tail];
			q->next[q->tail] = x;
			q->tail = x;
		}
	}
	if (++sch->q.qlen < q->limit-1) {
		sch->stats.bytes += skb->len;
		sch->stats.packets++;
		return 0;
	}

	esfq_drop(sch);
	return NET_XMIT_CN;
}

static int
esfq_requeue(struct sk_buff *skb, struct Qdisc* sch)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	unsigned hash = esfq_hash(q, skb);
	unsigned depth = q->depth;
	esfq_index x;

	x = q->ht[hash];
	if (x == depth) {
		q->ht[hash] = x = q->dep[depth].next;
		q->hash[x] = hash;
	}
	__skb_queue_head(&q->qs[x], skb);
	esfq_inc(q, x);
	if (q->qs[x].qlen == 1) {		/* The flow is new */
		if (q->tail == depth) {	/* It is the first flow */
			q->tail = x;
			q->next[x] = x;
			q->allot[x] = q->quantum;
		} else {
			q->next[x] = q->next[q->tail];
			q->next[q->tail] = x;
			q->tail = x;
		}
	}
	if (++sch->q.qlen < q->limit - 1) {
//		sch->stats.requeues++;
		return 0;
	}

	sch->stats.drops++;
	esfq_drop(sch);
	return NET_XMIT_CN;
}




static struct sk_buff *
esfq_dequeue(struct Qdisc* sch)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	struct sk_buff *skb;
	unsigned depth = q->depth;
	esfq_index a, old_a;

	/* No active slots */
	if (q->tail == depth)
		return NULL;

	a = old_a = q->next[q->tail];

	/* Grab packet */
	skb = __skb_dequeue(&q->qs[a]);
	esfq_dec(q, a);
	sch->q.qlen--;

	/* Is the slot empty? */
	if (q->qs[a].qlen == 0) {
		q->ht[q->hash[a]] = depth;
		a = q->next[a];
		if (a == old_a) {
			q->tail = depth;
			return skb;
		}
		q->next[q->tail] = a;
		q->allot[a] += q->quantum;
	} else if ((q->allot[a] -= skb->len) <= 0) {
		q->tail = a;
		a = q->next[a];
		q->allot[a] += q->quantum;
	}

	return skb;
}

static void
esfq_reset(struct Qdisc* sch)
{
	struct sk_buff *skb;

	while ((skb = esfq_dequeue(sch)) != NULL)
		kfree_skb(skb);
}

static void esfq_perturbation(unsigned long arg)
{
	struct Qdisc *sch = (struct Qdisc*)arg;
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;

	q->perturbation = net_random()&0x1F;

	if (q->perturb_period) {
		q->perturb_timer.expires = jiffies + q->perturb_period;
		add_timer(&q->perturb_timer);
	}
}

/*
static int esfq_change(struct Qdisc *sch, struct rtattr *opt)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	struct tc_esfq_qopt *ctl = RTA_DATA(opt);
	int old_perturb = q->perturb_period;

	if (opt->rta_len < RTA_LENGTH(sizeof(*ctl)))
		return -EINVAL;

	sch_tree_lock(sch);
	q->quantum = ctl->quantum ? : psched_mtu(sch->dev);
	q->perturb_period = ctl->perturb_period*HZ;
//	q->hash_divisor = ctl->divisor;
//	q->tail = q->limit = q->depth = ctl->flows;

	if (ctl->limit)
		q->limit = min_t(u32, ctl->limit, q->depth);

	if (ctl->hash_kind) {
		q->hash_kind = ctl->hash_kind;
		if (q->hash_kind !=  TCA_SFQ_HASH_CLASSIC)
			q->perturb_period = 0;
	}

	// is sch_tree_lock enough to do this ?
	while (sch->q.qlen >= q->limit-1)
		esfq_drop(sch);

	if (old_perturb)
		del_timer(&q->perturb_timer);
	if (q->perturb_period) {
		q->perturb_timer.expires = jiffies + q->perturb_period;
		add_timer(&q->perturb_timer);
	} else {
		q->perturbation = 0;
	}
	sch_tree_unlock(sch);
	return 0;
}
*/

static int esfq_init(struct Qdisc *sch, struct rtattr *opt)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	struct tc_esfq_qopt *ctl;
	esfq_index p = ~0UL/2;
	int i;

	if (opt && opt->rta_len < RTA_LENGTH(sizeof(*ctl)))
		return -EINVAL;

	init_timer(&q->perturb_timer);
	q->perturb_timer.data = (unsigned long)sch;
	q->perturb_timer.function = esfq_perturbation;
	q->perturbation = 0;
	q->hash_kind = TCA_SFQ_HASH_CLASSIC;
	q->max_depth = 0;
	q->dyn_min = ~0U; /* maximum value for this type */
	q->dyn_max = 0;  /* dyn_min/dyn_max will be set properly upon first packet */
	if (opt == NULL) {
		q->quantum = psched_mtu(sch->dev);
		q->perturb_period = 0;
		q->hash_divisor = 1024;
		q->tail = q->limit = q->depth = 128;

	} else {
		ctl = RTA_DATA(opt);
		q->quantum = ctl->quantum ? : psched_mtu(sch->dev);
		q->perturb_period = ctl->perturb_period*HZ;
		q->hash_divisor = ctl->divisor ? : 1024;
		q->tail = q->limit = q->depth = ctl->flows ? : 128;

		if ( q->depth > p - 1 )
			return -EINVAL;

		if (ctl->limit)
			q->limit = min_t(u32, ctl->limit, q->depth);

		if (ctl->hash_kind) {
			q->hash_kind = ctl->hash_kind;
		}

		if (q->perturb_period) {
			q->perturb_timer.expires = jiffies + q->perturb_period;
			add_timer(&q->perturb_timer);
		}
	}

	q->ht = kmalloc(q->hash_divisor*sizeof(esfq_index), GFP_KERNEL);
	if (!q->ht)
		goto err_case;

	q->dep = kmalloc((1+q->depth*2)*sizeof(struct esfq_head), GFP_KERNEL);
	if (!q->dep)
		goto err_case;
	q->next = kmalloc(q->depth*sizeof(esfq_index), GFP_KERNEL);
	if (!q->next)
		goto err_case;

	q->allot = kmalloc(q->depth*sizeof(short), GFP_KERNEL);
	if (!q->allot)
		goto err_case;
	q->hash = kmalloc(q->depth*sizeof(unsigned short), GFP_KERNEL);
	if (!q->hash)
		goto err_case;
	q->qs = kmalloc(q->depth*sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!q->qs)
		goto err_case;

	for (i=0; i< q->hash_divisor; i++)
		q->ht[i] = q->depth;
	for (i=0; i<q->depth; i++) {
		skb_queue_head_init(&q->qs[i]);
		q->dep[i+q->depth].next = i+q->depth;
		q->dep[i+q->depth].prev = i+q->depth;
	}

	for (i=0; i<q->depth; i++)
		esfq_link(q, i);
	return 0;
err_case:
	del_timer(&q->perturb_timer);
	if (q->ht)
		kfree(q->ht);
	if (q->dep)
		kfree(q->dep);
	if (q->next)
		kfree(q->next);
	if (q->allot)
		kfree(q->allot);
	if (q->hash)
		kfree(q->hash);
	if (q->qs)
		kfree(q->qs);
	return -ENOBUFS;
}

static void esfq_destroy(struct Qdisc *sch)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	del_timer(&q->perturb_timer);
	if(q->ht)
		kfree(q->ht);
	if(q->dep)
		kfree(q->dep);
	if(q->next)
		kfree(q->next);
	if(q->allot)
		kfree(q->allot);
	if(q->hash)
		kfree(q->hash);
	if(q->qs)
		kfree(q->qs);
}

static int esfq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct esfq_sched_data *q = (struct esfq_sched_data *)sch->data;
	unsigned char	 *b = skb->tail;
	struct tc_esfq_qopt opt;

	opt.quantum = q->quantum;
	opt.perturb_period = q->perturb_period/HZ;

	opt.limit = q->limit;
	opt.divisor = q->hash_divisor;
	opt.flows = q->depth;
	opt.hash_kind = q->hash_kind;

	RTA_PUT(skb, TCA_OPTIONS, sizeof(opt), &opt);

	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static struct Qdisc_ops esfq_qdisc_ops =
{
	.next		=	NULL,
	.cl_ops		=	NULL,
	.id		=	"esfq",
	.priv_size	=	sizeof(struct esfq_sched_data),
	.enqueue	=	esfq_enqueue,
	.dequeue	=	esfq_dequeue,
	.requeue	=	esfq_requeue,
	.drop		=	esfq_drop,
	.init		=	esfq_init,
	.reset		=	esfq_reset,
	.destroy	=	esfq_destroy,
	.change		=	NULL, /* esfq_change - needs more work */
	.dump		=	esfq_dump,
//	.owner		=	THIS_MODULE,
};

static int __init esfq_module_init(void)
{
	return register_qdisc(&esfq_qdisc_ops);
}
static void __exit esfq_module_exit(void)
{
	unregister_qdisc(&esfq_qdisc_ops);
}
module_init(esfq_module_init)
module_exit(esfq_module_exit)
MODULE_LICENSE("GPL");
