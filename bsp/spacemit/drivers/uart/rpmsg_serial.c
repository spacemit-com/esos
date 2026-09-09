/*
 * Copyright (c) 2022-2025, Spacemit
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * RPMSG virtual serial: expose the rpmsg link as an RT-Thread serial device.
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <drivers/serial.h>
#include <openamp/remoteproc.h>
#include <openamp/virtio.h>
#include <openamp/rpmsg.h>
#include <openamp/rpmsg_virtio.h>

#include "rpmsg_serial.h"

#ifdef BSP_USING_RPMSG_SERIAL

/*
 * This driver turns the existing OpenAMP rpmsg channel into a standard
 * RT-Thread serial device named RT_RPMSG_SERIAL_NAME.  On the Linux big-core
 * side the same channel shows up as /dev/ttyRPMSGx via the kernel rpmsg_tty
 * driver (service name "rpmsg-tty").  When BSP_USING_RPMSG_SERIAL is enabled,
 * board.c selects RT_RPMSG_SERIAL_NAME as the boot console instead of the
 * UART, so the small core's console/msh runs on the big core without a
 * physical UART.
 *
 * Boot-order note: the console is redirected to RT_RPMSG_SERIAL_NAME in
 * rt_hw_board_init(), long before the rpmsg endpoint is bound (the big core
 * may still be booting).  Until then putc() only buffers into the TX ring;
 * the flush thread sends the buffered bytes once is_rpmsg_ept_ready() turns
 * true, so nothing is printed to the UART and the early boot log is not lost.
 *
 * TX path (small core -> big core):
 *   putc() only stages bytes into a ring buffer and wakes the flush thread,
 *   because rpmsg_send() may block on the virtqueue and putc() may be called
 *   from interrupt context (rt_kprintf).  The flush thread drains the ring in
 *   chunks no larger than one rpmsg message (512-byte buffer minus 16-byte
 *   header).
 *
 * RX path (big core -> small core):
 *   The endpoint callback copies an incoming message into a staging ring and
 *   then feeds rt_hw_serial_isr(RX_IND), which loops getc() to fill the
 *   framework fifo and finally wakes the finsh/msh reader.
 */

#define RPMSG_SERIAL_SERVICE        "rpmsg-tty"
#define RPMSG_SERIAL_RX_SIZE        1024
#define RPMSG_SERIAL_TX_SIZE        2048
#define RPMSG_SERIAL_TX_CHUNK       496
#define RPMSG_SERIAL_BIND_TICK      10

extern struct rpmsg_device *rpdev;

static struct rpmsg_endpoint g_ept;
static rt_uint8_t g_ept_ready;
static rt_uint8_t g_rx_buf[RPMSG_SERIAL_RX_SIZE];
static rt_uint8_t g_tx_buf[RPMSG_SERIAL_TX_SIZE];
static rt_uint8_t g_tx_chunk[RPMSG_SERIAL_TX_CHUNK];
static rt_uint16_t g_rx_head, g_rx_tail, g_rx_count;
static rt_uint16_t g_tx_head, g_tx_tail, g_tx_count;
static rt_uint32_t g_rx_dropped, g_tx_dropped;
static rt_sem_t g_tx_sem;
static struct rt_serial_device g_serial;
static struct rt_spinlock g_lock;

/*
 * Serial device ops
 */
static rt_err_t rpmsg_serial_configure(struct rt_serial_device *serial,
                                       struct serial_configure *cfg)
{
    /* the link has a fixed speed, nothing to configure */
    (void)serial;
    (void)cfg;

    return RT_EOK;
}

static rt_err_t rpmsg_serial_control(struct rt_serial_device *serial,
                                     rt_int32_t cmd, void *arg)
{
    /* RX is always enabled, nothing to do here */
    (void)serial;
    (void)cmd;
    (void)arg;

    return RT_EOK;
}

static rt_int32_t rpmsg_serial_putc(struct rt_serial_device *serial, char c)
{
    rt_base_t level;
    rt_int32_t wake = 0;

    (void)serial;

    level = rt_spin_lock_irqsave(&g_lock);
    if (g_tx_count < RPMSG_SERIAL_TX_SIZE) {
        if (g_tx_count == 0)
            wake = 1;

        g_tx_buf[g_tx_head] = (rt_uint8_t)c;
        g_tx_head = (rt_uint16_t)((g_tx_head + 1) % RPMSG_SERIAL_TX_SIZE);
        g_tx_count++;
    } else {
        g_tx_dropped++;
    }
    rt_spin_unlock_irqrestore(&g_lock, level);

    /* wake the flush thread once the ring transitions from empty */
    if (wake && g_ept_ready)
        rt_sem_release(g_tx_sem);

    return 1;
}

static rt_int32_t rpmsg_serial_getc(struct rt_serial_device *serial)
{
    rt_int32_t ch = -1;
    rt_base_t level;

    (void)serial;

    level = rt_spin_lock_irqsave(&g_lock);
    if (g_rx_count > 0) {
        ch = g_rx_buf[g_rx_tail];
        g_rx_tail = (rt_uint16_t)((g_rx_tail + 1) % RPMSG_SERIAL_RX_SIZE);
        g_rx_count--;
    }
    rt_spin_unlock_irqrestore(&g_lock, level);

    return ch;
}

static const struct rt_uart_ops g_rpmsg_uart_ops =
{
    rpmsg_serial_configure,
    rpmsg_serial_control,
    rpmsg_serial_putc,
    rpmsg_serial_getc,
};

/*
 * RPMSG endpoint callbacks
 */
static int rpmsg_serial_rx_cb(struct rpmsg_endpoint *ept, void *data,
                              size_t len, uint32_t src, void *priv)
{
    rt_uint8_t *buf = (rt_uint8_t *)data;
    rt_uint32_t i;
    rt_base_t level;

    (void)ept;
    (void)src;
    (void)priv;

    /* stage the incoming bytes into the RX ring */
    level = rt_spin_lock_irqsave(&g_lock);
    for (i = 0; i < len; i++) {
        if (g_rx_count < RPMSG_SERIAL_RX_SIZE) {
            g_rx_buf[g_rx_head] = buf[i];
            g_rx_head = (rt_uint16_t)((g_rx_head + 1) % RPMSG_SERIAL_RX_SIZE);
            g_rx_count++;
        } else {
            g_rx_dropped++;
        }
    }
    rt_spin_unlock_irqrestore(&g_lock, level);

    /* feed the framework fifo and wake the finsh/msh reader */
    if (g_serial.serial_rx != RT_NULL)
        rt_hw_serial_isr(&g_serial, RT_SERIAL_EVENT_RX_IND);

    return 0;
}

static void rpmsg_serial_unbind(struct rpmsg_endpoint *ept)
{
    (void)ept;

    g_ept_ready = 0;
}

/*
 * Flush thread: drain the TX ring over rpmsg once the endpoint is bound
 */
static void rpmsg_serial_flush(void)
{
    rt_uint32_t n;
    rt_uint32_t i;
    rt_base_t level;

    while (1) {
        /* copy up to one rpmsg message worth of data out of the ring */
        level = rt_spin_lock_irqsave(&g_lock);
        n = g_tx_count;
        if (n == 0) {
            rt_spin_unlock_irqrestore(&g_lock, level);
            return;
        }
        if (n > RPMSG_SERIAL_TX_CHUNK)
            n = RPMSG_SERIAL_TX_CHUNK;

        for (i = 0; i < n; i++) {
            g_tx_chunk[i] = g_tx_buf[g_tx_tail];
            g_tx_tail = (rt_uint16_t)((g_tx_tail + 1) % RPMSG_SERIAL_TX_SIZE);
        }
        g_tx_count -= n;
        rt_spin_unlock_irqrestore(&g_lock, level);

        if (rpmsg_send(&g_ept, g_tx_chunk, (int)n) < 0)
            g_tx_dropped += n;
    }
}

static void rpmsg_serial_thread_entry(void *parameter)
{
    rt_int32_t ret;

    (void)parameter;

    /* wait until the rproc driver has probed the rpmsg device */
    while (rpdev == RT_NULL)
        rt_thread_delay(2);

    /* announce the service and wait for the Linux side to bind it */
    ret = rpmsg_create_ept(&g_ept, rpdev, RPMSG_SERIAL_SERVICE,
                           RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
                           rpmsg_serial_rx_cb, rpmsg_serial_unbind);
    if (ret) {
        rt_kprintf("%s:%d, create rpmsg endpoint failed\n", __func__, __LINE__);
        return;
    }

    while (1) {
        /* block on the TX semaphore; the timeout re-checks the binding */
        rt_sem_take(g_tx_sem, RPMSG_SERIAL_BIND_TICK);

        if (!g_ept_ready) {
            if (is_rpmsg_ept_ready(&g_ept)) {
                g_ept_ready = 1;
            }
        }

        if (g_ept_ready)
            rpmsg_serial_flush();
    }
}

/*
 * Register the serial device and start the flush thread
 */
static int rpmsg_serial_register(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    /*
     * The lock must be ready before the console is redirected here: board.c
     * calls rt_console_set_device(RT_RPMSG_SERIAL_NAME) right after this
     * INIT_BOARD_EXPORT runs, and the first putc() from rt_show_version()
     * already takes g_lock.
     */
    rt_spin_lock_init(&g_lock);

    g_serial.ops = &g_rpmsg_uart_ops;
    g_serial.config = config;
    g_serial.config.bufsz = RPMSG_SERIAL_TX_SIZE;

    rt_hw_serial_register(&g_serial, RT_RPMSG_SERIAL_NAME,
                          RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX,
                          RT_NULL);

    return 0;
}
INIT_BOARD_EXPORT(rpmsg_serial_register);

static int rpmsg_serial_init(void)
{
    rt_thread_t tid;

    g_tx_sem = rt_sem_create("rpmsgtx", 0, RT_IPC_FLAG_FIFO);
    if (g_tx_sem == RT_NULL) {
        rt_kprintf("%s:%d, create rpmsg tx sem failed\n", __func__, __LINE__);
        return -RT_ERROR;
    }

    tid = rt_thread_create("rpmsg0", rpmsg_serial_thread_entry, RT_NULL,
                           4096, RT_THREAD_PRIORITY_MAX / 5, 20);
    if (tid == RT_NULL) {
        rt_kprintf("%s:%d, create rpmsg serial thread failed\n", __func__, __LINE__);
        return -RT_ERROR;
    }
    rt_thread_startup(tid);

    return 0;
}
INIT_APP_EXPORT(rpmsg_serial_init);


#endif /* BSP_USING_RPMSG_SERIAL */
