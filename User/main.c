// Project wire cut started 01/10/2024
//  Copyright (c) 2022, Aleksey Sedyshev
//  https://github.com/AlekseySedyshev

// LCD:      SCL-PC5, SDA- PC6,
// Buttons: Key_Up - PD0, Key_Right - PD4,  Key_Stop - PD5
//          Key_Down - PC0, Key_Start - PC1, Key_Left - PC2

// Input: PC3 - wire end

// Prog:    PD1 - SWIO, PD7 - NRST

// Output:  PD6-Motor Enable, PA1 - Motor Step, PA2 - Motor Dir, PD3 - Servo PWM

// not using: PC4, PD2, PC7

#include "main.h"
#include "SH1106.h"

// #define IWDG_ON

extern uint32_t SystemCoreClock;

uint16_t TimingDelay;
//-------------LCD-------------------

volatile uint16_t i2c_timeout;
volatile I2C_stat i2c_ack_stat;

bool lcd_out_enable = false;
bool scan_on = true;

uint16_t count_lcd = 0, but_press = 0, blink = 0;
uint8_t menu_x = 0, menu_y = 0;

int16_t *EEP_REG = (int16_t *)(EEP_ADDR);
//-----------Определения-----------------
mode_param work_ = MODE_OFF;
volatile set_param set;
//----------Motor 1--------------
volatile uint32_t step_counter = 0, dest_count = 0;
bool tim_flag = 0;
bool flag_key = false;

//--------------------------------------------------------
void DelayMs(uint16_t Delay_time)
{
    TimingDelay = Delay_time;
    while (TimingDelay > 0x00)
    {
    };
}

void init(void)
{
    { //-------------Systic init---------------
        SysTick->SR &= ~(1 << 0);
        SysTick->CMP = (SystemCoreClock / 1000) - 1; // 1ms
        SysTick->CNT = 0;
        SysTick->CTLR = 0xF;

        NVIC_SetPriority(SysTicK_IRQn, 1);
        NVIC_EnableIRQ(SysTicK_IRQn);
    }
    { //-------------Clock On------------------
        RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD;
        RCC->APB2PCENR |= RCC_APB2Periph_TIM1 | RCC_APB2Periph_AFIO;
        RCC->APB1PCENR |= RCC_APB1Periph_TIM2 | RCC_APB1Periph_I2C1;
    }
    { // Buttons: Key_Down - PC0, Key_Start - PC1, Key_Left - PC2
      //          Key_Up - PD0, Key_Right - PD4,  Key_Stop - PD5
        GPIOC->CFGLR &= (~GPIO_CFGLR_CNF0) & (~GPIO_CFGLR_CNF1) & (~GPIO_CFGLR_CNF2);
        GPIOC->OUTDR |= GPIO_OUTDR_ODR0 | GPIO_OUTDR_ODR1 | GPIO_OUTDR_ODR2;       // Pull_Up Enable
        GPIOC->CFGLR |= GPIO_CFGLR_CNF0_1 | GPIO_CFGLR_CNF1_1 | GPIO_CFGLR_CNF2_1; // Input_Pull_Up

        GPIOD->CFGLR &= (~GPIO_CFGLR_CNF0) & (~GPIO_CFGLR_CNF4) & (~GPIO_CFGLR_CNF5);
        GPIOD->OUTDR |= GPIO_OUTDR_ODR0 | GPIO_OUTDR_ODR4 | GPIO_OUTDR_ODR5;       // Pull_Up Enable
        GPIOD->CFGLR |= GPIO_CFGLR_CNF0_1 | GPIO_CFGLR_CNF4_1 | GPIO_CFGLR_CNF5_1; // Input_Pull_Up
    }
    { // In Digit: Wire_End
        GPIOC->CFGLR &= (~GPIO_CFGLR_CNF3);
        GPIOC->OUTDR |= GPIO_OUTDR_ODR3;
        GPIOC->CFGLR |= GPIO_CFGLR_CNF3_1; // Input Mode Pull Up
    }
    { // Servo PWM:   PD3 - Servo PWM
        GPIOD->CFGLR &= (~GPIO_CFGLR_CNF3);
        GPIOD->CFGLR |= GPIO_CFGLR_MODE3 | GPIO_CFGLR_CNF3_1;

        TIM2->CCER |= TIM_CC2E;
        TIM2->CHCTLR1 |= TIM_OC2M_2 | TIM_OC2M_1 | TIM_OC2PE;
        TIM2->PSC = 1200 - 1;
        TIM2->ATRLR = 200;
        TIM2->CH2CVR = set.knife_open;

        TIM2->CTLR1 |= TIM_ARPE | TIM_CEN; // start
    }
    { // Step driver pins configuration
        // PD6 - Step_Driver ~Enable,
        GPIOD->CFGLR &= (~GPIO_CFGLR_CNF6); // Reset mode
        GPIOD->CFGLR |= GPIO_CFGLR_MODE6;   // PD6 - Output mode 50 MHZ

        // PA2 - Step_Driver - DIR
        GPIOA->CFGLR &= (~GPIO_CFGLR_CNF2); // Reset mode
        GPIOA->CFGLR |= GPIO_CFGLR_MODE2;   // PA2 - Output mode 50 MHZ

        // PA1 - Motor Step,
        GPIOA->CFGLR &= (~GPIO_CFGLR_CNF1);
        GPIOA->CFGLR |= GPIO_CFGLR_MODE1 | GPIO_CFGLR_CNF1_1; // Альтернативные выход

        DRV_OFF; // DRV_ON
        DIR_FWD; // DIR_BWD

        TIM1->PSC = (25000 - SPEED_DEF * 100) - 1; // 15000     // Тут можно настроить скорость под себя
        TIM1->ATRLR = 2;
        TIM1->CHCTLR1 |= TIM_OC2M_2 | TIM_OC2M_1 | TIM_OC2M_0; // PWM
        TIM1->CCER |= TIM_CC2E;
        TIM1->CH2CVR = 1;
        TIM1->DMAINTENR |= TIM_UIE;
        TIM1->BDTR |= TIM_MOE;
        NVIC_SetPriority(TIM1_UP_IRQn, 4);
        NVIC_EnableIRQ(TIM1_UP_IRQn);
    }
    //------------I2C-----------------
    {
        AFIO->PCFR1 |= AFIO_PCFR1_I2C1_REMAP | (1 << 22);    // PC5, PC6 - i2C remap
        GPIOC->CFGLR |= GPIO_CFGLR_CNF5 | GPIO_CFGLR_CNF6;   // Set Alternative OD mode
        GPIOC->CFGLR |= GPIO_CFGLR_MODE5 | GPIO_CFGLR_MODE6; // Speed 50 MHz

        I2C1->CTLR2 = SystemCoreClock / 1000000; // I2C_CTLR2_FREQ_1;//I2C_CTLR2_FREQ_3; // 8 MHz SystemCoreClock/1000000;

        I2C1->CKCFGR = SystemCoreClock / (I2C_SPEED << 1); // Normal mode 100kHz
        I2C1->CTLR1 |= I2C_CTLR1_PE;
    }
}
//---------------------------i2c -------------------------------------
void i2c_write(uint8_t byte_send) // WRITE
{
    while (!(I2C1->STAR1 & I2C_STAR1_TXE))
    {
    };
    I2C1->DATAR = byte_send;
}
void i2c_start(uint8_t i2c_addr) // Start
{
    i2c_timeout = 0;
    I2C1->CTLR1 |= I2C_CTLR1_ACK;
    while (I2C1->STAR2 & I2C_STAR2_BUSY)
    {
    }; // I2C_BUSY
    I2C1->CTLR1 |= I2C_CTLR1_START;
    while (!(I2C1->STAR1 & I2C_STAR1_SB))
    {
    }; // Start - Generated
    I2C1->DATAR = i2c_addr;
    while (!(I2C1->STAR1 & I2C_STAR1_ADDR) && i2c_timeout < 5000)
    {
        i2c_timeout++;
    }; // ACK - Received

    if (i2c_timeout >= 50000)
    {
        i2c_timeout = 0;
        i2c_ack_stat = I2C_ACK_ERR;
    }
    else
    {
        while (!(I2C1->STAR2 & I2C_STAR2_MSL))
        {
        }; // Master mode
        i2c_timeout = 0;
        i2c_ack_stat = I2C_ACK_OK;
    }
}
void i2c_stop(void)
{ // Stop

    while (!(I2C1->STAR1 & I2C_STAR1_TXE))
    {
    }; // TX buf Empty
    while (!(I2C1->STAR1 & I2C_STAR1_BTF))
    {
    }; // Byte transfer finished
    I2C1->CTLR1 |= I2C_CTLR1_STOP;
    while (I2C1->STAR2 & I2C_STAR2_BUSY)
    {
    };
}
void ReadEEPROM(void)
{
    set.wires_a = (uint16_t)EEP_REG[EEP_WIRE_A];
    set.wires_b = (uint16_t)EEP_REG[EEP_WIRE_B];
    set.wires_c = (uint16_t)EEP_REG[EEP_WIRE_C];
    set.wires_qnt = (uint16_t)EEP_REG[EEP_WIRE_QNT];
    set.knife = (uint16_t)EEP_REG[EEP_KNIFE];
    set.knife_open = (uint16_t)EEP_REG[EEP_KNIFE_OPEN];
    set.knife_close = (uint16_t)EEP_REG[EEP_KNIFE_CLOSE];
    set.wire_speed = (uint16_t)EEP_REG[EEP_SPEED];

    if (set.wires_a > 1000)
        set.wires_a = WIRE_DEF_A;
    if (set.wires_b > 10000)
        set.wires_b = WIRE_DEF_B;
    if (set.wires_c > 1000)
        set.wires_c = WIRE_DEF_C;
    if (set.wires_qnt > 1000)
        set.wires_qnt = WIRE_DEF_QNT;

    if (set.knife > 200)
        set.knife = KNIFE_DEF_CUT;
    if (set.knife_open > 200)
        set.knife_open = KNIFE_OPEN;
    if (set.knife_close > 200)
        set.knife_close = KNIFE_CLOSE;
    if (set.wire_speed > 150)
        set.wire_speed = SPEED_DEF;
}
void WriteEEPROM(void)
{
    FLASH_Unlock_Fast();
    FLASH_ErasePage_Fast(EEP_ADDR);

    FLASH_ProgramHalfWord(EEP_ADDR + 0, (uint16_t)set.wires_a);
    FLASH_ProgramHalfWord(EEP_ADDR + 2, (uint16_t)set.wires_b);
    FLASH_ProgramHalfWord(EEP_ADDR + 4, (uint16_t)set.wires_c);
    FLASH_ProgramHalfWord(EEP_ADDR + 6, (uint16_t)set.wires_qnt);
    FLASH_ProgramHalfWord(EEP_ADDR + 8, (uint16_t)set.knife);
    FLASH_ProgramHalfWord(EEP_ADDR + 10, (uint16_t)set.knife_open);
    FLASH_ProgramHalfWord(EEP_ADDR + 12, (uint16_t)set.knife_close);
    FLASH_ProgramHalfWord(EEP_ADDR + 14, (uint16_t)set.wire_speed);
    FLASH_Lock_Fast();
}
void set_knife(uint16_t knife_pos)
{
    TIM2->CH2CVR = knife_pos;
}
void key_scan(void)
{
    if (!UP())
    {
        if (but_press >= SHORT_PRESS && but_press <= SEC5_PRESS && flag_key == true)
        {
            if (menu_x == 1 && menu_y == 1 && set.wires_a < 100) // зачистка начала
            {
                set.wires_a += 1;
            }
            if (menu_x == 1 && menu_y == 2 && set.wires_b < 5000) // средняя часть провода
            {
                set.wires_b += 1;
            }
            if (menu_x == 1 && menu_y == 3 && set.wires_c < 100) // зачистка конца
            {
                set.wires_c += 1;
            }
            if (menu_x == 1 && menu_y == 4 && set.wires_qnt < 1000) // количество проводов
            {
                set.wires_qnt += 1;
            }
            if (menu_x == 1 && menu_y == 5 && set.knife < 200) // положение ножа при зачистке
            {
                set.knife += 1;
            }
            if (menu_x == 1 && menu_y == 6 && set.knife_open < 200) // положение ножа открытого
            {
                set.knife_open += 1;
            }
            if (menu_x == 1 && menu_y == 7 && set.knife_close < 200) // положение ножа закрытого
            {
                set.knife_close += 1;
            }
            if (menu_x == 1 && menu_y == 8 && set.wire_speed < 150) // скорость подачи провода
            {
                set.wire_speed += 1;
            }
            if (menu_x == 1 && menu_y == 9) // сохранить настройки
            {
                WriteEEPROM();
                ReadEEPROM();
                menu_x = 0;
                menu_y = 0;
                work_ = MODE_OFF;
            }
            flag_key = false;
        }
        if (but_press >= SEC5_PRESS && but_press < SEC10_PRESS && flag_key == true)
        {
            //--------------Начало провода----------------
            if (menu_x == 1 && menu_y == 1 && set.wires_a <= 90) // зачистка начала
            {
                set.wires_a += 10;
            }
            if (menu_x == 1 && menu_y == 2 && set.wires_b <= 4990) // средняя часть провода
            {
                set.wires_b += 10;
            }
            if (menu_x == 1 && menu_y == 3 && set.wires_c <= 90) // зачистка конца
            {
                set.wires_c += 10;
            }
            if (menu_x == 1 && menu_y == 4 && set.wires_qnt <= 990) // количество проводов
            {
                set.wires_qnt += 10;
            }
            if (menu_x == 1 && menu_y == 5 && set.knife <= 189) // положение ножа при зачистке
            {
                set.knife += 10;
            }
            if (menu_x == 1 && menu_y == 6 && set.knife_open <= 189) // положение ножа открытого
            {
                set.knife_open += 10;
            }
            if (menu_x == 1 && menu_y == 7 && set.knife_close <= 189) // положение ножа закрытого
            {
                set.knife_close += 10;
            }
            if (menu_x == 1 && menu_y == 8 && set.wire_speed <= 140) // скорость подачи провода
            {
                set.wire_speed += 10;
            }
            flag_key = false;
        }
        if (but_press >= SEC10_PRESS && but_press < 0xffff && flag_key == true)
        {
            if (menu_x == 1 && menu_y == 2 && set.wires_b <= 4900) // средняя часть провода
            {
                set.wires_b += 100;
            }
            flag_key = false;
        }

        if (but_press > 50)
            blink = 0;
        lcd_out_enable = true;
    }
    if (!DOWN())
    {
        if (but_press >= SHORT_PRESS && but_press <= SEC5_PRESS && flag_key == true)
        {
            //------------начало провода----------------
            if (menu_x == 1 && menu_y == 1 && set.wires_a > 0)
            {
                set.wires_a -= 1;
            }
            if (menu_x == 1 && menu_y == 2 && set.wires_b > 0)
            {
                set.wires_b -= 1;
            }
            if (menu_x == 1 && menu_y == 3 && set.wires_c > 0)
            {
                set.wires_c -= 1;
            }
            if (menu_x == 1 && menu_y == 4 && set.wires_qnt > 1)
            {
                set.wires_qnt -= 1;
            }
            if (menu_x == 1 && menu_y == 5 && set.knife > 1)
            {
                set.knife -= 1;
            }
            if (menu_x == 1 && menu_y == 6 && set.knife_open > 1)
            {
                set.knife_open -= 1;
            }
            if (menu_x == 1 && menu_y == 7 && set.knife_close > 1)
            {
                set.knife_close -= 1;
            }
            if (menu_x == 1 && menu_y == 8 && set.wire_speed > 0)
            {
                set.wire_speed -= 1;
            }
            if (menu_x == 1 && menu_y == 9)
            {
                WriteEEPROM();
                ReadEEPROM();
                menu_x = 0;
                menu_y = 0;
                work_ = MODE_OFF;
            }
            flag_key = false;
        }
        if (but_press > SEC5_PRESS && but_press < SEC10_PRESS && flag_key == true)
        {
            if (menu_x == 1 && menu_y == 1 && set.wires_a > 9)
            {
                set.wires_a -= 10;
            }
            if (menu_x == 1 && menu_y == 2 && set.wires_b > 10)
            {
                set.wires_b -= 10;
            }
            if (menu_x == 1 && menu_y == 3 && set.wires_c > 9)
            {
                set.wires_c -= 10;
            }
            if (menu_x == 1 && menu_y == 4 && set.wires_qnt > 10)
            {
                set.wires_qnt -= 10;
            }
            if (menu_x == 1 && menu_y == 5 && set.knife > 10)
            {
                set.knife -= 10;
            }
            if (menu_x == 1 && menu_y == 6 && set.knife_open > 10)
            {
                set.knife_open -= 10;
            }
            if (menu_x == 1 && menu_y == 7 && set.knife_close > 10)
            {
                set.knife_close -= 10;
            }
            if (menu_x == 1 && menu_y == 8 && set.wire_speed >= 10)
            {
                set.wire_speed -= 10;
            }
            flag_key = false;
        }
        if (but_press >= SEC10_PRESS && but_press < 0xffff && flag_key == true)
        {
            if (menu_x == 1 && menu_y == 2 && set.wires_b >= 100) // средняя часть провода
            {
                set.wires_b -= 100;
            }
            flag_key = false;
        }

        if (but_press > 50)
            blink = 0;
        lcd_out_enable = true;
    }
    if (!LEFT())
    {
        if (but_press > SHORT_PRESS && but_press < 0xffff && work_ == MODE_OFF)
        {
            menu_x = 1;
            if (menu_y > 1)
                menu_y--;
            else
                menu_y = 9;

            but_press = 0xffff;
        }
        lcd_out_enable = true;
    }
    if (!RIGHT())
    {
        if (but_press > SHORT_PRESS && but_press < 0xffff && work_ == MODE_OFF)
        {
            menu_x = 1;
            if (menu_y < 9)
                menu_y++;
            else
                menu_y = 1;

            but_press = 0xffff;
        }

        lcd_out_enable = true;
    }
    if (!STOP())
    {
        if (but_press > SHORT_PRESS && but_press < SEC5_PRESS)
        {
            menu_x = 1;
            menu_y = 4;
            if (work_ != MODE_OFF)
                lcd_clear();
            set_knife(set.knife_open);
            work_ = MODE_OFF;
        }

        if (but_press >= SEC5_PRESS)
        {
            menu_x = 1;
            menu_y = 4;
            if (work_ != MODE_OFF)
                lcd_clear();
            set_knife(set.knife_open);
            work_ = MODE_OFF;

            ReadEEPROM();
        }

        lcd_out_enable = true;
    }
    if (!START())
    {
        if (but_press > SHORT_PRESS && work_ == MODE_OFF && but_press < 0xffff)
        {
            lcd_clear();
            set_knife(set.knife_open);
            menu_x = 0;
            menu_y = 0;
            work_ = MODE_CUT;
            but_press = 0;
        }
        lcd_out_enable = true;
        work_ = MODE_CUT;
    }
    if (UP() && RIGHT() && DOWN() && START() && STOP() && LEFT())
    {
    }
    scan_on = false;
}

void lcd_routine(void)
{
    if (lcd_out_enable == 1)
    {
        lcd_wire_center(0, 0x11);
        lcd_gotoxy(2, 1);
        if (menu_y != 1 || (menu_x == 1 && menu_y == 1 && blink < 500))
        {
            lcd_printDec(set.wires_a, 0);
            lcd_printStr("  ", 0);
        }
        if (menu_x == 1 && menu_y == 1 && blink >= 500)
        {
            lcd_printStr("     ", 0);
        }
        lcd_gotoxy(9, 1);
        if (menu_y != 2 || (menu_x == 1 && menu_y == 2 && blink < 500))
        {
            lcd_printDec(set.wires_b, 0);
            lcd_printStr("  ", 0);
        }
        if (menu_x == 1 && menu_y == 2 && blink >= 500)
        {
            lcd_printStr("     ", 0);
        }
        lcd_gotoxy(17, 1);
        if (menu_y != 3 || (menu_x == 1 && menu_y == 3 && blink < 500))
        {
            lcd_printDec(set.wires_c, 0);
            lcd_printStr("  ", 0);
        }
        if (menu_x == 1 && menu_y == 3 && blink >= 500)
        {
            lcd_printStr("     ", 0);
        }
        lcd_gotoxy(0, 3);
        lcd_puts_UTF8("Кол");
        lcd_printStr("-", 0);
        lcd_puts_UTF8("во");
        lcd_printStr(": ", 0);
        if (menu_y != 4 || (menu_x == 1 && menu_y == 4 && blink < 500))
        {
            lcd_printDec(set.wires_qnt, 0);
            lcd_printStr(" ", 0);
        }
        if (menu_x == 1 && menu_y == 4 && blink >= 500)
        {
            lcd_printStr("   ", 0);
        }
        lcd_gotoxy(12, 3);
        lcd_printStr(" ", 0);
        lcd_puts_UTF8("Нож");
        lcd_printStr(" ", 0);
        if (menu_y != 5 || (menu_x == 1 && menu_y == 5 && blink < 500))
        {
            lcd_printDec(set.knife, 0);
            lcd_printStr(" ", 0);
        }
        if (menu_x == 1 && menu_y == 5 && blink >= 500)
        {
            lcd_printStr("   ", 0);
        }
        if (work_ == MODE_OFF)
        {
            lcd_gotoxy(6, 4);
            lcd_puts_UTF8("Нож");
            lcd_printStr(" ", 0);
            lcd_puts_UTF8("закрыт");
            lcd_printStr(" ", 0);
            if (menu_y != 6 || (menu_x == 1 && menu_y == 6 && blink < 500))
            {
                lcd_printDec(set.knife_close, 0);
                lcd_printStr(" ", 0);
            }
            if (menu_x == 1 && menu_y == 6 && blink >= 500)
            {
                lcd_printStr("     ", 0);
            }
            lcd_gotoxy(6, 5);
            lcd_puts_UTF8("Нож");
            lcd_printStr(" ", 0);
            lcd_puts_UTF8("открыт");
            lcd_printStr(" ", 0);
            if (menu_y != 7 || (menu_x == 1 && menu_y == 7 && blink < 500))
            {
                lcd_printDec(set.knife_open, 0);
                lcd_printStr(" ", 0);
            }
            if (menu_x == 1 && menu_y == 7 && blink >= 500)
            {
                lcd_printStr("   ", 0);
            }

            lcd_gotoxy(6, 6);
            lcd_puts_UTF8("Скорость");
            lcd_printStr("   ", 0);
            if (menu_y != 8 || (menu_x == 1 && menu_y == 8 && blink < 500))
            {
                lcd_printDec(set.wire_speed, 0);
                lcd_printStr(" ", 0);
            }
            if (menu_x == 1 && menu_y == 8 && blink >= 500)
            {
                lcd_printStr("   ", 0);
            }

            lcd_gotoxy(1, 7);
            if (menu_y != 9 || (menu_x == 1 && menu_y == 9 && blink < 500))
            {
                lcd_puts_UTF8("Сохранить");
                lcd_printStr(" ", 0);
                lcd_puts_UTF8("настройки");
            }
            if (menu_x == 1 && menu_y == 9 && blink >= 500)
            {
                lcd_printStr("                   ", 0);
            }
        }
    }
}

void work_routine(void)
{
    if (menu_x == 1 && menu_y == 5)
    {
        set_knife(set.knife);
    }
    if (menu_x == 1 && (menu_y == 7 || menu_y >= 8 || menu_y < 5))
    {
        set_knife(set.knife_open);
    }
    if (menu_x == 1 && menu_y == 6)
    {
        set_knife(set.knife_close);
    }
}
void set_speed(uint8_t motor_speed)
{
    TIM1->CTLR1 &= (~TIM_CEN);
    TIM1->PSC = (25000 - (motor_speed * 100)) - 1;
}
void go_to(uint32_t position, bool direction)
{
    DRV_ON;
    if (direction == DIR_FRWRD)
    {
        DIR_FWD;
    }
    else
    {
        DIR_BWD;
    }
    DelayMs(100);
    dest_count = (uint32_t)((float)(position) * (float)6.25);
    tim_flag = 0;
    TIM1->CTLR1 |= TIM_CEN;
    while (tim_flag == 0)
    {
        if (work_ == MODE_OFF)
            break;
    }
    DRV_OFF;
    DelayMs(100);
}

void action(void)
{
    uint16_t qq;

    set_knife(set.knife_open);
    DelayMs(KNIFE_TIME);

    lcd_clear();
    if (set.wires_b == 0 || set.wires_qnt == 0)
        return;
    DelayMs(1);
    set_speed(set.wire_speed);

    lcd_gotoxy(4, 1);
    lcd_puts_UTF8("Работаю");
    lcd_printStr("...", 0);

    for (qq = set.wires_qnt; qq > 0; qq--)
    {
        if (!WIRE_END()) // если провод закончился
        {
            DRV_OFF;
            set_knife(set.knife_open);
            lcd_clear();
            lcd_gotoxy(7, 0);
            lcd_puts_UTF8("Внимание");
            lcd_gotoxy(1, 2);
            lcd_puts_UTF8("Провод");
            lcd_printStr(" ", 0);
            lcd_puts_UTF8("закончился");
            lcd_printStr("!!!", 0);
            lcd_gotoxy(1, 4);
            lcd_printStr("[", 0);
            lcd_puts_UTF8("Старт");
            lcd_printStr("] - ", 0);
            lcd_puts_UTF8("Продолжить");
            lcd_gotoxy(1, 5);
            lcd_printStr("[", 0);
            lcd_puts_UTF8("Стоп");
            lcd_printStr("]  - ", 0);
            lcd_puts_UTF8("Отмена");

            while (STOP() && START())
            {
            };
            if (!START())
            {
                // DRV_ON;
                lcd_clear();
                lcd_gotoxy(4, 1);
                lcd_puts_UTF8("Работаю");
                lcd_printStr("...", 0);
            }
            if (!STOP())
            {
                work_ = MODE_OFF;
                return;
                DRV_OFF;
            }
        }
        if (set.wires_a > 0)
        {
            set_speed(set.wire_speed);
            go_to(set.wires_a, DIR_FRWRD);
            DelayMs(100);
            set_knife(set.knife);
            DelayMs(KNIFE_TIME);
            set_knife(set.knife_open);
            DelayMs(KNIFE_TIME);
            if (work_ == MODE_OFF)
                break;
        }
        DelayMs(100);
        if (set.wires_b > 0)
        {
            set_speed(set.wire_speed);
            go_to(set.wires_b, DIR_FRWRD);
            if (work_ == MODE_OFF)
                break;
        }
        if (set.wires_c > 0)
        {

            DelayMs(100);
            set_knife(set.knife);
            DelayMs(KNIFE_TIME);
            set_knife(set.knife_open);
            DelayMs(KNIFE_TIME);

            set_speed(set.wire_speed);
            go_to(set.wires_c, DIR_FRWRD);
            if (work_ == MODE_OFF)
                break;
        }
        set_knife(set.knife_close);
        DelayMs(KNIFE_TIME);
        set_knife(set.knife_open);
        DelayMs(KNIFE_TIME);
        set_speed(RETRACT_SPEED);
        //-------------Go backwar------
        DelayMs(100);
        go_to(BACK_CONST, DIR_BWRD);
        //-------------Go forward------
        DelayMs(100);
        go_to(BACK_CONST, DIR_FRWRD);
        set_speed(set.wire_speed);

        lcd_gotoxy(1, 2);
        lcd_puts_UTF8("Осталось");
        lcd_printStr(": ", 0);
        lcd_printDec(qq - 1, 0);

        lcd_printStr(" ", 0);
        lcd_gotoxy(1, 3);
        lcd_puts_UTF8("Сделано");
        lcd_printStr(": ", 0);
        lcd_printDec((set.wires_qnt - qq) + 1, 0);
        lcd_printStr(" ", 0);
    }
    // DRV_OFF;
    work_ = MODE_OFF;
    /*
     lcd_gotoxy(1, 5);
     lcd_puts_UTF8("Вкалывают");
     lcd_printStr(" ", 0);
     lcd_puts_UTF8("роботы");
     lcd_printStr(" ", 0);
     lcd_gotoxy(1, 6);
     lcd_puts_UTF8("счастлив");
     lcd_printStr(" ", 0);
     lcd_puts_UTF8("инженер");
     DelayMs(2000);
     */
    lcd_clear();
    menu_x = 1;
    menu_y = 4;
}

int main(void)
{
    ReadEEPROM();
    init();
    DelayMs(200);
    lcd_init();
    lcd_clear();
    lcd_gotoxy(4, 3);

    lcd_puts_UTF8("Версия");
    lcd_printStr(" ", 0);
    lcd_puts_UTF8("ПО");
    lcd_printStr(" ", 0);
    lcd_printStr(SW_REVISION, 0);
    //---------------Вывод даты
    lcd_gotoxy(6, 4);
    lcd_printStr(SW_DATE, 0);
    if (!STOP())
    {
        lcd_gotoxy(4 * 6, 6);

        lcd_printStr("Reset settings", 0);

        set.wires_a = WIRE_DEF_A;
        set.wires_b = WIRE_DEF_B;
        set.wires_c = WIRE_DEF_C;
        set.wires_qnt = WIRE_DEF_QNT;
        set.knife = KNIFE_DEF_CUT;
        set.knife_open = KNIFE_OPEN;
        set.knife_close = KNIFE_CLOSE;
        WriteEEPROM();
    }
    DelayMs(3000);
    lcd_clear();
    set_knife(set.knife_open);
    menu_x = 1;
    menu_y = 4;
    while (1)
    {
        if (scan_on)
            key_scan();
        lcd_routine();
        work_routine();
        if (work_ == MODE_CUT)
            action();
    }
}

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SysTick_Handler(void)
{
    if (TimingDelay > 0)
    {
        TimingDelay--;
    }
    if (blink < 1000)
    {
        blink++;
    }
    else
    {
        blink = 0;
    }
    if (count_lcd < LCD_RESCAN)
    {
        count_lcd++;
    }
    else
    {
        count_lcd = 0;
        lcd_out_enable = true;
    }

    if ((!UP()) || (!RIGHT()) || (!DOWN()) || (!START()) || (!STOP()) || (!LEFT()))
    {
        if (but_press < 0xffff)
            but_press++;

        if (but_press == SHORT_PRESS || (but_press % INC_PRESS) == 0)
            flag_key = true;
    }
    else
    {
        but_press = 0;
    }
    if ((!STOP()) && work_ == MODE_CUT && but_press > SHORT_PRESS)
    {
        work_ = MODE_OFF;
    }
    scan_on = true;
    SysTick->SR = 0;
}

void EXTI0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI0_IRQHandler(void)
{
}
void TIM1_UP_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM1_UP_IRQHandler(void)
{
    if (TIM1->INTFR & TIM_UIF)
    {
        step_counter++;
        TIM1->INTFR &= (~TIM_UIF);
        if (step_counter >= dest_count)
        {
            TIM1->CTLR1 &= (~TIM_CEN);
            tim_flag = 1;
            step_counter = 0;
            dest_count = 0;
            TIM1->CNT = 0;
        }
    }
}