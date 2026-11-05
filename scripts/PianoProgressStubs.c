/*
 * BootProgress references these 13T storage and stall-probe values; piano
 * has no UFS driver yet, so they stay zero (off).
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <stdint.h>

uint32_t pagani_stall_on;

/* Statistics of 13T drivers piano does not build. */
volatile uint32_t native_adreno_pool_stats[8];

/* Stall-probe hooks of the 13T kernel patches; unused while pagani_stall_on is 0. */
struct boot_progress_stall_vm;
int boot_progress_stall_xcall(unsigned cpu, void (*function)(void *), void *parameter) { (void)cpu; (void)function; (void)parameter; return 0; }
unsigned boot_progress_stall_thread_times(void *task, uint64_t *tids, uint64_t *times, unsigned max) { (void)task; (void)tids; (void)times; (void)max; return 0; }
void boot_progress_stall_vm_read(struct boot_progress_stall_vm *out) { (void)out; }
struct boot_progress_stall_thread;
int boot_progress_stall_thread_read(void *thread, struct boot_progress_stall_thread *out) { (void)thread; (void)out; return 0; }
void *boot_progress_stall_thread_find(void *task, const char *name, uint64_t tid) { (void)task; (void)name; (void)tid; return 0; }
void boot_progress_stall_thread_release(void *thread) { (void)thread; }

struct boot_progress_crash_image;
struct boot_progress_stall_cpu;
struct boot_progress_stall_mem;
struct boot_progress_stall_regs;
struct boot_progress_stall_task;
uint32_t boot_progress_stall_abi(void) {  return 0; }
void boot_progress_stall_cpu_read(unsigned cpu, struct boot_progress_stall_cpu *out) { (void)cpu; (void)out; }
int32_t boot_progress_stall_foreground(const char *not_a, const char *not_b, uint32_t *count) { (void)not_a; (void)not_b; (void)count; return 0; }
int boot_progress_stall_image(void *task, uint64_t address, struct boot_progress_crash_image *image) { (void)task; (void)address; (void)image; return 0; }
void *boot_progress_stall_map(void *task) { (void)task; return 0; }
int boot_progress_stall_map_read(void *map, uint64_t address, void *buffer, uint32_t bytes) { (void)map; (void)address; (void)buffer; (void)bytes; return 0; }
void boot_progress_stall_map_release(void *map) { (void)map; }
void boot_progress_stall_mem_read(struct boot_progress_stall_mem *out) { (void)out; }
int boot_progress_stall_regs_here(void *thread, struct boot_progress_stall_regs *out) { (void)thread; (void)out; return 0; }
void *boot_progress_stall_task_find(const char *name, int32_t pid, struct boot_progress_stall_task *out) { (void)name; (void)pid; (void)out; return 0; }
void boot_progress_stall_task_release(void *task) { (void)task; }

/* GPU frame hint of the 13T Adreno 830 driver; no GPU driver on piano yet. */
uint32_t adreno830_cpu_hint(void) { return 0; }

/* Optional sheng launchd watchdog policy after the Data mount; the 13T
 * drops the call, piano does nothing. */
int sheng_boot_watchdog_policy(void *proc, unsigned long long scratch)
{
	(void)proc; (void)scratch;
	return 0;
}

/* No Adreno830 GPU driver on piano yet: GPU tracing is off. */
struct NativeGpuPixelPiece;
int native_gpu_trace_enabled(void) { return 0; }
void native_gpu_trace_record(unsigned event, unsigned pid, const unsigned long long *values, unsigned count)
{ (void)event; (void)pid; (void)values; (void)count; }
int native_gpu_pixel_put(const struct NativeGpuPixelPiece *piece, unsigned pid) { (void)piece; (void)pid; return 1; }
