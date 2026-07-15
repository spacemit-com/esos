#include <rthw.h>
#include <drivers/pm.h>
#include <rtconfig.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <riscv_sleep.h>
#include <riscv-ops.h>
#include <riscv_encoding.h>
#include <clint.h>
#include <stdlib.h>
#include <riscv-plic.h>
#include <spacemit_sdk_soc.h>
#include <register_defination.h>
#include "../rpmi/k3/k3_hsm.h"

static rt_sem_t rt_lowpwrsem;
static rt_thread_t rt_lowpwrtid;
static rt_device_t lpmdev;
static int pm_enter_flag;
static struct mbox_client lpm_tx_client, lpm_rx_client;
static struct mbox_chan *lpm_tx_chan, *lpm_rx_chan;

extern unsigned long __esos_lite_start[], __esos_lite_end[];
extern void rt_system_power_manager(void);

static void __core1_enter_wfi(rt_ubase_t entry)
{
	unsigned int val;

	/* tell rcpu0 that i will power down */
	mbox_send_message(lpm_tx_chan, &val);
	mbox_chan_txdone(lpm_tx_chan, 0);

	writel(entry & 0xffffffff, (void *)RCPU_CORE1_BOOT_ENTRY_LO);
	writel((entry >> 32) & 0xffffffff, (void *)RCPU_CORE1_BOOT_ENTRY_HI);

	val = readl((unsigned int *)RT24_CORE1_IDLE_CFG_REG);
	val |= 0x3;
	writel(val, (unsigned int *)RT24_CORE1_IDLE_CFG_REG);

	asm volatile ("fence iorw, iorw");
	asm volatile ("fence");

	while (1) {
		asm volatile ("fence");
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("wfi");
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("nop");
		asm volatile ("nop");
	}
}

__attribute__((noinline))
int __do_hibernation(rt_ubase_t entry)
{
	/* support hibernation restore */
	if (readl((void *)RCPU_CORE0_BOOT_ENTRY_LO) == 0) {
		writel(entry & 0xffffffff, (void *)RCPU_CORE0_BOOT_ENTRY_LO);
		writel((entry >> 32) & 0xffffffff, (void *)RCPU_CORE0_BOOT_ENTRY_HI);

		rt_memcpy((void *)RT_SNAPSHOT_RUNTIME0_MEM_START, (void *)RT_SNAPSHOT0_MEM_START, RT_SNAPSHOT_RUNTIME0_MEM_SIZE);
		rt_memcpy((void *)RT_SNAPSHOT_RUNTIME1_MEM_START,
				(void *)(RT_SNAPSHOT0_MEM_START + RT_SNAPSHOT_RUNTIME0_MEM_SIZE), RT_SNAPSHOT_RUNTIME1_MEM_SIZE);
		rt_memcpy((void *)RT_SNAPSHOT_RUNTIME2_MEM_START,
				(void *)(RT_SNAPSHOT0_MEM_START + RT_SNAPSHOT_RUNTIME0_MEM_SIZE + RT_SNAPSHOT_RUNTIME1_MEM_SIZE), RT_SNAPSHOT_RUNTIME2_MEM_SIZE);
		rt_memcpy((void *)RT_SNAPSHOT_RUNTIME3_MEM_START, (void *)RT_SNAPSHOT1_MEM_START, RT_SNAPSHOT_RUNTIME3_MEM_SIZE);
		rt_memcpy((void *)RT_SNAPSHOT_RUNTIME4_MEM_START, (void *)(RT_SNAPSHOT1_MEM_START + RT_SNAPSHOT_RUNTIME3_MEM_SIZE), RT_SNAPSHOT_RUNTIME4_MEM_SIZE);

		asm volatile ("fence.i");
		spacemit_wakeup_c0();

		asm volatile("jr %0" :: "r"(entry));
		__builtin_unreachable();

	}

	/* support hibernation store */
	asm volatile ("fence.i");
	rt_memcpy((void *)RT_SNAPSHOT0_MEM_START, (void *)RT_SNAPSHOT_RUNTIME0_MEM_START, RT_SNAPSHOT_RUNTIME0_MEM_SIZE);
	rt_memcpy((void *)(RT_SNAPSHOT0_MEM_START + RT_SNAPSHOT_RUNTIME0_MEM_SIZE),
			(void *)RT_SNAPSHOT_RUNTIME1_MEM_START, RT_SNAPSHOT_RUNTIME1_MEM_SIZE);
	rt_memcpy((void *)(RT_SNAPSHOT0_MEM_START + RT_SNAPSHOT_RUNTIME0_MEM_SIZE + RT_SNAPSHOT_RUNTIME1_MEM_SIZE),
			(void *)RT_SNAPSHOT_RUNTIME2_MEM_START, RT_SNAPSHOT_RUNTIME2_MEM_SIZE);
	rt_memcpy((void *)RT_SNAPSHOT1_MEM_START, (void *)RT_SNAPSHOT_RUNTIME3_MEM_START, RT_SNAPSHOT_RUNTIME3_MEM_SIZE);
	rt_memcpy((void *)(RT_SNAPSHOT1_MEM_START + RT_SNAPSHOT_RUNTIME3_MEM_SIZE), (void *)RT_SNAPSHOT_RUNTIME4_MEM_START, RT_SNAPSHOT_RUNTIME4_MEM_SIZE);
	
	asm volatile ("fence.i");
	spacemit_wakeup_c0();
	return RT_EOK;
}

/* switch to dedicated stack at 0x100700000 before hibernation operations */
__attribute__((naked))
static int __hibernation_enter(rt_ubase_t entry)
{
	asm volatile (
		"mv   t0, sp\n\t"            /* save caller sp */
		"li   sp, 0x100700400\n\t"   /* switch to dedicated stack */
		"addi sp, sp, -16\n\t"       /* allocate frame on dedicated stack */
		"sd   t0, 0(sp)\n\t"         /* save caller sp on dedicated stack */
		"sd   ra, 8(sp)\n\t"         /* save return address on dedicated stack */
		"call __do_hibernation\n\t"  /* call hibernation (not tail) */
		"ld   ra, 8(sp)\n\t"         /* restore return address */
		"ld   sp, 0(sp)\n\t"         /* restore caller sp */
		"ret"
	);
}

static int __suspend_asm_finish(rt_ubase_t arg, rt_ubase_t entry, rt_ubase_t context)
{
	unsigned int val;
	typedef void (*__entry)(void *);
	__entry ptr;

	if (read_csr(mhartid) != 0) {
		__core1_enter_wfi(entry);
		/* unreachable */
	}

	/* hart0 path */
	if (arg == PM_SLEEP_MODE_DEEP) {
		rt_memcpy((void *)0x0, (void *)__esos_lite_start,
			(unsigned long)__esos_lite_end - (unsigned long)__esos_lite_start);
		asm volatile ("fence.i");
		ptr = (__entry)0x0;
		ptr((void *)entry);
	} else {
		return __hibernation_enter(entry);
	}

	/* should never be here */
	return RT_EOK;
}

static void suspend_save_csrs(struct suspend_context *context)
{
	context->mscratch = read_csr(mscratch);
	context->mie = read_csr(mie);
	context->mtvec = read_csr(mtvec);
}

static void suspend_restore_csrs(struct suspend_context *context)
{
	write_csr(mscratch, context->mscratch);
	write_csr(mtvec, context->mtvec);
	write_csr(mie, context->mie);
}

extern int __cpu_suspend_enter(rt_ubase_t context);
extern int __cpu_resume_enter(rt_ubase_t context);

struct suspend_context context = { 0 };

int cpu_suspend(rt_ubase_t arg,
                int (*finish)(rt_ubase_t arg,
                              rt_ubase_t entry,
                              rt_ubase_t context))
{
	int rc = 0;

	/* Finisher should be non-NULL */
	if (!finish)
		return -RT_EINVAL;

	/* Save additional CSRs*/
	suspend_save_csrs(&context);

	/* Save context on stack */
	if (__cpu_suspend_enter((unsigned long)&context)) {
		/* Call the finisher */
		rc = finish(arg, (rt_ubase_t)__cpu_resume_enter, (rt_ubase_t)&context);

		/*
		 * Should never reach here, unless the suspend finisher
		 * fails. Successful cpu_suspend() should return from
		 * __cpu_resume_entry()
		 */
		if (!rc)
			rc = -RT_EINVAL;
	}

	/* Restore additional CSRs */
	suspend_restore_csrs(&context);

	return rc;
}

extern void rt_hw_eclic_save(void);
extern void rt_hw_eclic_restore(void);

/**
 * This function will put n308 into sleep mode.
 *
 * @param pm pointer to power manage structure
 */
static void sleep(struct rt_pm *pm, uint8_t mode)
{
	rt_uint32_t val;
	rt_uint64_t time;

	switch (mode)
	{
	case PM_SLEEP_MODE_NONE:
	break;

	case PM_SLEEP_MODE_IDLE:
	break;

	case PM_SLEEP_MODE_LIGHT:
	break;

	case PM_SLEEP_MODE_DEEP:
		/* save the plic configuration */
		rt_hw_eclic_save();

		/* disable the clint timer */
		time = SysTimer_GetLoadValue();
		SysTimer_SetCompareValue(0xffffffffffffffff);
		/* clear the timer pending */
		clear_csr(mip, MIP_MTIP);

		cpu_suspend(PM_SLEEP_MODE_DEEP, __suspend_asm_finish);

		/* enable the clint timer */
		SysTimer_SetCompareValue(time);

		/* restore the plic configuration */
		rt_hw_eclic_restore();

		if (read_csr(mhartid) == 1) {
			val = readl((unsigned int *)RT24_CORE1_IDLE_CFG_REG);
			val &= ~0x3;
			writel(val, (unsigned int *)RT24_CORE1_IDLE_CFG_REG);

			/* tell rcpu0 that i has been powered up */
			rt_sem_release(rt_lowpwrsem);
		} else {
			rt_sem_release((rt_sem_t)lpmdev->user_data);
			/* unmaks Cluster0 M2 exit interrupt */
			rt_hw_interrupt_umask(AP_C0_M2_EXIT_INT_NUM);
		}

		rt_pm_request(RT_PM_DEFAULT_SLEEP_MODE);
	break;

	case PM_SLEEP_MODE_STANDBY:
	break;

	case PM_SLEEP_MODE_SHUTDOWN:
		/* save the plic configuration */
		rt_hw_eclic_save();

		/* disable the clint timer */
		time = SysTimer_GetLoadValue();
		SysTimer_SetCompareValue(0xffffffffffffffff);
		/* clear the timer pending */
		clear_csr(mip, MIP_MTIP);

		cpu_suspend(PM_SLEEP_MODE_SHUTDOWN, __suspend_asm_finish);

		/* enable the clint timer */
		SysTimer_SetCompareValue(time);

		/* restore the plic configuration */
		rt_hw_eclic_restore();

		if (read_csr(mhartid) == 1) {
			val = readl((unsigned int *)RT24_CORE1_IDLE_CFG_REG);
			val &= ~0x3;
			writel(val, (unsigned int *)RT24_CORE1_IDLE_CFG_REG);

			/* tell rcpu0 that i has been powered up */
			rt_sem_release(rt_lowpwrsem);
		} else {
			rt_sem_release((rt_sem_t)lpmdev->user_data);
			/* unmaks Cluster0 M2 exit interrupt */
			rt_hw_interrupt_umask(AP_C0_M2_EXIT_INT_NUM);
			rt_pm_release(PM_SLEEP_MODE_SHUTDOWN);
			rt_pm_request(RT_PM_DEFAULT_DEEPSLEEP_MODE);
		}

		rt_pm_request(RT_PM_DEFAULT_SLEEP_MODE);
	break;

	default:
		RT_ASSERT(0);
	break;
	}
}

static void run(struct rt_pm *pm, uint8_t mode)
{
}

/**
 * This function start the timer of pm
 *
 * @param pm Pointer to power manage structure
 * @param timeout How many OS Ticks that MCU can sleep
 */
static void pm_timer_start(struct rt_pm *pm, rt_uint32_t timeout)
{
	RT_ASSERT(pm != RT_NULL);
	RT_ASSERT(timeout > 0);
}

/**
 * This function stop the timer of pm
 *
 * @param pm Pointer to power manage structure
 */
static void pm_timer_stop(struct rt_pm *pm)
{
	RT_ASSERT(pm != RT_NULL);
}

/**
 * This function calculate how many OS Ticks that MCU have suspended
 *
 * @param pm Pointer to power manage structure
 *
 * @return OS Ticks
 */
static rt_tick_t pm_timer_get_tick(struct rt_pm *pm)
{
	return 0;
}

static void rt_lowpwr_rx_callback(struct mbox_client *cl, void *data)
{
	pm_enter_flag = 1;
	rt_sem_release(rt_lowpwrsem);
}

static void rt_lowpwr_poll(void *priv)
{
	unsigned int val;

	while (1) {
		rt_sem_take(rt_lowpwrsem, RT_WAITING_FOREVER);

		if (pm_enter_flag == 1) {
			pm_enter_flag = 0;
			rt_pm_release(RT_PM_DEFAULT_SLEEP_MODE);
			rt_system_power_manager();
		} else {
			/* tell rcpu0 that i has been waked up*/
			mbox_send_message(lpm_tx_chan, &val);
			mbox_chan_txdone(lpm_tx_chan, 0);
		}
	}
}

void rt_lowpwr_notify(rt_uint8_t event, rt_uint8_t mode, void *data)
{
	unsigned int val;

	switch (event) {
	case RT_PM_ENTER_SLEEP:
		/* let rcpu control is own low power mode */
		val = readl((unsigned int *)PMU_AUDIO_CLK_CTRL);
		val &= ~((1 << AUIO_FORCE_PWR_ON_OFFSET) | (1 << AUDIO_CTRL_BY_AP_OFFSET));
		writel(val, (unsigned int *)PMU_AUDIO_CLK_CTRL);
		break;
	case RT_PM_EXIT_SLEEP:
 		break;
	}
}

int rt_hw_k3_pm_init(void)
{
	int ret, i;
	unsigned int value;
	char *string, *strend;
	rt_int32_t size;
	rt_uint8_t timer_mask = 0;
	struct dtb_node *compatible_node;
	struct dtb_node *dtb_head_node = get_dtb_node_head();
	audio_pmu_vote_t *lpvote = (audio_pmu_vote_t *)AUDIO_PMU_VOTE_REG;

	static const struct rt_pm_ops _ops = {
		sleep,
		run,
		pm_timer_start,
		pm_timer_stop,
		pm_timer_get_tick
	};

	if (read_csr(mhartid) == 0) {
		/* clear the vote registers before system low power mode */
		writel(0x0, (unsigned int *)AUDIO_VOTE_FOR_MAIN_PMU);

		lpvote->bits.vote_for_clk_off = 0;
		lpvote->bits.vote_for_plloff = 0;

		writel(0, (unsigned int *)APCR_PER_VETE_REG);
	}

	/* initialize timer mask */
	/* timer_mask = 1UL << PM_SLEEP_MODE_DEEP; */

	rt_pm_notify_set(rt_lowpwr_notify, RT_NULL);

	/* initialize system pm module */
	rt_system_pm_init(&_ops, timer_mask, RT_NULL);

	if (read_csr(mhartid) == 0) {
		lpmdev = rt_device_find("lpmdev");
		if (!lpmdev) {
			rt_kprintf("Can't find low power device\n");
			return -RT_EINVAL;
		}

		return 0;
	}

	compatible_node = dtb_node_find_compatible_node(dtb_head_node, "spacemit,rslpm");
	if (compatible_node != RT_NULL) {
		/* check the status */
		if (!dtb_node_device_is_available(compatible_node))
			return -RT_EINVAL;

		for_each_property_string_extend(compatible_node, "mbox-names", string, strend, size) {
			if (rt_strcmp(string, "tx") == 0) {
				lpm_tx_client.dev = compatible_node;
				lpm_tx_client.tx_block = false;
				lpm_tx_client.rx_callback = RT_NULL;
				lpm_tx_chan = mbox_request_channel_byname(&lpm_tx_client, string);
			} else {
				lpm_rx_client.dev = compatible_node;
				lpm_rx_client.tx_block = false;
				lpm_rx_client.rx_callback = rt_lowpwr_rx_callback;
				lpm_rx_chan = mbox_request_channel_byname(&lpm_rx_client, string);
			}
		}
	}

	rt_lowpwrsem = rt_sem_create("lpmsem", 0, RT_IPC_FLAG_FIFO);
	if (!rt_lowpwrsem) {
		rt_kprintf("create low power sem error\n");
		return -RT_EINVAL;
	}

	rt_lowpwrtid = rt_thread_create("lpm_thread",
			rt_lowpwr_poll,
			RT_NULL,
			2048,
			RT_THREAD_PRIORITY_MAX / 3,
			20);
	if (!rt_lowpwrtid) {
		rt_kprintf("Failed to create low power mode thread\n");
		return -RT_EINVAL;
	}

	rt_thread_startup(rt_lowpwrtid);

	return 0;
}
INIT_ENV_EXPORT(rt_hw_k3_pm_init);
