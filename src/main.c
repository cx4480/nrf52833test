/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/random/rand32.h>

#include <string.h>

#include "../inc/mymath.h"

/* Register your code with the logger */
LOG_MODULE_REGISTER(myapps,LOG_LEVEL_DBG);

/* The devicetree node identifier for the "led0" alias. */
#define LED1_NODE 	DT_ALIAS(led1)
#define LED2_NODE 	DT_ALIAS(led2)
#define LED3_NODE 	DT_ALIAS(led3)
#define SW0_NODE	DT_ALIAS(sw0) 
#define SW1_NODE	DT_ALIAS(sw1) 
#define SW2_NODE	DT_ALIAS(sw2) 
#define SW3_NODE	DT_ALIAS(sw3) 
#define PWM_LED0    DT_ALIAS(pwm_led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);
static const struct gpio_dt_spec button0 = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static const struct gpio_dt_spec button1 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
static const struct gpio_dt_spec button2 = GPIO_DT_SPEC_GET(SW2_NODE, gpios);
static const struct gpio_dt_spec button3 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);
static const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart20));
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0);

static struct gpio_callback button1_cb_data;

static uint8_t rx_buf[10] = {0}; //A buffer to store incoming UART data 

/* STEP 9 - Define semaphore to monitor instances of available resource */
K_SEM_DEFINE(instance_monitor_sem, 10, 10);

/* STEP 3 - Initialize the available instances of this resource */
volatile uint32_t available_instance_count = 10;

typedef struct {
    uint32_t x;
    uint32_t y;
} queue_val;
K_MSGQ_DEFINE(device_message_queue, sizeof(queue_val), 10, 4);

void button1_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&led2);

	{
		static int i = 0,j = 0;
		printk("sum %d+%d = %d\n\r", i,j,sum(i,j));
		i++;
		j+=2;
	}

	{
		int exercise_num=2;
		uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 'H', 'e', 'l', 'l','o'};

		LOG_INF("Exercise %d",exercise_num);    
		LOG_DBG("A log message in debug level");
		LOG_WRN("A log message in warning level!");
		LOG_ERR("A log message in Error level!");

		LOG_HEXDUMP_INF(data, sizeof(data),"Sample Data!"); 
	}

	{
		static uint8_t tx_buf[] =  {"uart0 tx send test buf\n\r"};
		int err = uart_tx(uart, tx_buf, sizeof(tx_buf), SYS_FOREVER_US);
		if (err) {
			return ;
		}
	}
}

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {
	
	case UART_TX_DONE:
		// do something
		break;

	case UART_TX_ABORTED:
		// do something
		break;
		
	case UART_RX_RDY:
		if((evt->data.rx.len) == 1){
			if(evt->data.rx.buf[evt->data.rx.offset] == '1')
				gpio_pin_toggle_dt(&led3);
			else if (evt->data.rx.buf[evt->data.rx.offset] == '2')
				gpio_pin_toggle_dt(&led2);	
		}
		break;

	case UART_RX_BUF_REQUEST:
		// do something
		break;

	case UART_RX_BUF_RELEASED:
		// do something
		break;
		
	case UART_RX_DISABLED:
		uart_rx_enable(dev, rx_buf, sizeof(rx_buf), 100);
		break;

	case UART_RX_STOPPED:
		// do something
		break;
		
	default:
		break;
	}
}

// Define stack size used by each thread
#define THREAD0_STACKSIZE        512
#define THREAD1_STACKSIZE        512
#define WORQ_THREAD_STACK_SIZE   512
#define PRODUCER_STACKSIZE       512
#define CONSUMER_STACKSIZE       512
#define MutexTHREAD0_STACKSIZE       512
#define MutexTHREAD1_STACKSIZE       512

/* STEP 2 - Set the priorities of the threads */
#define THREAD0_PRIORITY 2 
#define THREAD1_PRIORITY 3
#define WORKQ_PRIORITY   4
/* STEP 2 - Set the priority of the producer and consumper thread */
#define PRODUCER_PRIORITY 6 
#define CONSUMER_PRIORITY 5
#define MutexTHREAD0_PRIORITY 7 
#define MutexTHREAD1_PRIORITY 7

/* STEP 5 - Define the two counters with a constant combined total */
#define COMBINED_TOTAL   40

int32_t increment_partner = 0; 
int32_t decrement_partner = COMBINED_TOTAL; 

/* STEP 11 - Define mutex to protect access to shared code section */
K_MUTEX_DEFINE(test_mutex);

// Define stack area used by workqueue thread
static K_THREAD_STACK_DEFINE(my_stack_area, WORQ_THREAD_STACK_SIZE);

// Define queue structure
static struct k_work_q offload_work_q = {0};

/* STEP 5 - Define function to emulate non-urgent work */
static inline void emulate_work()
{
	for(volatile int count_out = 0; count_out < 150000; count_out ++);
}

/* STEP 7 - Create work_info structure and offload function */
struct k_work work;

void offload_function(struct k_work *work_tem)
{
	emulate_work();
}

// Shared code run by both threads
void shared_code_section(void)
{
	/* STEP 12.1 - Lock the mutex */
	k_mutex_lock(&test_mutex, K_FOREVER);

	/* STEP 6 - Increment partner and decrement partner changed */
	/* according to logic defined in exercise text */
	increment_partner += 1;
	increment_partner = increment_partner % COMBINED_TOTAL; 

	decrement_partner -= 1;
	if (decrement_partner == 0) 
	{
		decrement_partner = COMBINED_TOTAL;
	}

	/* STEP 12.2 - Unlock the mutex */
	k_mutex_unlock(&test_mutex);

	/* STEP 7 - Print counter values if they do not add up to COMBINED_TOTAL */
	if(increment_partner + decrement_partner != COMBINED_TOTAL ) {
		printk("Increment_partner (%d) + Decrement_partner (%d) = %d \n",
	                increment_partner, decrement_partner, (increment_partner + decrement_partner));
	}
	else {
		// printk("Mutex test OK\n");
	}
}

void thread0(void)
{
    uint64_t time_stamp;
    int64_t delta_time;
	/* STEP 8 - Start the workqueue, */
	/* initialize the work item and connect it to its handler function */ 
	k_work_queue_start(&offload_work_q, my_stack_area,
					K_THREAD_STACK_SIZEOF(my_stack_area), WORKQ_PRIORITY,
					NULL);
	k_work_init(&work, offload_function);
    while (1) {
        time_stamp = k_uptime_get();
		/* STEP 9 - Submit the work item to the workqueue instead of calling emulate_work() directly */
		/* Remember to comment out emulate_work(); */
        k_work_submit_to_queue(&offload_work_q, &work);
        delta_time = k_uptime_delta(&time_stamp);
        // printk("thread0 yielding this round in %lld ms\n", delta_time);
        k_msleep(500);
    }   
}

/* STEP 4 - Define entry function for thread1 */
void thread1(void)
{
    uint64_t time_stamp;
    int64_t delta_time;

    while (1) {
        time_stamp = k_uptime_get();
        emulate_work();
        delta_time = k_uptime_delta(&time_stamp);

        // printk("thread1 yielding this round in %lld ms\n", delta_time);
        k_msleep(500);
    }   
}

/* STEP 4 - Producer thread relinquishing access to instance */
void producer(void)
{
	printk("Producer thread started\n");
	while (1) {
		{
			/* STEP 6.2 - Increment available resource */
			available_instance_count++;
			// printk("Resource given and available_instance_count = %d\n", available_instance_count);

			/* STEP 10.2 - Give semaphore after finishing access to resource */
			k_sem_give(&instance_monitor_sem);
		}
		// Assume the resource instance access is released at this point
		k_msleep(500 + sys_rand32_get() % 10);
	}
}

/* STEP 5 - Consumer thread obtaining access to instance */
void consumer(void)
{
	printk("Consumer thread started\n");
	while (1) {
		{
			/* STEP 10.1 - Get semaphore before access to the resource */
			k_sem_take(&instance_monitor_sem, K_FOREVER);

			/* STEP 6.1 - Decrement available resource */
			available_instance_count--;
			// printk("Resource taken and available_instance_count = %d\n", available_instance_count);
		}
		// Assume the resource instance access is released at this point
		k_msleep( sys_rand32_get() % 10);
	}
}

/* STEP 4 - Functions for thread0 and thread1 with a shared code section */
void Mutexthread0(void)
{
	printk("MutexThread 0 started\n");
	while (1) {
		shared_code_section(); 
		k_msleep(100);

		if(gpio_pin_get_dt(&button2))
		{
			gpio_pin_toggle_dt(&led2);

			int ret;
			static queue_val val = {0, '0'};
			ret = k_msgq_put(&device_message_queue,&val,K_FOREVER);
			if (ret){
				LOG_ERR("Return value from k_msgq_put = %d",ret);
			}
			LOG_INF("Values put to the queue: %d.%d", val.x, val.y);
			val.x ++;
			val.y ++;
		}
	}
}

void Mutexthread1(void)
{
	printk("MutexThread 1 started\n");
	while (1) {
		shared_code_section(); 
		k_msleep(100);

		if(gpio_pin_get_dt(&button3))
		{
			gpio_pin_toggle_dt(&led2);

			int ret;
			queue_val val;
			ret = k_msgq_get(&device_message_queue,&val,K_FOREVER);
			if (ret){
				LOG_ERR("Return value from k_msgq_put = %d",ret);
			}
			LOG_INF("Values got from the queue: %d.%d", val.x, val.y);
		}
	}
}

static void timer0_handler(struct k_timer *dummy)
{
    /*Interrupt Context - System Timer ISR */
    gpio_pin_toggle_dt(&led3);
}

// Define and initialize threads
K_THREAD_DEFINE(thread0_id, THREAD0_STACKSIZE, thread0, NULL, NULL, NULL,
		THREAD0_PRIORITY, 0, 0);
K_THREAD_DEFINE(thread1_id, THREAD1_STACKSIZE, thread1, NULL, NULL, NULL,
		THREAD1_PRIORITY, 0, 0);

K_THREAD_DEFINE(producer_id, PRODUCER_STACKSIZE, producer, NULL, NULL, NULL,
		PRODUCER_PRIORITY, 0, 0);
K_THREAD_DEFINE(consumer_id, CONSUMER_STACKSIZE, consumer, NULL, NULL, NULL,
		CONSUMER_PRIORITY, 0, 0);

K_THREAD_DEFINE(Mutexthread0_id, MutexTHREAD0_STACKSIZE, Mutexthread0, NULL, NULL, NULL,
		MutexTHREAD0_PRIORITY, 0, 0);
K_THREAD_DEFINE(Mutexthread1_id, MutexTHREAD1_STACKSIZE, Mutexthread1, NULL, NULL, NULL,
		MutexTHREAD1_PRIORITY, 0, 0);


K_TIMER_DEFINE(timer0, timer0_handler, NULL);

int main(void)
{
	int ret;
	
	if (!gpio_is_ready_dt(&led1)) {
		return 0;
	}
	if (!gpio_is_ready_dt(&led2)) {
		return 0;
	}
	if (!gpio_is_ready_dt(&led3)) {
		return 0;
	}
	if (!device_is_ready(button0.port)) {
		return -1;
	}
	if (!device_is_ready(button1.port)) {
		return -1;
	}
	if (!device_is_ready(button2.port)) {
		return -1;
	}
	if (!device_is_ready(button3.port)) {
		return -1;
	}
	if (!device_is_ready(uart)) {
		return -1;
	}
	if (!pwm_is_ready_dt(&pwm_led0)) {
		LOG_ERR("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
		return 0;
	}

	ret = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	ret = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	ret = gpio_pin_configure_dt(&led3, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}
	ret = gpio_pin_configure_dt(&button0, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&button1, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&button1, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&button2, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	ret = gpio_pin_configure_dt(&button3, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	// {
	// 	const struct uart_config uart_cfg = {
	// 		.baudrate = 57600,
	// 		.parity = UART_CFG_PARITY_NONE,
	// 		.stop_bits = UART_CFG_STOP_BITS_1,
	// 		.data_bits = UART_CFG_DATA_BITS_8,
	// 		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	// 	};
	// 	ret = uart_configure(uart, &uart_cfg);
	// 	if (ret == -ENOSYS) {
	// 		return -ENOSYS;
	// 	}
	// }

	gpio_init_callback(&button1_cb_data, button1_pressed, BIT(button1.pin)); 
	gpio_add_callback(button1.port, &button1_cb_data);	

	ret = uart_callback_set(uart, uart_cb, NULL);
	if (ret) {
		return ret;
	}

	/* start periodic timer that expires once every 1 second  */
	k_timer_start(&timer0, K_MSEC(1000), K_MSEC(1000));

	uart_rx_enable(uart, rx_buf, sizeof(rx_buf), 100);
	
	gpio_pin_set_dt(&led1,0);
	gpio_pin_set_dt(&led2,0);

	LOG_INF("mcu V4 starting............");
	while (1) {
		#define add_speed 100
		static int32_t led0_duty_cycle = 0;
		static bool led0_duty_cycle_addflag = true;
		if((led0_duty_cycle_addflag == true)&&(led0_duty_cycle >= 10000/add_speed))
			led0_duty_cycle_addflag = false;
		if((led0_duty_cycle_addflag == false)&&(led0_duty_cycle <= 0))
			led0_duty_cycle_addflag = true;
		if(led0_duty_cycle_addflag == true)
			led0_duty_cycle++;
		else
			led0_duty_cycle--;

		int ret = pwm_set_pulse_dt(&pwm_led0, led0_duty_cycle*add_speed);
		if (ret) {
			LOG_ERR("Error in pwm_set_dt(), err: %d", ret);
			return 0;
		}
		#undef add_speed

		bool val0 = gpio_pin_get_dt(&button0);
		gpio_pin_set_dt(&led1,val0);

		if(rx_buf[9])
		{
			int i;
			for(i=0; i<10; i++) {
				rx_buf[i] ++;
			}
			uint8_t *buf;
			buf = (uint8_t*)k_malloc(sizeof(rx_buf));
			memcpy(buf, rx_buf, sizeof(rx_buf));
			uart_tx(uart, buf, sizeof(rx_buf), SYS_FOREVER_US);
			k_free(buf);
			memset(rx_buf, 0, sizeof(rx_buf));
		}

		k_msleep(10); 
	}
	return 0;
}
