/* main.c - Application main entry point */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <misc/printk.h>

#include <net/buf.h>

struct bt_data {
	void *hci_sync;

	union {
		uint16_t hci_opcode;
		uint16_t acl_handle;
	};

	uint8_t type;
};

static int destroy_called;

static struct nano_fifo bufs_fifo;

static void buf_destroy(struct net_buf *buf)
{
	destroy_called++;

	if (buf->free != &bufs_fifo) {
		printk("Invalid free pointer in buffer!\n");
	}

	printk("destroying %p\n", buf);

	nano_fifo_put(buf->free, buf);
}

static NET_BUF_POOL(bufs_pool, 22, 74, &bufs_fifo, buf_destroy,
		    sizeof(struct bt_data));

static bool net_buf_test_1(void)
{
	struct net_buf *bufs[ARRAY_SIZE(bufs_pool)];
	struct net_buf *buf;
	int i;

	for (i = 0; i < ARRAY_SIZE(bufs_pool); i++) {
		buf = net_buf_get_timeout(&bufs_fifo, 0, TICKS_NONE);
		if (!buf) {
			printk("Failed to get buffer!\n");
			return false;
		}

		bufs[i] = buf;
	}

	for (i = 0; i < ARRAY_SIZE(bufs_pool); i++) {
		net_buf_unref(bufs[i]);
	}

	if (destroy_called != ARRAY_SIZE(bufs_pool)) {
		printk("Incorrect destroy callback count: %d\n",
		       destroy_called);
		return false;
	}

	return true;
}

static bool net_buf_test_2(void)
{
	struct net_buf *frag, *head;
	struct nano_fifo fifo;
	int i;

	head = net_buf_get_timeout(&bufs_fifo, 0, TICKS_NONE);
	if (!head) {
		printk("Failed to get fragment list head!\n");
		return false;
	}

	printk("Fragment list head %p\n", head);

	frag = head;
	for (i = 0; i < ARRAY_SIZE(bufs_pool) - 1; i++) {
		frag->frags = net_buf_get_timeout(&bufs_fifo, 0, TICKS_NONE);
		if (!frag->frags) {
			printk("Failed to get fragment!\n");
			return false;
		}
		printk("%p -> %p\n", frag, frag->frags);
		frag = frag->frags;
	}

	printk("%p -> %p\n", frag, frag->frags);

	nano_fifo_init(&fifo);
	net_buf_put(&fifo, head);
	head = net_buf_get_timeout(&fifo, 0, TICKS_NONE);

	destroy_called = 0;
	net_buf_unref(head);

	if (destroy_called != ARRAY_SIZE(bufs_pool)) {
		printk("Incorrect fragment destroy callback count: %d\n",
		       destroy_called);
		return false;
	}

	return true;
}

static const struct {
	const char *name;
	bool (*func)(void);
} tests[] = {
	{ "Test 1", net_buf_test_1, },
	{ "Test 2", net_buf_test_2, },
};

void main(void)
{
	int count, pass;

	printk("sizeof(struct net_buf) = %u\n", sizeof(struct net_buf));
	printk("sizeof(bufs_pool)      = %u\n", sizeof(bufs_pool));

	net_buf_pool_init(bufs_pool);

	for (count = 0, pass = 0; count < ARRAY_SIZE(tests); count++) {
		if (!tests[count].func()) {
			printk("%s failed!\n", tests[count].name);
		} else {
			pass++;
		}
	}

	printk("%d / %d tests passed\n", pass, count);
}
