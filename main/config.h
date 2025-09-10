#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s.h"

// ========================== 系统配置 ==========================
#define I2C_MASTER_FREQ_HZ    400000    // I2C总线频率（400kHz）
#define I2S_SAMPLE_RATE       44100     // I2S采样率（Hz）
#define PWM_FREQ_HZ           50        // 舵机PWM频率（50Hz=20ms周期）
#define ENCODER_PULSE_PER_ROT 100       // 编码器每圈脉冲数（根据实际型号修改）

// ========================== INMP441 麦克风 ==========================
#define INMP441_LR_GPIO       GPIO_NUM_4
#define INMP441_BCK_GPIO      GPIO_NUM_5
#define INMP441_WS_GPIO       GPIO_NUM_6
#define INMP441_DOUT_GPIO     GPIO_NUM_7

// I2S配置（输入通道，麦克风）
#define I2S_IN_PORT           I2S_NUM_0
#define I2S_IN_MODE           I2S_MODE_MASTER | I2S_MODE_RX
#define I2S_IN_FORMAT         I2S_DATA_FORMAT_I2S_MSB

// ========================== SH1106 OLED ==========================
#define SH1106_SDA_GPIO       GPIO_NUM_8
#define SH1106_SCL_GPIO       GPIO_NUM_9
#define SH1106_RES_GPIO       GPIO_NUM_NC  // 可选，不使用时设为GPIO_NUM_NC

// I2C配置（OLED和MPU6050共享）
#define I2C_MASTER_NUM        I2C_NUM_0
#define SH1106_ADDR           0x3C         // OLED I2C地址（0x3C或0x3D）

// ========================== 舵机配置 ==========================
#define SERVO1_GPIO           GPIO_NUM_10
#define SERVO2_GPIO           GPIO_NUM_11
#define SERVO3_GPIO           GPIO_NUM_12
#define SERVO4_GPIO           GPIO_NUM_13

// PWM定时器配置
#define SERVO_PWM_GROUP       PWM_GROUP_0
#define SERVO_PWM_CHANNELS    4
#define SERVO_MIN_DUTY_US     500     // 0°对应高电平时间（μs）
#define SERVO_MAX_DUTY_US     2500    // 180°对应高电平时间（μs）

// ========================== MAX98357A 功放 ==========================
#define MAX98357A_DIN_GPIO    GPIO_NUM_14
#define MAX98357A_GAIN_GPIO   GPIO_NUM_15  // 高电平12dB，低电平32dB
#define MAX98357A_BCK_GPIO    INMP441_BCK_GPIO  // 复用麦克风BCK
#define MAX98357A_WS_GPIO     INMP441_WS_GPIO   // 复用麦克风WS

// I2S配置（输出通道，功放）
#define I2S_OUT_PORT          I2S_NUM_1
#define I2S_OUT_MODE          I2S_MODE_MASTER | I2S_MODE_TX
#define I2S_OUT_FORMAT        I2S_DATA_FORMAT_I2S_MSB

// ========================== MPU6050 传感器 ==========================
#define MPU6050_SDA_GPIO      SH1106_SDA_GPIO  // 复用OLED SDA
#define MPU6050_SCL_GPIO      SH1106_SCL_GPIO  // 复用OLED SCL
#define MPU6050_INT_GPIO      GPIO_NUM_16
#define MPU6050_ADDR          0x68             // MPU6050 I2C地址

// ========================== OV2640 摄像头 ==========================
#define OV2640_SIOC_GPIO      GPIO_NUM_18
#define OV2640_SIOD_GPIO      GPIO_NUM_19
#define OV2640_XCLK_GPIO      GPIO_NUM_20
#define OV2640_RESET_GPIO     GPIO_NUM_21
#define OV2640_PWDN_GPIO      GPIO_NUM_22
#define OV2640_VSYNC_GPIO     GPIO_NUM_23
#define OV2640_HSYNC_GPIO     GPIO_NUM_24
#define OV2640_PCLK_GPIO      GPIO_NUM_25

// 图像数据总线（D0-D7）
#define OV2640_D0_GPIO        GPIO_NUM_26
#define OV2640_D1_GPIO        GPIO_NUM_27
#define OV2640_D2_GPIO        GPIO_NUM_28
#define OV2640_D3_GPIO        GPIO_NUM_29
#define OV2640_D4_GPIO        GPIO_NUM_30
#define OV2640_D5_GPIO        GPIO_NUM_31
#define OV2640_D6_GPIO        GPIO_NUM_32
#define OV2640_D7_GPIO        GPIO_NUM_33

// ========================== 正交编码器 ==========================
#define ENCODER_A_GPIO        GPIO_NUM_34  // 仅输入引脚
#define ENCODER_B_GPIO        GPIO_NUM_35  // 仅输入引脚

// ========================== 按钮配置 ==========================
#define BUTTON1_GPIO          GPIO_NUM_36  // 仅输入引脚
#define BUTTON2_GPIO          GPIO_NUM_37  // 仅输入引脚
#define BUTTON3_GPIO          GPIO_NUM_38  // 仅输入引脚
#define BUTTON4_GPIO          GPIO_NUM_39  // 仅输入引脚
#define BUTTON_PULLUP_EN      1            // 启用内部上拉电阻

// ========================== 未分配 ==========================
#define LIGHT_SENSOR_GPIO     GPIO_NUM_16  // 空闲待定

#endif // CONFIG_H
