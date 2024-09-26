/**
 * Copyright (c) 2021 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>

#include "port_common.h"

#include "wizchip_conf.h"
#include "w5x00_spi.h"

#include "coapClient.h"

#include "timer.h"

#include "wizchip_conf.h"
#include "socket.h"
/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_COAP 0

/* Port */
#define PORT_COAP 5683

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 11, 2},                     // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 11, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC                       // DHCP enable/disable
};

/* COAP */
static uint8_t g_coap_send_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_coap_recv_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
coap_packet_t tx_pkt;

/* Timer  */
static volatile uint32_t g_msec_cnt = 0;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);

/* Timer  */
static void repeating_timer_callback(void);
static time_t millis(void);

/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */

int main()
{
    /* Initialize */
    int retval = 0;
    int32_t ret;
    uint8_t scratch_raw[ETHERNET_BUF_MAX_SIZE];
    coap_rw_buffer_t scratch_buf = {scratch_raw, sizeof(scratch_raw)};
    uint8_t payload[] = ""; 
    uint8_t uri_path[] = ".well-known/core"; 
    size_t payload_len = strlen((char *)payload);
    size_t uri_path_len = strlen((char *)uri_path);

    set_clock_khz();

    stdio_init_all();

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    wizchip_1ms_timer_initialize(repeating_timer_callback);

    network_initialize(g_net_info);

    /* Get network information */
    print_network_information(g_net_info);

    coapClient_init(g_coap_send_buf, g_coap_recv_buf, SOCKET_COAP);

    coap_make_request(&scratch_buf, &tx_pkt, uri_path, uri_path_len, payload, payload_len, 0x12, 0x34, NULL, COAP_METHOD_GET, COAP_CONTENTTYPE_APPLICATION_LINKFORMAT);

    while(1)
    {
        coapClient_run();
        sleep_ms(1000);
    }
    
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}

/* Timer */
static void repeating_timer_callback(void)
{
    g_msec_cnt++;

    MilliTimer_Handler();
}

static time_t millis(void)
{
    return g_msec_cnt;
}