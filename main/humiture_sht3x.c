/**
    Copyright (C) 2025 The Sistemas Empotrados subject at UPV

    @file    humiture_sht3x.c
    @author  María Cebriá
    @version V0.1
    @date    2026-04-23
    @brief   This file is a template for C implementation files

    Bla, ... here you can write more explanation about your C module file
    Put your definitions, code, etc. in the provided subsection
*/

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include "humiture.h"
#include <stdio.h>

#include "driver/i2c.h"
#include "i2c_bus.h"
#include "sht3x.h"
#include "driver/gpio.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define I2C_MASTER_SCL_IO GPIO_NUM_10 //!< gpio number for I2C master clock
#define I2C_MASTER_SDA_IO GPIO_NUM_11 //!< gpio number for I2C master data
#define I2C_MASTER_NUM I2C_NUM_1      //!< I2C port number for master dev
#define I2C_MASTER_FREQ_HZ 100000     //!< I2C master clock frequency

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static i2c_bus_handle_t i2c_bus = NULL;
static sht3x_handle_t sht3x = NULL;

/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/**
    @brief  Bla, bla
    @param  Bla, bla

    @retval The number os pepes
*/
void humiture_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    sht3x = sht3x_create(i2c_bus, SHT3x_ADDR_PIN_SELECT_VSS);

    sht3x_set_measure_mode(sht3x, SHT3x_PER_2_MEDIUM); /*!< here read data in periodic mode*/
}

bool humiture_read(float *temperature, float *humidity)
{
    return sht3x_get_humiture(sht3x, temperature, humidity);
}

/* End of file ****************************************************************/
