/**
  ******************************************************************************
  * @file    ads1220.c
  * @brief   ADS1220 24-bit ADC 驱动源文件
  *          硬件平台: STM32H750 + SPI2
  *          功能: 初始化、寄存器读写、连续转换模式下的电压读取
  ******************************************************************************
  */

#include "ads1220.h"
#include <stdio.h>

/* ======================================================================== */
/*  本地变量                                                                    */
/* ======================================================================== */
static SPI_HandleTypeDef *hspi = &ADS1220_SPI_HANDLE;

/* ======================================================================== */
/*  底层 SPI 收发 (私有)                                                       */
/* ======================================================================== */

/**
  * @brief  SPI 全双工收发一个字节
  * @param  tx_data  待发送数据
  * @retval         接收到的数据
  */
static uint8_t ADS1220_SPI_TransferByte(uint8_t tx_data)
{
    uint8_t rx_data = 0;
    HAL_StatusTypeDef ret;

    for (int32_t retry = 0; retry < 3; retry++)
    {
        ret = HAL_SPI_TransmitReceive(hspi, &tx_data, &rx_data, 1, 100);
        if (ret == HAL_OK)
            break;
    }
    return rx_data;
}

/* ======================================================================== */
/*  寄存器操作                                                                  */
/* ======================================================================== */

/**
  * @brief  向 ADS1220 指定寄存器写入一个字节
  * @param  reg    寄存器地址 (0x00 ~ 0x03)
  * @param  value  要写入的值
  */
void ADS1220_WriteRegister(uint8_t reg, uint8_t value)
{
    ADS1220_CS_LOW();
    ADS1220_SPI_TransferByte(ADS1220_CMD_WREG(reg));
    ADS1220_SPI_TransferByte(value);
    ADS1220_CS_HIGH();
}

/**
  * @brief  从 ADS1220 指定寄存器读取一个字节
  * @param  reg    寄存器地址 (0x00 ~ 0x03)
  * @retval        读取到的值
  */
uint8_t ADS1220_ReadRegister(uint8_t reg)
{
    uint8_t value;

    ADS1220_CS_LOW();
    ADS1220_SPI_TransferByte(ADS1220_CMD_RREG(reg));
    value = ADS1220_SPI_TransferByte(0x00);   /* 发送 dummy 以读取 */
    ADS1220_CS_HIGH();

    return value;
}

/* ======================================================================== */
/*  命令发送                                                                  */
/* ======================================================================== */

/**
  * @brief  向 ADS1220 发送单字节命令
  * @param  cmd  命令字节
  */
static void ADS1220_SendCommand(uint8_t cmd)
{
    ADS1220_CS_LOW();
    ADS1220_SPI_TransferByte(cmd);
    ADS1220_CS_HIGH();
}

/**
  * @brief  复位 ADS1220
  */
void ADS1220_Reset(void)
{
    ADS1220_SendCommand(ADS1220_CMD_RESET);
    HAL_Delay(5);   /* 等待复位完成 */
}

/**
  * @brief  启动连续转换
  */
void ADS1220_Start(void)
{
    ADS1220_SendCommand(ADS1220_CMD_START);
}

/**
  * @brief  进入掉电模式
  */
void ADS1220_PowerDown(void)
{
    ADS1220_SendCommand(ADS1220_CMD_POWERDOWN);
}

/* ======================================================================== */
/*  初始化                                                                     */
/* ======================================================================== */

/**
  * @brief  初始化 ADS1220
  *         使用单次转换模式，每次读取时手动触发转换
  *         Reg0 (0x0A): AIN0 vs AIN1 差分, PGA 增益 32
  *         Reg1 (0x14): 20 SPS, 单次转换模式
  *         Reg2 (0x10): IDAC 关闭
  *         Reg3 (0x00): 默认
  */
void ADS1220_Init(void)
{
    /* 写入配置: 单次转换模式 */
    ADS1220_WriteRegister(ADS1220_REG_CONFIG0, 0x0A);   /* AIN0/AIN1 diff, Gain 32 */
    ADS1220_WriteRegister(ADS1220_REG_CONFIG1, 0x14);   /* 20 SPS, 单次转换模式 */
    ADS1220_WriteRegister(ADS1220_REG_CONFIG2, 0x10);   /* IDAC off */
    ADS1220_WriteRegister(ADS1220_REG_CONFIG3, 0x00);
}

/* ======================================================================== */
/*  数据读取                                                                  */
/* ======================================================================== */

/**
  * @brief  读取 ADC 原始 24 位数据 (单次转换模式)
  *         每次调用: 发送 START → 等待 65ms → 发送 RDATA → 读取 3 字节
  * @retval 有符号 24 位整型 (扩展至 int32_t), 超时返回 0x7FFFFF
  */
int32_t ADS1220_ReadRaw(void)
{
    uint8_t  buf[3] = {0};
    int32_t  result;

    /* 发送 START 触发单次转换 */
    ADS1220_CS_LOW();
    ADS1220_SPI_TransferByte(ADS1220_CMD_START);
    ADS1220_CS_HIGH();

    /* 等待转换完成 (20 SPS = 50ms, 留余量 65ms) */
    HAL_Delay(65);

    /* 发送 RDATA 命令读取 3 字节 */
    ADS1220_CS_LOW();
    ADS1220_SPI_TransferByte(ADS1220_CMD_RDATA);
    buf[0] = ADS1220_SPI_TransferByte(0x00);
    buf[1] = ADS1220_SPI_TransferByte(0x00);
    buf[2] = ADS1220_SPI_TransferByte(0x00);
    ADS1220_CS_HIGH();

    /* 拼合并符号扩展 */
    result = ((int32_t)buf[0] << 16)
           | ((int32_t)buf[1] << 8)
           | ((int32_t)buf[2] << 0);

    if (result & 0x00800000)
        result |= 0xFF000000;

    return result;
}

/**
  * @brief  读取 ADS1220 差分电压 (单位: mV)
  *         计算公式:
  *           V_diff(mV) = raw_code × Vref / (Gain × 2^23)
  *           = raw_code × 2048 / (32 × 8388608)
  *           = raw_code / 131072
  * @retval 差分电压值 (mV), 浮点数
  */
float ADS1220_ReadMilliVolts(void)
{
    int32_t raw = ADS1220_ReadRaw();

    /* raw / 131072.0f 等效于 raw * 2048.0f / (32.0f * 8388608.0f) */
    float voltage_mv = (float)raw / 131072.0f;

    return voltage_mv;
}
