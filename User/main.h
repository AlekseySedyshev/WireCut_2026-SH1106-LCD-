// Project wire cut started 01/10/2024
//  Copyright (c) 2022, Aleksey Sedyshev
//  https://github.com/AlekseySedyshev
#ifndef __MAIN_H
#define __MAIN_H

#include <ch32v00x.h>
#include <stdbool.h>

#define I2C_READ 1u
#define I2C_WRITE 0u
#define I2C_SPEED 100000u

#define SW_REVISION "1.08"
#define SW_DATE "18/11/2025"
#define DRV_PIN GPIO_OUTDR_ODR6 // PD6
#define DRV_ON GPIOD->BCR |= DRV_PIN
#define DRV_OFF GPIOD->BSHR |= DRV_PIN

#define DIR_PIN GPIO_OUTDR_ODR2 // PA2
#define DIR_FWD GPIOA->BCR |= DIR_PIN
#define DIR_BWD GPIOA->BSHR |= DIR_PIN

//---------Определение кнопок---------------
#define RIGHT() (GPIOD->INDR & GPIO_INDR_IDR4)
#define STOP() (GPIOD->INDR & GPIO_INDR_IDR5)
#define UP() (GPIOD->INDR & GPIO_INDR_IDR0)
#define DOWN() (GPIOC->INDR & GPIO_INDR_IDR0)
#define START() (GPIOC->INDR & GPIO_INDR_IDR1)
#define LEFT() (GPIOC->INDR & GPIO_INDR_IDR2)

#define WIRE_END() (GPIOC->INDR & GPIO_INDR_IDR3)

#define SHORT_PRESS 50u
#define INC_PRESS 200u
#define SEC5_PRESS 5000u
#define SEC10_PRESS 10000u

#define LCD_RESCAN 50u
//-----------------------настройки-------------------------
#define WIRE_DEF_QNT 1u  // количество нарезаемых проводов по умолчанию
#define WIRE_DEF_A 2u    // 5u   // длинна начала мм
#define WIRE_DEF_B 1196u // 185u // длина середины
#define WIRE_DEF_C 2u    // 10u  // длинна конца

#define KNIFE_DEF_CUT 35u // Обрезание провода

#define KNIFE_OPEN 20u  // 20U
#define KNIFE_CLOSE 63u // 40U
#define SPEED_DEF 30u   // 23999u
#define RETRACT_SPEED 5u

#define KNIFE_TIME 500u
#define BACK_CONST 10u

#define EEP_ADDR 0x08003FC0 // 0x0800FFC0

typedef enum
{
    I2C_ACK_ERR,
    I2C_ACK_OK,
    SENS_BASE = 0
} I2C_stat;
typedef enum
{
    MODE_OFF,
    MODE_CUT,
    MODE_SETUP,
    DIR_FRWRD = 0,
    DIR_BWRD = 1,
} mode_param;

typedef enum
{
    EEP_WIRE_A,
    EEP_WIRE_B,
    EEP_WIRE_C,
    EEP_WIRE_QNT,
    EEP_KNIFE,
    EEP_KNIFE_OPEN,
    EEP_KNIFE_CLOSE,
    EEP_SPEED
} data_eep;

typedef struct
{
    uint16_t wires_a;     // длинна зачистки
    uint16_t wires_b;     // длинна середины
    uint16_t wires_c;     // длинна зачистки
    uint16_t wires_qnt;   // количество проводов
    uint16_t knife;       // нож положение зачистки
    uint16_t knife_open;  // нож открыт
    uint16_t knife_close; // нож закрыт
    uint16_t wire_speed;  // скорость подачи провода
} set_param;

void DelayMs(uint16_t Delay_time);
void i2c_write(uint8_t byte_send);
void i2c_start(uint8_t i2c_addr);
void i2c_stop(void);
#endif