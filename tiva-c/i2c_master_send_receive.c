#include "stdint.h"
#include "inc/tm4c123gh6pm.h"
#ifndef __TM4C123GH6PM_H__
#define SYSCTL_RCGCGPIO_R       (*((volatile uint32_t *)0x400FE608))
#define SYSCTL_RCGCGPIO_R3      0x00000008  // GPIO Port D Run Mode Clock
                                            // Gating Control

#define SYSCTL_RCGCI2C_R        (*((volatile uint32_t *)0x400FE620))
#define SYSCTL_RCGCI2C_R3       0x00000008  // I2C Module 3 Run Mode Clock
                                            // Gating Control

#define GPIO_PORTD_DEN_R        (*((volatile uint32_t *)0x4000751C))
#define GPIO_PORTD_AFSEL_R      (*((volatile uint32_t *)0x40007420))
#define GPIO_PORTD_ODR_R        (*((volatile uint32_t *)0x4000750C))

#define GPIO_PORTD_PCTL_R       (*((volatile uint32_t *)0x4000752C))
#define GPIO_PCTL_PD0_I2C3SCL   0x00000003  // I2C3SCL on PD0
#define GPIO_PCTL_PD1_I2C3SDA   0x00000030  // I2C3SDA on PD1

#define I2C3_MCR_R              (*((volatile uint32_t *)0x40023020))
#define I2C_MCR_MFE             0x00000010  // I2C Master Function Enable

#define I2C3_MTPR_R             (*((volatile uint32_t *)0x4002300C))

#define I2C3_MSA_R              (*((volatile uint32_t *)0x40023000))
#define I2C_MSA_RS              0x00000001  // Receive not send

#define I2C3_MDR_R              (*((volatile uint32_t *)0x40023008))

#define I2C3_MCS_R              (*((volatile uint32_t *)0x40023004))
#define I2C_MCS_START           0x00000002  // Generate START
#define I2C_MCS_RUN             0x00000001  // I2C Master Enable
#define I2C_MCS_STOP            0x00000004  // Generate STOP
#define I2C_MCS_ACK             0x00000008  // Data Acknowledge Enable
#define I2C_MCS_DATACK          0x00000008  // Acknowledge Data
#define I2C_MCS_ADRACK          0x00000004  // Acknowledge Address
#define I2C_MCS_ERROR           0x00000002  // Error
#define I2C_MCS_BUSY            0x00000001  // I2C Busy
#define I2C_MCS_BUSBSY          0x00000040  // Bus Busy
#endif

/**
* Wait until I2C master is not busy and return error code
* If there is no error, return 0 
**/
static int I2C_wait_till_done(void)
{
    while(I2C3_MCS_R & I2C_MCS_BUSY); // wait until I2C master is not busy
    return I2C3_MCS_R & (I2C_MCS_DATACK + I2C_MCS_ADRACK + I2C_MCS_ERROR);  // return I2C error code
}

/**
* I2C intialization and GPIO alternate function configuration
* Configure Port D pins 0 and 1 as I2C module 3
* PD0 -- I2C3_SCL
* PD1 -- I2C3_SDA
**/
void I2C3_Init ( void )
{
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R3; // Enable and provide a clock to GPIO Port D
    SYSCTL_RCGCI2C_R  |= SYSCTL_RCGCI2C_R3 ; // Enable and provide a clock to I2C module 3
    GPIO_PORTD_DEN_R  |= 0x00000003;  // Enable digital functions for PD0, PD1 pins.
    GPIO_PORTD_AFSEL_R |= 0x00000003; // PD0 and PD1 function is controlled by the alternate hardware function
    GPIO_PORTD_ODR_R  |= 0x00000002;  // Enable the I2C3SDA (PD1) pin for open-drain operation

    GPIO_PORTD_PCTL_R &= ~0x000000FF;  // Clear encoded value of PD0 and PD1
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD0_I2C3SCL + GPIO_PCTL_PD1_I2C3SDA;  // Assign the I2C signals to PD0 and PD1 pins

    I2C3_MCR_R = I2C_MCR_MFE;          // Enable I2C3 Master mode

    /* Configure I2C clock rate to 100Kbps for Standard Mode, with System Clock is 16MHz */
    // TIMER_PRD = (SystemClock / (2 × (SCL_LP + SCL_HP) × SCL_CLK)) - 1
    // TIMER_PRD = 16,000,000 / (2 × (6 + 4) × 100000)) - 1
    // TIMER_PRD = 7
    I2C3_MTPR_R = 0x07 ;
}

/** Write single byte **/
int I2C3_Write_Byte(int slave_address, char data)
{
    char error;

    I2C3_MSA_R = slave_address << 1;
    I2C3_MDR_R = data;             // write first byte
    I2C3_MCS_R = I2C_MCS_START + I2C_MCS_RUN + I2C_MCS_STOP; // 0x0000.0007
    error = I2C_wait_till_done();   // wait until write is complete
    if(error) return error;

    while(I2C3_MCS_R & I2C_MCS_BUSBSY);  // wait until bus is not busy

    return 0;
}

/** I2C Master transfer multiple bytes **/
int I2C3_Write_Buffer(int slave_address, int length, char* data)
{
    char error;
    if (length <= 0)
        return -1;
    /* send slave address and starting address */
    I2C3_MSA_R = slave_address << 1;
    I2C3_MDR_R = *data++;          // write first byte
    I2C3_MCS_R = I2C_MCS_START + I2C_MCS_RUN;
    error = I2C_wait_till_done();  // wait until write is complete
    if(error) return error;
    length--;

    /* send data one byte at a time */
    while (length > 1)
    {
        I2C3_MDR_R = *data++;   // write the next byte
        I2C3_MCS_R = I2C_MCS_RUN;
        error = I2C_wait_till_done();
        if (error) return error;
        length--;
    }

    /* send last byte and a STOP */
    I2C3_MDR_R = *data++;   //write the last byte
    I2C3_MCS_R = I2C_MCS_RUN + I2C_MCS_STOP;
    error = I2C_wait_till_done();
    if(error) return error;

    while(I2C3_MCS_R & I2C_MCS_BUSBSY);  // wait until bus is not busy

    return 0;
}

/** I2c Master read single byte **/
int I2C3_Read_Byte(int slave_address, char *data)
{
    char error;

    I2C3_MSA_R = slave_address << 1;
    I2C3_MSA_R |= I2C_MSA_RS;  // receive direction
    I2C3_MCS_R = I2C_MCS_START + I2C_MCS_RUN + I2C_MCS_STOP;
    error = I2C_wait_till_done();   // wait until read is complete
    if(error) return error;
    *data = I2C3_MDR_R;             // read byte

    while(I2C3_MCS_R & I2C_MCS_BUSBSY);  // wait until bus is not busy

    return 0;
}

/** I2c Master read multiple bytes **/
int I2C3_Read_Buffer(int slave_address, int length, char *data)
{
    char error;

    if(length <= 0)
        return -1;

    I2C3_MSA_R = slave_address << 1;
    I2C3_MSA_R |= I2C_MSA_RS;  // receive direction

    if(length == 1)
        I2C3_MCS_R = I2C_MCS_ACK + I2C_MCS_START + I2C_MCS_RUN + I2C_MCS_STOP;
    else
        I2C3_MCS_R = I2C_MCS_ACK + I2C_MCS_START + I2C_MCS_RUN;
    error = I2C_wait_till_done();   // wait until operation is complete
    if(error) return error;
    *data++ = I2C3_MDR_R;           // read first byte
    length--;

    if(length > 0)
    {
        /* read the remain bytes */
        while(length > 1)
        {
            I2C3_MCS_R = I2C_MCS_ACK + I2C_MCS_RUN;  // RUN
            error = I2C_wait_till_done();   // wait until operation is complete
            if(error) return error;
            *data++ = I2C3_MDR_R;           // read bytes
            length--;
        }

        I2C3_MCS_R = I2C_MCS_RUN + I2C_MCS_STOP;  // RUN + STOP
        error = I2C_wait_till_done();   // wait until operation is complete
        if(error) return error;
        *data++ = I2C3_MDR_R;           // read last byte
        length--;
    }

    while(I2C3_MCS_R & I2C_MCS_BUSBSY);  // wait until bus is not busy

    return 0;
}

/**
* System Clock: 16MHz
* SCL rate: 100Kbps
* PD0 -- SCL
* PD1 -- SDA
* Slave Address: 0x3B
**/
int main(void)
{
    const int slave_address = 0x3B;  // 0x3B = 0011.1011

    I2C3_Init();

    I2C3_Write_Byte(slave_address, 'H');
    I2C3_Write_Byte(slave_address, 'e');
    I2C3_Write_Byte(slave_address, 'l');
    I2C3_Write_Byte(slave_address, 'l');
    I2C3_Write_Byte(slave_address, 'o');
    I2C3_Write_Byte(slave_address, '.');

    I2C3_Write_Buffer(slave_address, 7, "HELLO");

    char received_bytes_1[5] = {0x55, 0x55, 0x55, 0x55, 0x55};
    char received_bytes_2[5] = {0};

    I2C3_Read_Byte(slave_address, &received_bytes_1[0]);
    I2C3_Read_Byte(slave_address, &received_bytes_1[1]);
    I2C3_Read_Byte(slave_address, &received_bytes_1[2]);
    I2C3_Read_Byte(slave_address, &received_bytes_1[3]);
    I2C3_Read_Byte(slave_address, &received_bytes_1[4]);
    
    I2C3_Read_Buffer(slave_address, 5, received_bytes_2);

    for(;;)
    {
    }
}

