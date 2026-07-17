/**
  ******************************************************************************
  * @file    ads1220.h
  * @brief   ADS1220 24-bit ADC 驱动头文件
  *          适用于 STM32H750 + SPI2 接口
  *          支持 K 型热电偶测量 (增益 32, 内部 2.048V 基准)
  ******************************************************************************
  */

#ifndef __ADS1220_H__
#define __ADS1220_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"

/* ======================================================================== */
/*  ADS1220 寄存器地址                                                        */
/* ======================================================================== */
#define ADS1220_REG_CONFIG0                 0x00
#define ADS1220_REG_CONFIG1                 0x01
#define ADS1220_REG_CONFIG2                 0x02
#define ADS1220_REG_CONFIG3                 0x03

/* ======================================================================== */
/*  ADS1220 命令字节                                                          */
/* ======================================================================== */
#define ADS1220_CMD_RESET                   0x06
#define ADS1220_CMD_START                   0x08
#define ADS1220_CMD_POWERDOWN               0x02
#define ADS1220_CMD_RDATA                   0x10
#define ADS1220_CMD_RREG(addr)             (0x20 | ((addr) & 0x03))
#define ADS1220_CMD_WREG(addr)             (0x40 | ((addr) & 0x03))

/* ======================================================================== */
/*  Register 0x00 — Config0: 输入多路复用器 & PGA 增益                        */
/* ======================================================================== */
/* MUX[3:0] — 输入选择 (位 7:4) */
#define ADS1220_MUX_AIN0_AIN1               0x00    /* AINP = AIN0, AINN = AIN1 (差分) */

/* PGA[2:0] — 增益设置 (位 3:1) */
#define ADS1220_PGA_GAIN_1                  0x00    /* 增益  1 */
#define ADS1220_PGA_GAIN_2                  0x02    /* 增益  2 */
#define ADS1220_PGA_GAIN_4                  0x04    /* 增益  4 */
#define ADS1220_PGA_GAIN_8                  0x06    /* 增益  8 */
#define ADS1220_PGA_GAIN_16                 0x08    /* 增益 16 */
#define ADS1220_PGA_GAIN_32                 0x0A    /* 增益 32 */
#define ADS1220_PGA_GAIN_64                 0x0C    /* 增益 64 */
#define ADS1220_PGA_GAIN_128                0x0E    /* 增益 128 */

/* ======================================================================== */
/*  Register 0x01 — Config1: 数据速率 & 转换模式 & 基准                        */
/* ======================================================================== */
/* DR[2:0] — 数据速率 (位 7:5) */
#define ADS1220_DR_20SPS                    0x00    /* 20  SPS (同时抑制 50/60Hz) */
#define ADS1220_DR_45SPS                    0x20    /* 45  SPS */
#define ADS1220_DR_90SPS                    0x40    /* 90  SPS */
#define ADS1220_DR_175SPS                   0x60    /* 175 SPS */
#define ADS1220_DR_330SPS                   0x80    /* 330 SPS */
#define ADS1220_DR_600SPS                   0xA0    /* 600 SPS */
#define ADS1220_DR_1000SPS                  0xC0    /* 1000 SPS */

/* CM — 转换模式 (位 4) */
#define ADS1220_CM_CONTINUOUS               0x00    /* 连续转换模式 */
#define ADS1220_CM_SINGLESHOT               0x10    /* 单次转换模式 */

/* VREF[1:0] — 基准电压选择 (位 2:1) */
#define ADS1220_VREF_INTERNAL               0x00    /* 内部 2.048V 基准 */
#define ADS1220_VREF_EXT_REFP0              0x02    /* 外部 REFP0/REFN0 */
#define ADS1220_VREF_EXT_REFP1              0x04    /* 外部 REFP1/REFN1 */
#define ADS1220_VREF_INTERNAL_ALWAYS        0x06    /* 内部基准保持上电 */

/* ======================================================================== */
/*  Register 0x02 — Config2: IDAC & DRDY 模式等                               */
/* ======================================================================== */
#define ADS1220_IDAC_DISABLE                0x00    /* IDAC 关闭 */
#define ADS1220_DRDYMOD_DEFAULT             0x00    /* DRDY 默认模式 */

/* ======================================================================== */
/*  硬件抽象宏 (移植时只需修改此处)                                             */
/* ======================================================================== */
/* --- CS (片选) 控制 --- */
#define ADS1220_CS_LOW()        HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET)
#define ADS1220_CS_HIGH()       HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET)

/* --- DRDY (数据就绪) 引脚状态读取 --- */
#define ADS1220_DRDY_IS_LOW()   (HAL_GPIO_ReadPin(DRDY_GPIO_Port, DRDY_Pin) == GPIO_PIN_RESET)

/* --- SPI 句柄 --- */
#define ADS1220_SPI_HANDLE      hspi2

/* ======================================================================== */
/*  全局参数定义                                                              */
/* ======================================================================== */
#define ADS1220_VREF_MV         2048.0f     /* 内部基准电压 (mV) */
#define ADS1220_GAIN            32.0f       /* PGA 增益 */
#define ADS1220_LSB_MSB         (1.0f / 131072.0f)  /* 1 LSB = Vref / (Gain * 2^23) = 2048/(32*8388608) */

/* ======================================================================== */
/*  函数声明                                                                  */
/* ======================================================================== */
void    ADS1220_Init(void);
void    ADS1220_Reset(void);
void    ADS1220_Start(void);
void    ADS1220_PowerDown(void);
uint8_t ADS1220_ReadRegister(uint8_t reg);
void    ADS1220_WriteRegister(uint8_t reg, uint8_t value);
int32_t ADS1220_ReadRaw(void);
float   ADS1220_ReadMilliVolts(void);

#ifdef __cplusplus
}
#endif

#endif /* __ADS1220_H__ */
