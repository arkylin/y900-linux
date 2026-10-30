/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_Y900_TRACE_H
#define _LINUX_Y900_TRACE_H

#include <linux/printk.h>

extern int y900_tracing;

#define y900_pr(fmt, ...)						\
	do {								\
		if (y900_tracing)				\
			pr_info(fmt, ##__VA_ARGS__);			\
	} while (0)

#endif /* _LINUX_Y900_TRACE_H */
