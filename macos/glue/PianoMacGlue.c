/*
 * Board hooks the macOS 27.3 kernel patches call. Piano has no verified
 * root storage on macOS yet, so no root is accepted and exit tracing is off.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <stdint.h>

/* macos-owner-root-authenticated-open.patch: set once a verified root is mounted. */
volatile int native_owner_root_accepted;
volatile int32_t native_owner_root_mount_device = -1;	/* NODEV */

/* macos-droidvm-k27-parity.patch exit-trace: user frames of a listed process's exit(). */
void native_exit_trace(int pid, const char *name, int status)
{
	(void)pid; (void)name; (void)status;
}
