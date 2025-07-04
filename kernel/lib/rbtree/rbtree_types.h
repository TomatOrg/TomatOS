/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
  Copied from Linux kernel commit baaa2567a712d449bbaabc7e923c4d972f67cae1.
*/
#ifndef _LINUX_RBTREE_TYPES_H
#define _LINUX_RBTREE_TYPES_H

typedef struct rb_node {
	unsigned long  __rb_parent_color;
	struct rb_node* rb_right;
	struct rb_node* rb_left;
} rb_node_t;
/* The alignment might seem pointless, but allegedly CRIS needs it */

typedef struct rb_root {
	rb_node_t* rb_node;
} rb_root_t;

/*
 * Leftmost-cached rbtrees.
 *
 * We do not cache the rightmost node based on footprint
 * size vs number of potential users that could benefit
 * from O(1) rb_last(). Just not worth it, users that want
 * this feature can always implement the logic explicitly.
 * Furthermore, users that want to cache both pointers may
 * find it a bit asymmetric, but that's ok.
 */
typedef struct rb_root_cached {
	rb_root_t rb_root;
	rb_node_t* rb_leftmost;
} rb_root_cached_t;

#define RB_ROOT (struct rb_root) { NULL, }
#define RB_ROOT_CACHED (struct rb_root_cached) { {NULL, }, NULL }

#endif