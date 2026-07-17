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
  *         配置寄存器:
  *           Reg0 (0x0A): AIN0 vs AIN1 差分, PGA 增益 32
  *           Reg1 (0x04): 20 SPS, 连续转换, 50/60Hz 同步抑制
  *           Reg2 (0x10): IDAC 关闭
  *           Reg3 (0x00): 默认
  *         完成后发送 START 命令进入连续转换模式
  */
void ADS1220_Init(void)
{
    uint8_t reg_val;

    printf("[DBG] ADS1220_Init: Resetting device...\r\n");
    ADS1220_Reset();
    HAL_Delay(10);

    /* 写入 4 个配置寄存器 */
    printf("[DBG] Writing Reg0 = 0x0A (AIN0/AIN1 diff, Gain 32)...\r\n");
    ADS1220_WriteRegister(ADS1220_REG_CONFIG0, 0x0A);
    ADS1220_WriteRegister(ADS1220_REG_CONFIG1, 0x04);   /* 20 SPS, 连续转换 */
    ADS1220_WriteRegister(ADS1220_REG_CONFIG2, 0x10);   /* IDAC off */
    ADS1220_WriteRegister(ADS1220_REG_CONFIG3, 0x00);   /* 默认值 */

    HAL_Delay(1);

    /* 回读验证寄存器 */
    reg_val = ADS1220_ReadRegister(ADS1220_REG_CONFIG0);
    printf("[DBG] Readback Reg0 = 0x%02X (expected 0x0A)\r\n", reg_val);
    reg_val = ADS1220_ReadRegister(ADS1220_REG_CONFIG1);
    printf("[DBG] Readback Reg1 = 0x%02X (expected 0x04)\r\n", reg_val);
    reg_val = ADS1220_ReadRegister(ADS1220_REG_CONFIG2);
    printf("[DBG] Readback Reg2 = 0x%02X (expected 0x10)\r\n", reg_val);
    reg_val = ADS1220_ReadRegister(ADS1220_REG_CONFIG3);
    printf("[DBG] Readback Reg3 = 0x%02X (expected 0x00)\r\n", reg_val);

    /* 启动连续转换 */
    printf("[DBG] Sending START command...\r\n");
    ADS1220_Start();

    /* 等待首次转换完成 (20 SPS → 50ms, 留余量) */
    HAL_Delay(60);

    /* 检查 DRDY 状态 */
    printf("[DBG] DRDY pin after init: %s\r\n",
           ADS1220_DRDY_IS_LOW() ? "LOW (ready)" : "HIGH (busy/idle)");
}

/* ======================================================================== */
/*  数据读取                                                                  */
/* ======================================================================== */

/**
  * @brief  读取 ADC 原始 24 位数据 (带符号扩展)
  *         等待 DRDY 拉低后读取，100ms 超时保护
  * @retval 有符号 24 位整型 (扩展至 int32_t)
  */
int32_t ADS1220_ReadRaw(void)
{
    uint8_t  buf[3] = {0};
    int32_t  result;
    uint32_t timeout;
    uint32_t drdy_pin_state;

    /* 读取 DRDY 引脚当前电平 */
    drdy_pin_state = HAL_GPIO_ReadPin(DRDY_GPIO_Port, DRDY_Pin);

    /* 等待 DRDY 拉低 (数据就绪) */
    timeout = HAL_GetTick() + 100;   /* 100ms 超时 (20 SPS → 50ms 周期) */
    while (!ADS1220_DRDY_IS_LOW())
    {
        if (HAL_GetTick() >= timeout)
        {
            printf("[DBG] DRDY wait timeout! DRDY pin = %s\r\n",
                   drdy_pin_state == GPIO_PIN_RESET ? "LOW" : "HIGH");
            return 0x7FFFFF;   /* 超时返回特殊值以便区分 */
        }
    }

    /* 发送 RDATA 命令并读取 3 字节 */
    ADS1220_CS_LOW();
    ADS1220_SPI_TransferByte(ADS1220_CMD_RDATA);
    buf[0] = ADS1220_SPI_TransferByte(0x00);
    buf[1] = ADS1220_SPI_TransferByte(0x00);
    buf[2] = ADS1220_SPI_TransferByte(0x00);
    ADS1220_CS_HIGH();

    printf("[DBG] RAW bytes: 0x%02X 0x%02X 0x%02X\r\n", buf[0], buf[1], buf[2]);

    /* 拼合 24 位数据 */
    result = ((int32_t)buf[0] << 16)
           | ((int32_t)buf[1] << 8)
           | ((int32_t)buf[2] << 0);

    /* 符号扩展: 如果最高位 (bit23) 为 1, 扩展高位 */
    if (result & 0x00800000)
    {
        result |= 0xFF000000;
    }

    printf("[DBG] Raw ADC value (signed 24-bit): %ld (0x%08lX)\r\n", (long)result, (unsigned long)result);

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
