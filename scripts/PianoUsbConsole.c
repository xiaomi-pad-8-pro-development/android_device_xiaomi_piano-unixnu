/*
 * USB serial console for piano test images.
 *
 * Takes the SM8750 DWC3 controller (usb@a600000) over in device mode and
 * presents a single CDC-ACM port (VID 0x1209, PID 0x0001), which a Mac shows
 * as /dev/cu.usbmodem*. Everything the kernel prints (the msgbuf) is copied to
 * that port while the host holds it open (DTR set).
 *
 * The PHYs and the eUSB2 repeater are left as the bootloader's fastboot left
 * them: nothing here writes the PMIC. The controller is reset, put in device
 * mode, fed a forced VBUS/session through QSCRATCH and driven by polling (no
 * interrupt). Its stream (SID 0x40) is routed around the apps SMMU so the DMA
 * addresses are physical.
 *
 * Started from bsd_init by patches/xnu/piano-usb-console.patch (test images
 * only); every step is reported on the boot screen as "usbc: ...".
 *
 * Programming model: Synopsys DWC_usb3x databook (as used by Linux
 * drivers/usb/dwc3/{core,gadget,ep0}.c and dwc3-qcom.c), USB 2.0 chapter 9,
 * USB CDC 1.2 PSTN subclass. Register offsets for SM8750 from gcc-sm8750.c.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */
#include <stdint.h>
#include <stddef.h>

typedef uint64_t vm_offset_t;
typedef int kern_return_t;
typedef void *thread_t;
typedef int wait_result_t;

extern vm_offset_t ml_io_map(uint64_t phys, vm_offset_t size);
extern void *IOMallocContiguous(vm_offset_t size, vm_offset_t alignment, uint64_t *phys);
extern uint64_t kvtophys(vm_offset_t va);
extern void flush_dcache64(uint64_t addr, unsigned count, int phys);
extern void IOSleep(unsigned int ms);
extern void IODelay(unsigned int us);
extern int PE_parse_boot_argn(const char *, void *, int);
extern int snprintf(char *, unsigned long, const char *, ...);
extern int printf(const char *, ...);
extern void boot_progress_stage(const char *);
extern kern_return_t kernel_thread_start(void (*)(void *, wait_result_t), void *, thread_t *);
extern void thread_deallocate(thread_t);

struct usbc_msgbuf {
	int msg_magic, msg_size, msg_bufx, msg_bufr;
	char *msg_bufc;
};
extern struct usbc_msgbuf *msgbufp;

static void
say(const char *fmt, uint64_t a, uint64_t b)
{
	char line[96];
	snprintf(line, sizeof line, fmt, a, b);
	printf("%s\n", line);
	boot_progress_stage(line);
}

/* ---- hardware ---------------------------------------------------------- */

#define GCC_BASE              0x00100000ull
#define GCC_USB30_PRIM_GDSCR  0x39004
#define GCC_USB30_MASTER_CBCR 0x39018
#define GCC_USB30_SLEEP_CBCR  0x39028
#define GCC_USB30_UTMI_CBCR   0x3902c
#define GCC_CFG_NOC_USB3_AXI  0x3908c
#define GCC_AGGRE_USB3_AXI    0x39090

#define USB_BASE              0x0a600000ull
#define USB_SIZE              0x00100000ull
#define QS                    0xf8800        /* QSCRATCH inside the wrapper */
#define QS_GENERAL_CFG        (QS + 0x08)
#define QS_HS_PHY_CTRL        (QS + 0x10)
#define QS_SS_PHY_CTRL        (QS + 0x30)

#define GCTL         0xc110
#define GSNPSID      0xc120
#define GUSB2PHYCFG  0xc200
#define GUSB3PIPECTL 0xc2c0
#define GEVNTADRLO   0xc400
#define GEVNTADRHI   0xc404
#define GEVNTSIZ     0xc408
#define GEVNTCOUNT   0xc40c
#define DCFG         0xc700
#define DCTL         0xc704
#define DEVTEN       0xc708
#define DSTS         0xc70c
#define DALEPENA     0xc720
#define DEPCMDPAR2(n) (0xc800 + (n) * 0x10)
#define DEPCMDPAR1(n) (0xc804 + (n) * 0x10)
#define DEPCMDPAR0(n) (0xc808 + (n) * 0x10)
#define DEPCMD(n)     (0xc80c + (n) * 0x10)

#define DCTL_RUN_STOP   (1u << 31)
#define DCTL_CSFTRST    (1u << 30)
#define DSTS_DEVCTRLHLT (1u << 22)

#define CMD_SETEPCONFIG    1
#define CMD_SETTRANSFRESCONFIG 2
#define CMD_SETSTALL       4
#define CMD_CLEARSTALL     5
#define CMD_STARTTRANSFER  6
#define CMD_ENDTRANSFER    8
#define CMD_DEPSTARTCFG    9
#define CMD_ACT            (1u << 10)
#define CMD_CMDIOC         (1u << 8)

#define TRB_HWO   (1u << 0)
#define TRB_LST   (1u << 1)
#define TRB_ISP   (1u << 10)
#define TRB_IOC   (1u << 11)
#define TRBCTL_NORMAL    (1u << 4)
#define TRBCTL_SETUP     (2u << 4)
#define TRBCTL_STATUS2   (3u << 4)
#define TRBCTL_STATUS3   (4u << 4)
#define TRBCTL_DATA      (5u << 4)

#define SMMU_BASE 0x15000000ull
#define SMMU_SID  0x40

static volatile uint32_t *gcc, *usb;

/*
 * Progress for unattended tests: the test autoreset waits 40 s longer per
 * step, so the host reads how far the console got from when Android returns.
 */
volatile unsigned piano_diag_step;
static void
step(unsigned n)
{
	if (n <= piano_diag_step) {
		return;
	}
	piano_diag_step = n;
	/*
	 * Unattended tests: 15 s at every new bring-up step, so a crash that
	 * resets the board early still tells the host (by when Android returns)
	 * the last step that completed. Not from step 8 on: those come from bus
	 * events, and the host gives up on a control transfer after 5 s.
	 */
	/* piano-usbc-step-sleep=1 brings back 15 s per bring-up step for crash bisects. */
	unsigned slow = 0;
	PE_parse_boot_argn("piano-usbc-step-sleep", &slow, sizeof(slow));
	if (slow && n < 8) {
		IOSleep(15000);
	}
}

static uint32_t rd(uint32_t off) { return usb[off / 4]; }
static void wr(uint32_t off, uint32_t v) { usb[off / 4] = v; }

static int
wait_bits(uint32_t off, uint32_t mask, uint32_t want, unsigned ms)
{
	for (unsigned i = 0; i < ms * 10; i++) {
		if ((rd(off) & mask) == want) {
			return 1;
		}
		IODelay(100);
	}
	return 0;
}

/* ---- DMA memory: one physically contiguous block, CPU view uncached ---- */

#define DMA_SIZE   0x10000
#define OFF_EVT    0x0000   /* 4 KiB event buffer */
#define OFF_EP0TRB 0x1000   /* EP0 TRBs */
#define OFF_EP0BUF 0x1100   /* 512 B control data */
#define OFF_INTRB  0x1400   /* bulk IN TRB */
#define OFF_OUTTRB 0x1440   /* bulk OUT TRB */
#define OFF_OUTBUF 0x1800   /* 512 B bulk OUT */
#define OFF_INBUF  0x2000   /* 16 KiB bulk IN */
#define EVT_SIZE   0x1000
#define IN_MAX     0x4000

static uint64_t dma_phys;
static volatile uint8_t *dma;

struct trb { uint32_t bpl, bph, size, ctrl; };

/* write back and drop the CPU's cached copy of the whole DMA block */
static void
dma_sync(void)
{
	__asm__ volatile ("dsb sy" ::: "memory");
	flush_dcache64((uint64_t)(uintptr_t)dma, DMA_SIZE, 0);
	__asm__ volatile ("dsb sy" ::: "memory");
}

static volatile struct trb *
trb_at(uint32_t off)
{
	return (volatile struct trb *)(dma + off);
}

/* ---- descriptors -------------------------------------------------------- */

static const uint8_t dev_desc[18] = {
	18, 1, 0x00, 0x02, 0x02, 0x00, 0x00, 64,
	0x09, 0x12, 0x01, 0x00, 0x00, 0x01, 1, 2, 3, 1,
};
static const uint8_t qual_desc[10] = { 10, 6, 0x00, 0x02, 0x02, 0x00, 0x00, 64, 1, 0 };
static const uint8_t cfg_desc[67] = {
	9, 2, 67, 0, 2, 1, 0, 0x80, 50,
	/* interface 0: CDC communication, ACM */
	9, 4, 0, 0, 1, 0x02, 0x02, 0x01, 0,
	5, 0x24, 0x00, 0x10, 0x01,          /* header */
	5, 0x24, 0x01, 0x00, 0x01,          /* call management */
	4, 0x24, 0x02, 0x02,                /* ACM: line coding + state */
	5, 0x24, 0x06, 0x00, 0x01,          /* union 0 -> 1 */
	7, 5, 0x83, 0x03, 8, 0, 9,          /* EP3 IN interrupt */
	/* interface 1: CDC data */
	9, 4, 1, 0, 2, 0x0a, 0x00, 0x00, 0,
	7, 5, 0x01, 0x02, 0x00, 0x02, 0,    /* EP1 OUT bulk 512 */
	7, 5, 0x82, 0x02, 0x00, 0x02, 0,    /* EP2 IN bulk 512 */
};
static const char *const strings[] = { "", "piano", "XNU console", "PIANOXNU1" };

/* physical endpoints: 0 = EP0 OUT, 1 = EP0 IN, 2 = EP1 OUT, 5 = EP2 IN, 7 = EP3 IN */
#define PEP_BULK_OUT 2
#define PEP_BULK_IN  5
#define PEP_INTR_IN  7

/* ---- endpoint commands -------------------------------------------------- */

static uint32_t rsc[8];

static int
ep_cmd(unsigned pep, uint32_t cmd, uint32_t p0, uint32_t p1, uint32_t p2)
{
	wr(DEPCMDPAR0(pep), p0);
	wr(DEPCMDPAR1(pep), p1);
	wr(DEPCMDPAR2(pep), p2);
	wr(DEPCMD(pep), cmd | CMD_ACT);
	if (!wait_bits(DEPCMD(pep), CMD_ACT, 0, 50)) {
		say("usbc: ep %llu cmd %llx timeout", pep, cmd);
		return -1;
	}
	uint32_t v = rd(DEPCMD(pep));
	if (cmd == CMD_STARTTRANSFER) {
		rsc[pep] = (v >> 16) & 0x7f;
	}
	return (v >> 12) & 0xf;
}

static void
ep_config(unsigned pep, unsigned type, unsigned maxp, unsigned interval, int modify)
{
	uint32_t p0 = (type << 1) | (maxp << 3);
	if (pep & 1) {
		p0 |= (pep >> 1) << 17;          /* TX FIFO number = EP number */
	}
	if (modify) {
		p0 |= 2u << 30;
	}
	uint32_t p1 = (1u << 8) | (1u << 10) | (pep << 25);   /* XferComplete, XferNotReady */
	if (type == 3 && interval) {
		p1 |= (interval - 1) << 16;
	}
	ep_cmd(pep, CMD_SETEPCONFIG, p0, p1, 0);
	if (!modify) {
		ep_cmd(pep, CMD_SETTRANSFRESCONFIG, 1, 0, 0);
	}
}

static void
start_trb(unsigned pep, uint32_t trboff, uint32_t bufoff, uint32_t len, uint32_t ctl)
{
	volatile struct trb *t = trb_at(trboff);
	uint64_t buf = dma_phys + bufoff, at = dma_phys + trboff;
	t->bpl = (uint32_t)buf;
	t->bph = (uint32_t)(buf >> 32);
	t->size = len;
	t->ctrl = ctl | TRB_LST | TRB_IOC | TRB_ISP | TRB_HWO;
	dma_sync();
	ep_cmd(pep, CMD_STARTTRANSFER, (uint32_t)(at >> 32), (uint32_t)at, 0);
}

/* ---- control endpoint --------------------------------------------------- */

enum { EP0_SETUP, EP0_DATA, EP0_WAIT_STATUS, EP0_STATUS };
static int ep0_state, ep0_in, ep0_three, configured, dtr, pending_addr = -1;
static uint8_t line_coding[7] = { 0x00, 0xc2, 0x01, 0x00, 0, 0, 8 };   /* 115200 8N1 */
static int line_coding_out;

static void
ep0_arm_setup(void)
{
	ep0_state = EP0_SETUP;
	start_trb(0, OFF_EP0TRB, OFF_EP0BUF, 8, TRBCTL_SETUP);
}

static void
ep0_stall(void)
{
	ep_cmd(0, CMD_SETSTALL, 0, 0, 0);
	ep0_arm_setup();
}

static void
ep0_reply(const void *src, uint32_t len, uint32_t wlength)
{
	if (len > wlength) {
		len = wlength;
	}
	for (uint32_t i = 0; i < len; i++) {
		dma[OFF_EP0BUF + 0x100 + i] = ((const uint8_t *)src)[i];
	}
	ep0_in = 1;
	ep0_state = EP0_DATA;
	start_trb(1, OFF_EP0TRB + 0x10, OFF_EP0BUF + 0x100, len, TRBCTL_DATA);
}

static void
bulk_out_arm(void)
{
	start_trb(PEP_BULK_OUT, OFF_OUTTRB, OFF_OUTBUF, 512, TRBCTL_NORMAL);
}

static void
set_configuration(void)
{
	ep_cmd(0, CMD_DEPSTARTCFG | (2u << 16), 0, 0, 0);
	ep_config(PEP_BULK_OUT, 2, 512, 0, 0);
	ep_config(PEP_BULK_IN, 2, 512, 0, 0);
	ep_config(PEP_INTR_IN, 3, 8, 9, 0);
	wr(DALEPENA, rd(DALEPENA) | (1u << PEP_BULK_OUT) | (1u << PEP_BULK_IN) | (1u << PEP_INTR_IN));
	bulk_out_arm();
	configured = 1;
	step(11);
}

static uint32_t
string_desc(unsigned idx, uint8_t *out)
{
	if (idx == 0) {
		out[0] = 4; out[1] = 3; out[2] = 0x09; out[3] = 0x04;
		return 4;
	}
	if (idx >= sizeof strings / sizeof strings[0]) {
		return 0;
	}
	const char *s = strings[idx];
	uint32_t n = 0;
	while (s[n] && n < 30) {
		out[2 + 2 * n] = (uint8_t)s[n];
		out[3 + 2 * n] = 0;
		n++;
	}
	out[0] = (uint8_t)(2 + 2 * n);
	out[1] = 3;
	return out[0];
}

static void
ep0_setup(void)
{
	uint8_t s[8];
	for (int i = 0; i < 8; i++) {
		s[i] = dma[OFF_EP0BUF + i];
	}
	uint8_t type = s[0], req = s[1];
	uint16_t value = s[2] | (s[3] << 8), index = s[4] | (s[5] << 8), length = s[6] | (s[7] << 8);
	(void)index;
	ep0_three = length != 0;
	ep0_in = (type & 0x80) != 0;

	if ((type & 0x60) == 0x00) {             /* standard */
		switch (req) {
		case 0x06: {                     /* GET_DESCRIPTOR */
			uint8_t sd[64];
			switch (value >> 8) {
			case 1: ep0_reply(dev_desc, sizeof dev_desc, length); return;
			case 2: ep0_reply(cfg_desc, sizeof cfg_desc, length); return;
			case 3: {
				uint32_t n = string_desc(value & 0xff, sd);
				if (n) { ep0_reply(sd, n, length); return; }
				break;
			}
			case 6: ep0_reply(qual_desc, sizeof qual_desc, length); return;
			}
			ep0_stall();
			return;
		}
		case 0x05:                       /* SET_ADDRESS: DevAddr in the setup stage */
			wr(DCFG, (rd(DCFG) & ~(0x7fu << 3)) | ((value & 0x7f) << 3));
			pending_addr = value & 0x7f;
			break;
		case 0x09:                       /* SET_CONFIGURATION */
			if (value == 1 && !configured) {
				set_configuration();
			}
			break;
		case 0x00: {                     /* GET_STATUS */
			static const uint8_t zero[2] = { 0, 0 };
			ep0_reply(zero, 2, length);
			return;
		}
		case 0x08: {                     /* GET_CONFIGURATION */
			uint8_t c = configured ? 1 : 0;
			ep0_reply(&c, 1, length);
			return;
		}
		case 0x0b: case 0x01: case 0x03: /* SET_INTERFACE, CLEAR/SET_FEATURE */
			break;
		default:
			ep0_stall();
			return;
		}
	} else if ((type & 0x60) == 0x20) {      /* class (CDC ACM) */
		switch (req) {
		case 0x20:                       /* SET_LINE_CODING: 7 bytes OUT */
			line_coding_out = 1;
			ep0_state = EP0_DATA;
			start_trb(0, OFF_EP0TRB + 0x20, OFF_EP0BUF + 0x100, 512, TRBCTL_DATA);
			return;
		case 0x21:                       /* GET_LINE_CODING */
			ep0_reply(line_coding, sizeof line_coding, length);
			return;
		case 0x22:                       /* SET_CONTROL_LINE_STATE */
			if (!dtr && (value & 1)) {
				say("usbc: host opened the port", 0, 0);
				step(12);
			}
			dtr = value & 1;
			break;
		default:
			ep0_stall();
			return;
		}
	} else {
		ep0_stall();
		return;
	}
	/* two-stage: wait for XferNotReady (status) on EP0 IN */
	ep0_state = EP0_WAIT_STATUS;
}

static void
ep0_start_status(void)
{
	/* status goes the opposite way of the data stage; IN when there was none */
	unsigned pep = ep0_three ? (ep0_in ? 0 : 1) : 1;
	ep0_state = EP0_STATUS;
	start_trb(pep, OFF_EP0TRB + 0x30, OFF_EP0BUF + 0x180, 0,
	    ep0_three ? TRBCTL_STATUS3 : TRBCTL_STATUS2);
}

/* ---- bulk IN: the kernel log ------------------------------------------- */

static int in_busy;
static int log_pos = -1;

static void
log_pump(void)
{
	if (!configured || !dtr || in_busy || !msgbufp || !msgbufp->msg_bufc) {
		return;
	}
	int size = msgbufp->msg_size, end = msgbufp->msg_bufx;
	if (size <= 0 || end < 0 || end >= size) {
		return;
	}
	if (log_pos < 0) {
		/* the whole ring, oldest first: after a wrap that is just past the write index */
		log_pos = (end + 1) % size;
	}
	uint32_t n = 0;
	while (log_pos != end && n < IN_MAX) {
		char c = msgbufp->msg_bufc[log_pos];
		if (c) {                         /* the unused part of a ring that has not wrapped is zero */
			dma[OFF_INBUF + n++] = (uint8_t)c;
		}
		if (++log_pos >= size) {
			log_pos = 0;
		}
	}
	if (n) {
		in_busy = 1;
		start_trb(PEP_BULK_IN, OFF_INTRB, OFF_INBUF, n, TRBCTL_NORMAL);
	}
}

/* ---- events --------------------------------------------------------------- */

static uint32_t evt_pos;

static void
device_event(uint32_t ev)
{
	switch ((ev >> 8) & 0xf) {
	case 0:                                  /* disconnect */
		configured = dtr = in_busy = 0;
		say("usbc: disconnect", 0, 0);
		break;
	case 1:                                  /* bus reset */
		configured = dtr = in_busy = 0;
		wr(DCFG, rd(DCFG) & ~(0x7fu << 3));
		wr(DALEPENA, 3);
		say("usbc: bus reset", 0, 0);
		step(8);
		break;
	case 2: {                                /* connect done */
		uint32_t speed = rd(DSTS) & 7;
		ep_config(0, 0, 64, 0, 1);
		ep_config(1, 0, 64, 0, 1);
		say("usbc: connected, speed %llu", speed, 0);
		step(9);
		break;
	}
	}
}

static void
endpoint_event(uint32_t ev)
{
	unsigned pep = (ev >> 1) & 0x1f, kind = (ev >> 6) & 0xf, status = (ev >> 12) & 0xf;
	if (pep <= 1) {
		if (kind == 1) {                 /* XferComplete */
			if (ep0_state == EP0_SETUP) {
				ep0_setup();
			} else if (ep0_state == EP0_DATA) {
				if (line_coding_out) {
					for (int i = 0; i < 7; i++) {
						line_coding[i] = dma[OFF_EP0BUF + 0x100 + i];
					}
					line_coding_out = 0;
				}
				ep0_state = EP0_WAIT_STATUS;
			} else if (ep0_state == EP0_STATUS) {
				if (pending_addr >= 0) {
					say("usbc: address %llu", (uint64_t)pending_addr, 0);
					step(10);
					pending_addr = -1;
				}
				ep0_arm_setup();
			}
		} else if (kind == 3 && ep0_state == EP0_WAIT_STATUS && (status & 2)) {
			ep0_start_status();      /* XferNotReady, control status phase */
		}
		return;
	}
	if (kind != 1) {
		return;
	}
	if (pep == PEP_BULK_IN) {
		in_busy = 0;
	} else if (pep == PEP_BULK_OUT) {
		/*
		 * "REBOOT" from the host resets the board (PSCI SYSTEM_RESET), so an
		 * unattended test can end a run without anyone at the buttons.
		 */
		dma_sync();
		const volatile char *b = (const volatile char *)(dma + OFF_OUTBUF);
		if (b[0] == 'R' && b[1] == 'E' && b[2] == 'B' && b[3] == 'O' && b[4] == 'O' && b[5] == 'T') {
			say("usbc: reboot requested by host", 0, 0);
			IOSleep(200);
			register uint64_t x0 __asm__("x0") = 0x84000009;
			__asm__ volatile ("smc #0" : "+r"(x0) : : "x1", "x2", "x3", "memory");
		}
		bulk_out_arm();
	}
}

static void
poll_events(void)
{
	uint32_t count = rd(GEVNTCOUNT) & 0xffff;
	if (count >= 4) {
		dma_sync();
	}
	while (count >= 4) {
		uint32_t ev = *(volatile uint32_t *)(dma + OFF_EVT + evt_pos);
		evt_pos = (evt_pos + 4) % EVT_SIZE;
		if (ev & 1) {
			device_event(ev);
		} else {
			endpoint_event(ev);
		}
		wr(GEVNTCOUNT, 4);
		count -= 4;
	}
}

/* ---- bring-up ------------------------------------------------------------- */

static int
power_on(void)
{
	gcc = (volatile uint32_t *)ml_io_map(GCC_BASE + 0x39000, 0x1000);
	uint32_t gdsc = gcc[0x004 / 4], master = gcc[0x018 / 4];
	say("usbc: gdsc %llx master %llx", gdsc, master);
	if (!(gdsc & 0x80000000u)) {
		gcc[0x004 / 4] = gdsc & ~1u;     /* SW_COLLAPSE off */
		for (int i = 0; i < 1000 && !(gcc[0x004 / 4] & 0x80000000u); i++) {
			IODelay(10);
		}
	}
	static const uint32_t cbcr[] = { 0x018, 0x028, 0x02c, 0x08c, 0x090 };
	for (unsigned i = 0; i < sizeof cbcr / sizeof cbcr[0]; i++) {
		gcc[cbcr[i] / 4] |= 1u;          /* CLK_ENABLE */
	}
	IODelay(100);
	gdsc = gcc[0x004 / 4];
	master = gcc[0x018 / 4];
	say("usbc: now gdsc %llx master %llx", gdsc, master);
	return (gdsc & 0x80000000u) && !(master & 0x80000000u);
}

/*
 * Route SID 0x40 through a context bank with translation off, the way
 * NativeUfs routes the UFS stream (Sm8750Platform/SmmuTakeover.h): S2CR type
 * "translate" to a free bank whose CBAR is "stage 1, stage 2 bypass", CBA2R
 * VA64 and SCTLR.M = 0. The S2CR "bypass" type is not used: writing it made
 * the first DMA reset the board.
 */
static void
smmu_bypass(void)
{
	volatile uint32_t *smmu = (volatile uint32_t *)ml_io_map(SMMU_BASE, 0x100000);
	uint32_t idr0 = smmu[0x20 / 4], idr1 = smmu[0x24 / 4];
	unsigned groups = idr0 & 0xff, banks = idr1 & 0xff;
	unsigned pages = 1u << (((idr1 >> 28) & 7) + 1);
	if (groups > 128) {
		groups = 128;
	}
	if (!groups || banks < 16 || (pages + banks) * 4096 > 0x100000) {
		say("usbc: smmu geometry %llx %llx", idr0, idr1);
		return;
	}
	/* the stream's existing entry (the bootloader's) and a free group */
	int ours = -1, free_slot = -1;
	unsigned used[4] = { 0, 0, 0, 0 };
	for (unsigned n = 0; n < groups; n++) {
		uint32_t smr = smmu[(0x800 + 4 * n) / 4], s2cr = smmu[(0xc00 + 4 * n) / 4];
		if (!(smr & 0x80000000u)) {
			if (free_slot < 0) {
				free_slot = (int)n;
			}
			continue;
		}
		if (((s2cr >> 16) & 3) == 0) {
			used[(s2cr & 0x7f) / 32] |= 1u << ((s2cr & 0x7f) % 32);
		}
		uint32_t mask = (smr >> 16) & 0x7fff;
		if (ours < 0 && (((smr ^ SMMU_SID) & 0xffff & ~mask) == 0)) {
			ours = (int)n;
		}
	}
	/* a bank no stream uses and with translation off, below the UFS banks */
	int bank = -1;
	for (int b = (int)banks - 9; b >= 1; b--) {
		if (used[b / 32] & (1u << (b % 32))) {
			continue;
		}
		if (smmu[((pages + b) * 4096) / 4] & 1) {
			continue;
		}
		bank = b;
		break;
	}
	if (bank < 0 || (ours < 0 && free_slot < 0)) {
		say("usbc: smmu no bank %llx slot %llx", (uint64_t)bank, (uint64_t)free_slot);
		return;
	}
	volatile uint32_t *cb = smmu + ((pages + bank) * 4096) / 4;
	cb[0] = 0;                                     /* SCTLR: translation off */
	smmu[(0x1800 + 4 * bank) / 4] = 1;             /* CBA2R: VA64 */
	smmu[(0x1000 + 4 * bank) / 4] = 0x10000;       /* CBAR: S1 context, S2 bypass */
	__asm__ volatile ("dsb sy" ::: "memory");
	int slot = ours >= 0 ? ours : free_slot;
	if (ours >= 0) {
		say("usbc: smmu slot %llu was s2cr %llx", (uint64_t)slot, smmu[(0xc00 + 4 * slot) / 4]);
	}
	smmu[(0xc00 + 4 * slot) / 4] = (uint32_t)bank;
	if (ours < 0) {
		smmu[(0x800 + 4 * slot) / 4] = 0x80000000u | SMMU_SID;
	}
	__asm__ volatile ("dsb sy" ::: "memory");
	say("usbc: smmu sid 40 slot %llu bank %llu", (uint64_t)slot, (uint64_t)bank);
}

static int
core_setup(void)
{
	usb = (volatile uint32_t *)ml_io_map(USB_BASE, USB_SIZE);
	uint32_t id = rd(GSNPSID);
	say("usbc: id %llx dsts %llx", id, rd(DSTS));
	if (id == 0 || id == 0xffffffffu) {
		return 0;
	}
	step(5);
	wr(DCTL, rd(DCTL) & ~DCTL_RUN_STOP);
	if (!wait_bits(DSTS, DSTS_DEVCTRLHLT, DSTS_DEVCTRLHLT, 100)) {
		say("usbc: halt timeout", 0, 0);
	}
	/* the stock firmware's USB shutdown can leave the PHY suspend bits set */
	wr(GUSB2PHYCFG, rd(GUSB2PHYCFG) & ~((1u << 6) | (1u << 8)));
	wr(GUSB3PIPECTL, rd(GUSB3PIPECTL) & ~(1u << 17));
	/* UTMI clock as the PIPE clock: the SuperSpeed PHY is not used */
	wr(QS_GENERAL_CFG, rd(QS_GENERAL_CFG) | (1u << 8));
	wr(QS_GENERAL_CFG, rd(QS_GENERAL_CFG) | (1u << 0) | (1u << 3));
	IODelay(100);
	wr(QS_GENERAL_CFG, rd(QS_GENERAL_CFG) & ~(1u << 8));

	wr(DCTL, DCTL_CSFTRST);
	int ok = 0;
	for (int i = 0; i < 10 && !ok; i++) {
		IOSleep(20);
		ok = !(rd(DCTL) & DCTL_CSFTRST);
	}
	if (!ok) {
		say("usbc: soft reset stuck", 0, 0);
		return 0;
	}
	step(6);
	wr(GUSB2PHYCFG, rd(GUSB2PHYCFG) & ~((1u << 6) | (1u << 8)));
	wr(GCTL, (rd(GCTL) & ~(3u << 12)) | (2u << 12));          /* device role */
	wr(DCFG, (rd(DCFG) & ~(7u | (0x7fu << 3))) | 0 /* high speed */);

	wr(GEVNTADRLO, (uint32_t)(dma_phys + OFF_EVT));
	wr(GEVNTADRHI, (uint32_t)((dma_phys + OFF_EVT) >> 32));
	wr(GEVNTSIZ, EVT_SIZE | (1u << 31));                        /* interrupt masked: polled */
	wr(GEVNTCOUNT, rd(GEVNTCOUNT) & 0xffff);
	evt_pos = 0;
	wr(DEVTEN, (1u << 0) | (1u << 1) | (1u << 2));

	ep_cmd(0, CMD_DEPSTARTCFG, 0, 0, 0);
	ep_config(0, 0, 64, 0, 0);
	ep_config(1, 0, 64, 0, 0);
	wr(DALEPENA, 3);
	ep0_arm_setup();

	/* forced VBUS valid and session: there is no PMIC VBUS notification here */
	wr(QS_HS_PHY_CTRL, rd(QS_HS_PHY_CTRL) | (1u << 20) | (1u << 28));
	wr(QS_SS_PHY_CTRL, rd(QS_SS_PHY_CTRL) | (1u << 24));

	wr(DCTL, rd(DCTL) | DCTL_RUN_STOP);
	if (!wait_bits(DSTS, DSTS_DEVCTRLHLT, 0, 100)) {
		say("usbc: run timeout", 0, 0);
	}
	say("usbc: running dctl %llx dsts %llx", rd(DCTL), rd(DSTS));
	step(7);
	return 1;
}

static void
usbc_thread(void *arg, wait_result_t wr_)
{
	(void)arg; (void)wr_;
	say("usbc: start", 0, 0);
	step(1);
	/*
	 * Contiguous kernel memory without the below-4 GiB restriction of the
	 * physical-address form of IOMallocContiguous. The CPU uses its normal
	 * cached view (an uncached alias of RAM through ml_io_map stopped the boot
	 * at setconf); dma_sync() cleans and invalidates the block around every
	 * hand-over, and the controller's stream bypasses the SMMU.
	 */
	void *mem = IOMallocContiguous(DMA_SIZE, 0x1000, (uint64_t *)0);
	if (!mem) {
		say("usbc: no DMA memory", 0, 0);
		return;
	}
	dma_phys = kvtophys((vm_offset_t)mem);
	if (!dma_phys || (dma_phys & 0xfff)) {
		say("usbc: bad DMA address %llx", dma_phys, 0);
		return;
	}
	dma = (volatile uint8_t *)mem;
	for (uint32_t i = 0; i < DMA_SIZE; i++) {
		dma[i] = 0;
	}
	dma_sync();
	say("usbc: dma %llx", dma_phys, 0);
	step(2);
	if (!power_on()) {
		say("usbc: controller not powered", 0, 0);
		return;
	}
	step(3);
	smmu_bypass();
	step(4);
	if (!core_setup()) {
		return;
	}
	for (;;) {
		poll_events();
		log_pump();
		IOSleep(configured ? 2 : 20);
	}
}

void piano_usb_console_start(void);
void
piano_usb_console_start(void)
{
	thread_t thread;
	if (kernel_thread_start(usbc_thread, (void *)0, &thread) == 0) {
		thread_deallocate(thread);
	}
}
