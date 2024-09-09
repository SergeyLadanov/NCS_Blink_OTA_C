/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <bluetooth/services/nus.h>


/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)



static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),	
};

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;

#define BT_LE_ADV_CONN_CUSTOM BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE, \
				       BT_GAP_ADV_SLOW_INT_MIN, \
				       BT_GAP_ADV_SLOW_INT_MAX, NULL)



static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		//LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	//LOG_INF("Connected %s", addr);

	current_conn = bt_conn_ref(conn);

	//dk_set_led_on(CON_STATUS_LED);
}


static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	//LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	if (auth_conn) {
		bt_conn_unref(auth_conn);
		auth_conn = NULL;
	}

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
		//dk_set_led_off(CON_STATUS_LED);
	}
}

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_WRN("Security failed: %s level %u err %d", addr,
			level, err);
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
	.security_changed = security_changed,
#endif
};


static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

	// LOG_INF("Received data from: %s", addr);

	// for (uint16_t pos = 0; pos != len;) {
	// 	struct uart_data_t *tx = (uart_data_t*)k_malloc(sizeof(*tx));

	// 	if (!tx) {
	// 		LOG_WRN("Not able to allocate UART send data buffer");
	// 		return;
	// 	}

	// 	/* Keep the last byte of TX buffer for potential LF char. */
	// 	size_t tx_data_size = sizeof(tx->data) - 1;

	// 	if ((len - pos) > tx_data_size) {
	// 		tx->len = tx_data_size;
	// 	} else {
	// 		tx->len = (len - pos);
	// 	}

	// 	memcpy(tx->data, &data[pos], tx->len);

	// 	pos += tx->len;

	// 	/* Append the LF character when the CR character triggered
	// 	 * transmission from the peer.
	// 	 */
	// 	if ((pos == len) && (data[len - 1] == '\r')) {
	// 		tx->data[tx->len] = '\n';
	// 		tx->len++;
	// 	}

	// 	err = uart_tx(uart, tx->data, tx->len, SYS_FOREVER_MS);
	// 	if (err) {
	// 		k_fifo_put(&fifo_uart_tx_data, tx);
	// 	}
	// }
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};

void error(void)
{
	//dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);

	// while (true) {
	// 	/* Spin for ever */
	// 	k_sleep(K_MSEC(1000));
	// }
}

// static const bt_le_adv_param *ble_param_struct = BT_LE_ADV_CONN_CUSTOM;

static int BLE_Start(void)
{
	int err = 0;


	err = bt_enable(NULL);
	if (err) {
		//error();
	}

	

	smp_bt_register();

	err = bt_nus_init(&nus_cb);
	if (err) {
		//LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return -1;
	}


		
	err = bt_le_adv_start(BT_LE_ADV_CONN_CUSTOM, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));


	if (err) {
		//LOG_ERR("Advertising failed to start (err %d)", err);
		return -1;
	}


	return err;
}

void main(void)
{
	int ret;

	if (!device_is_ready(led.port)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	//start_smp_bluetooth_adverts();
	BLE_Start();

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}
		k_msleep(SLEEP_TIME_MS);
	}
}
