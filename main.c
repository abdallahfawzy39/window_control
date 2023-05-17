#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "tm4c123gh6pm.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include "queue.h"
#include "semphr.h"

void delay_ms(int time)
{
int i, j;
for(i = 0 ; i < time; i++)
  for(j = 0; j < 3180; j++){}
}

#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"

#include "driverLIb/interrupt.h"
#include "driverLIb/pin_map.h"
#include "driverLIb/debug.h"
#include "driverLIb/gpio.h"
#include "driverLIb/sysctl.h"
#include "driverLIb/systick.h"
#include "driverLIb/timer.h"
#include "driverLIb/gpio.h"
#include "driverLIb/sysctl.h"
#include "driverLIb/pin_map.h"

#define DRIVER_UP_PIN    GPIO_PIN_4
#define DRIVER_DN_PIN    GPIO_PIN_5
#define PASSEN_UP_PIN    GPIO_PIN_2
#define PASSEN_DN_PIN    GPIO_PIN_3

#define LIMIT_UP_PIN		 GPIO_PIN_0
#define LIMIT_DN_PIN		 GPIO_PIN_1

#define IR_PIN		 			 GPIO_PIN_7

#define PASSEN_LK_PIN		 			 GPIO_PIN_6

#define DRIVER_UP_MANU_PR	5
#define DRIVER_DN_MANU_PR	5
#define DRIVER_UP_AUTO_PR	4
#define DRIVER_DN_AUTO_PR	4


#define PASSEN_UP_MANU_PR	3
#define PASSEN_DN_MANU_PR	3
#define PASSEN_UP_AUTO_PR	2
#define PASSEN_DN_AUTO_PR	2

#define IDLE_PR	1

#define EMERGGENCY_PR	6

#define UP  0
#define DN  1
#define STP 2 

int last_priority = IDLE_PR;
int motor_dir = STP;

void portB_init();
void portF_init();
	

void motor_up(){
	motor_dir = UP;
	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_2);
}
void motor_dn(){
	motor_dir = DN;
	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_3);
}
void motor_stp(){
	motor_dir = STP;
	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3, 0);
}


SemaphoreHandle_t xMutex_motor;
SemaphoreHandle_t xSemaphore_emergency;

SemaphoreHandle_t xSemaphore_driver_up_auto;
SemaphoreHandle_t xSemaphore_driver_up_manu;
SemaphoreHandle_t xSemaphore_driver_dn_auto;
SemaphoreHandle_t xSemaphore_driver_dn_manu;

SemaphoreHandle_t xSemaphore_passen_up_auto;
SemaphoreHandle_t xSemaphore_passen_up_manu;
SemaphoreHandle_t xSemaphore_passen_dn_auto;
SemaphoreHandle_t xSemaphore_passen_dn_manu;

void B_Handeler(void)
{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
		uint8_t state = GPIOIntStatus(GPIO_PORTB_BASE, true);
		delay_ms(300);
	  GPIOIntClear(GPIO_PORTB_BASE, state);
	switch(state){
		
		case(DRIVER_UP_PIN):
			if (GPIOPinRead(GPIO_PORTB_BASE, DRIVER_UP_PIN) != DRIVER_UP_PIN){
					xSemaphoreGiveFromISR(xSemaphore_driver_up_manu, &xHigherPriorityTaskWoken); // Give the semaphore to a task
			}
			else{
					xSemaphoreGiveFromISR(xSemaphore_driver_up_auto, &xHigherPriorityTaskWoken); // Give the semaphore to a task
			}
			break;
			
		case(DRIVER_DN_PIN):
				if (GPIOPinRead(GPIO_PORTB_BASE, DRIVER_DN_PIN) != DRIVER_DN_PIN){
					xSemaphoreGiveFromISR(xSemaphore_driver_dn_manu, &xHigherPriorityTaskWoken); // Give the semaphore to a task
				}
				else{
					xSemaphoreGiveFromISR(xSemaphore_driver_dn_auto, &xHigherPriorityTaskWoken); // Give the semaphore to a task
				}
			
			break;
			
		case(PASSEN_UP_PIN):
			if (GPIOPinRead(GPIO_PORTB_BASE, PASSEN_LK_PIN) != PASSEN_LK_PIN){
				if (GPIOPinRead(GPIO_PORTB_BASE, PASSEN_UP_PIN) != PASSEN_UP_PIN){
					xSemaphoreGiveFromISR(xSemaphore_passen_up_manu, &xHigherPriorityTaskWoken); // Give the semaphore to a task
			}
			else{
					xSemaphoreGiveFromISR(xSemaphore_passen_up_auto, &xHigherPriorityTaskWoken); // Give the semaphore to a task
			}
			}
			break;
			
		case(PASSEN_DN_PIN):
			if (GPIOPinRead(GPIO_PORTB_BASE, PASSEN_LK_PIN) != PASSEN_LK_PIN){
			if (GPIOPinRead(GPIO_PORTB_BASE, PASSEN_DN_PIN) != PASSEN_DN_PIN){
					xSemaphoreGiveFromISR(xSemaphore_passen_dn_manu, &xHigherPriorityTaskWoken); // Give the semaphore to a task
			}
			else{
					xSemaphoreGiveFromISR(xSemaphore_passen_dn_auto, &xHigherPriorityTaskWoken); // Give the semaphore to a task
			}
		}
			break;
	}
	portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}


void driver_up_auto(void *pvParameters)
{
	int my_priority = DRIVER_UP_AUTO_PR;
	int my_op_dir = DN;
	xSemaphoreTake(xSemaphore_driver_up_auto, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_driver_up_auto, portMAX_DELAY); // Wait for the semaphore
			
			if ( last_priority >= my_priority){
				continue;
			}
			last_priority = my_priority;
			
			if(motor_dir == my_op_dir){
				continue;
			}
			
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
			while ( (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_UP_PIN) == LIMIT_UP_PIN)
				&& (my_priority >= last_priority) && (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) == IR_PIN))
			{
				motor_up();
			}
			xSemaphoreGive(xMutex_motor);
			if (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) != IR_PIN){
				xSemaphoreGive(xSemaphore_emergency);
			}
    }
}

void driver_up_manu(void *pvParameters)
{
	int my_priority = DRIVER_UP_MANU_PR;
	int my_op_dir = DN;
	xSemaphoreTake(xSemaphore_driver_up_manu, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_driver_up_manu, portMAX_DELAY); // Wait for the semaphore
			
			if ( last_priority >= my_priority){
				continue;
			}
			last_priority = my_priority;
			if(motor_dir == my_op_dir){
				continue;
			}
			
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
			while ( (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_UP_PIN) == LIMIT_UP_PIN)
				&& (GPIOPinRead(GPIO_PORTB_BASE, DRIVER_UP_PIN) != DRIVER_UP_PIN)
			&& (my_priority >= last_priority) && (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) == IR_PIN))
			{
				motor_up();
			}
			xSemaphoreGive(xMutex_motor);
			if (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) != IR_PIN){
				xSemaphoreGive(xSemaphore_emergency);
			}
    }
}


void driver_dn_auto(void *pvParameters)
{
	int my_priority = DRIVER_DN_AUTO_PR;
	int my_op_dir = UP;
	xSemaphoreTake(xSemaphore_driver_dn_auto, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_driver_dn_auto, portMAX_DELAY); // Wait for the semaphore
      
			if ( last_priority >= my_priority){
				continue;
			}
			last_priority = my_priority;
			
			if(motor_dir == my_op_dir){
				continue;
			}
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
			while ( ((GPIOPinRead(GPIO_PORTB_BASE, LIMIT_DN_PIN) == LIMIT_DN_PIN)) && (my_priority >= last_priority)){
				motor_dn();
			}
			xSemaphoreGive(xMutex_motor);
    }
}

void driver_dn_manu(void *pvParameters)
{
	int my_priority = DRIVER_DN_MANU_PR;
	int my_op_dir = UP;
	xSemaphoreTake(xSemaphore_driver_dn_manu, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_driver_dn_manu, portMAX_DELAY); // Wait for the semaphore
			if ( last_priority >= my_priority){
				continue;
			}
			last_priority = my_priority;
			
			if(motor_dir == my_op_dir){
				continue;
			}
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
			while ( (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_DN_PIN) == LIMIT_DN_PIN)
				&& (GPIOPinRead(GPIO_PORTB_BASE, DRIVER_DN_PIN) != DRIVER_DN_PIN)
			&& (my_priority >= last_priority)){
				motor_dn();
			}
			xSemaphoreGive(xMutex_motor);
		}
}

void passen_up_auto(void *pvParameters)
{
	int my_priority = PASSEN_UP_AUTO_PR;
	int my_op_dir = DN;
	xSemaphoreTake(xSemaphore_passen_up_auto, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_passen_up_auto, portMAX_DELAY); // Wait for the semaphore
			if ( last_priority >= my_priority){
				continue;
			}
			last_priority = my_priority;
			
			if(motor_dir == my_op_dir){
				continue;
			}
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
      while ( (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_UP_PIN) == LIMIT_UP_PIN)  
				&& (my_priority >= last_priority)  && (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) == IR_PIN)){
				motor_up();
			}
			xSemaphoreGive(xMutex_motor);
			if (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) != IR_PIN){
				xSemaphoreGive(xSemaphore_emergency);
			}
    }
}

void passen_up_manu(void *pvParameters)
{
	int my_priority = PASSEN_UP_MANU_PR;
	int my_op_dir = DN;
	xSemaphoreTake(xSemaphore_passen_up_manu, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_passen_up_manu, portMAX_DELAY); // Wait for the semaphore
			if ( last_priority >= my_priority){
				continue;
			}
			last_priority = my_priority;
			if(motor_dir == my_op_dir){
				continue;
			}
			
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
			while ( (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_UP_PIN) == LIMIT_UP_PIN)
				&& (GPIOPinRead(GPIO_PORTB_BASE, PASSEN_UP_PIN) != PASSEN_UP_PIN) &&
			(my_priority >= last_priority) && (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) == IR_PIN)){
				motor_up();
			}
			xSemaphoreGive(xMutex_motor);
			if (GPIOPinRead(GPIO_PORTB_BASE, IR_PIN) != IR_PIN){
				xSemaphoreGive(xSemaphore_emergency);
			}
    }
}


void passen_dn_auto(void *pvParameters)
{
	int my_priority = PASSEN_DN_AUTO_PR;
	int my_op_dir = UP;
	xSemaphoreTake(xSemaphore_passen_dn_auto, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_passen_dn_auto, portMAX_DELAY); // Wait for the semaphore
			if ( last_priority >= my_priority){
				continue;
			}
			
			last_priority = my_priority;
			if(motor_dir == my_op_dir){
				continue;
			}
			
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
			while ( (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_DN_PIN) == LIMIT_DN_PIN) && (my_priority >= last_priority)){
				motor_dn();
			}
		xSemaphoreGive(xMutex_motor);
    }
}

void passen_dn_manu(void *pvParameters)
{
	int my_priority = PASSEN_DN_MANU_PR;
	int my_op_dir = UP;
	xSemaphoreTake(xSemaphore_passen_dn_manu, 0);
    for(;;)
    {
			xSemaphoreTake(xSemaphore_passen_dn_manu, portMAX_DELAY); // Wait for the semaphore
			if ( last_priority >= my_priority){
				continue;
			}
			
			last_priority = my_priority;
			if(motor_dir == my_op_dir){
				continue;
			}
			xSemaphoreTake(xMutex_motor, portMAX_DELAY);
			while ( (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_DN_PIN) == LIMIT_DN_PIN)
				&& (GPIOPinRead(GPIO_PORTB_BASE, PASSEN_DN_PIN) != PASSEN_DN_PIN)   && (my_priority >= last_priority)){
				motor_dn();
			}
			xSemaphoreGive(xMutex_motor);
    }
}


void emergency( void *pvParameters)
{
	int my_priority = EMERGGENCY_PR;
	xSemaphoreTake(xSemaphore_emergency, 0);
	const TickType_t xFrequency = pdMS_TO_TICKS(2000); // 2000ms = 2s
	
  int  xLastWakeTime = xTaskGetTickCount();
	for(;;){
			xSemaphoreTake(xSemaphore_emergency, portMAX_DELAY);
			xLastWakeTime = xTaskGetTickCount();
			last_priority = my_priority; 
			
			motor_stp();
			delay_ms(100);
			
			motor_dn();
			while( ( xTaskGetTickCount() <= (xFrequency + xLastWakeTime)) && (GPIOPinRead(GPIO_PORTB_BASE, LIMIT_DN_PIN) == LIMIT_DN_PIN)){}
			
			motor_stp();
	}
    // This function will be called by the Idle Task at idle time
}

void idle( void *pvParameters)
{
	int my_priority = IDLE_PR;
	for(;;){
			last_priority = my_priority;
			motor_stp();
	}
    // This function will be called by the Idle Task at idle time
}



int main(){
	
		xMutex_motor = xSemaphoreCreateMutex();
		xSemaphore_emergency = xSemaphoreCreateBinary();
		xSemaphore_driver_up_auto = xSemaphoreCreateBinary();
		xSemaphore_driver_up_manu = xSemaphoreCreateBinary();
		xSemaphore_driver_dn_auto = xSemaphoreCreateBinary();
		xSemaphore_driver_dn_manu = xSemaphoreCreateBinary();

		xSemaphore_passen_up_auto = xSemaphoreCreateBinary();
		xSemaphore_passen_up_manu = xSemaphoreCreateBinary();
		xSemaphore_passen_dn_auto = xSemaphoreCreateBinary();
		xSemaphore_passen_dn_manu = xSemaphoreCreateBinary();

	
	portB_init();
	portF_init();
	
	xTaskCreate(driver_up_auto, "Task1", 100, NULL, 4, NULL); // Create a task with priority 1
	xTaskCreate(driver_up_manu, "Task2", 100, NULL, 5, NULL); // Create a task with priority 1
  xTaskCreate(driver_dn_auto, "Task3", 100, NULL, 4, NULL); // Create a task with priority 1
  xTaskCreate(driver_dn_manu, "Task4", 100, NULL, 5, NULL); // Create a task with priority 1
  
	xTaskCreate(passen_up_auto, "Task5", 100, NULL, 2, NULL); // Create a task with priority 1
	xTaskCreate(passen_up_manu, "Task6", 100, NULL, 3, NULL); // Create a task with priority 1
  xTaskCreate(passen_dn_auto, "Task7", 100, NULL, 2, NULL); // Create a task with priority 1
  xTaskCreate(passen_dn_manu, "Task8", 100, NULL, 3, NULL); // Create a task with priority 1
  
  xTaskCreate(idle, "Task9", 100, NULL, 1, NULL); // Create an idle task
  xTaskCreate(emergency, "Task10", 100, NULL, 6, NULL); // Create an idle task
  vTaskStartScheduler();
	
	while(1){
	//	motor_dn();
		//delay_ms(2000);
		//motor_stp();
		//delay_ms(2000);
		//motor_up();
		//delay_ms(2000);
		//motor_stp();
		//delay_ms(2000);
	}
}



void portB_init(){                                                              // this port hanldes all the inputs                 
  /************ connecting clock to PORTF ****************************/
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);                                  
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOB)){}                          
  
  /************ unlocking debugging **********************************/
  GPIOUnlockPin(GPIO_PORTB_BASE, 0xFF);
  
  /************ setting input output pins ****************************/
  //GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3); 
  GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);               
  GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);                     
  
	IntPrioritySet(17, 0xE0);
  /************ PORTF interrupt enable ****************************/
  GPIOIntTypeSet(GPIO_PORTB_BASE, DRIVER_DN_PIN | DRIVER_UP_PIN | PASSEN_DN_PIN | PASSEN_UP_PIN, GPIO_FALLING_EDGE); 
  GPIOIntRegister(GPIO_PORTB_BASE, B_Handeler);                               
  GPIOIntEnable(GPIO_PORTB_BASE, DRIVER_DN_PIN | DRIVER_UP_PIN | PASSEN_DN_PIN | PASSEN_UP_PIN);
}

void portF_init(){                                                              // this port hanldes all the inputs                 
  /************ connecting clock to PORTF ****************************/
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);                                  
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOF)){}                          
  
  /************ unlocking debugging **********************************/
  GPIOUnlockPin(GPIO_PORTF_BASE, 0xFF);
  
  /************ setting input output pins ****************************/
  GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_2 | GPIO_PIN_3); 
  //GPIOPinTypeGPIOInput(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);               
  //GPIOPadConfigSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);                     
  
	//IntPrioritySet(17, 0x00);
  /************ PORTF interrupt enable ****************************/
  //GPIOIntTypeSet(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_RISING_EDGE); 
  //GPIOIntRegister(GPIO_PORTB_BASE, B_Handeler);                               
  //GPIOIntEnable(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_INT_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
}



