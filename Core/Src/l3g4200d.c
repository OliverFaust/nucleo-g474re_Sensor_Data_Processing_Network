/*
 * l3g4200d.c
 *
 *  Created on: Feb 22, 2026
 *      Author: oliver
 */

#include "l3g4200d.h"
#include "stdio.h"

#define REG_WHO_AM_I   0x0F
#define REG_CTRL1      0x20
#define REG_CTRL4      0x23
#define REG_OUT_X_L    0x28
#define REG_CTRL2      0x21
#define REG_CTRL3      0x22
#define REG_CTRL5      0x24
#define REG_STATUS     0x27

#define READ_BIT       0x80
#define AUTO_INC       0x40

static void cs_low(L3G4200D_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_RESET);
}

static void cs_high(L3G4200D_t *dev)
{
    HAL_GPIO_WritePin(dev->cs_port, dev->cs_pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef write_reg(L3G4200D_t *dev, uint8_t reg, uint8_t data)
{
    uint8_t tx[2] = {reg, data};

    cs_low(dev);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(dev->hspi, tx, 2, HAL_MAX_DELAY);
    cs_high(dev);

    return status;
}

HAL_StatusTypeDef read_reg(L3G4200D_t *dev, uint8_t reg, uint8_t *data)
{
    uint8_t tx[2] = {reg | READ_BIT, 0};
    uint8_t rx[2];

    cs_low(dev);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 2, HAL_MAX_DELAY);
    cs_high(dev);

    *data = rx[1];
    return status;
}

/**
 * @brief Enables Data Ready interrupt on the INT1 pin of the sensor
 */
HAL_StatusTypeDef L3G4200D_EnableINT1(L3G4200D_t *dev)
{
    // CTRL_REG3 (0x22): Bit 3 is I1_DRDY
    // Setting this to 1 routes the "Data Ready" signal to the INT1 pin
    uint8_t ctrl3 = 0x08;
    return write_reg(dev, REG_CTRL3, ctrl3);
}

HAL_StatusTypeDef L3G4200D_Init(L3G4200D_t *dev, L3G4200D_Scale_t scale)
{
    uint8_t id;
    uint8_t verify;
    cs_high(dev);  // Ensure CS is High
    HAL_Delay(5);  // Short "settling" delay
    const int MAX_RETRIES = 100;
    uint8_t success = 0;
	const uint8_t EXPECTED_ID = 0xD3;

    // 2. Retry loop for WHO_AM_I
	for (int i = 0; i < MAX_RETRIES; i++) {
		if (read_reg(dev, REG_WHO_AM_I, &id) == HAL_OK) {
			if (id == EXPECTED_ID) {
				success = 1;
				break; // Found it! Exit the loop early.
			}
		}
		// Small delay to let the bus/sensor settle before trying again
		HAL_Delay(1);
	}

	if (!success) {
		printf("L3G4200D Fault: WHO_AM_I failed after %d attempts. Last value: 0x%02X\r\n", MAX_RETRIES, id);
		return HAL_ERROR;
	}

    // 1. Force a Reboot of the internal memory (Reset everything)
    // REG_CTRL5 is 0x24. Bit 7 is BOOT.
    write_reg(dev, 0x24, 0x80);
    HAL_Delay(10); // Give it time to reload from flash
    read_reg(dev, 0x24, &verify);
    printf("REG_CTRL5 = 0x%02X\r\n", verify);

    // 2. Set ODR and Power Mode (CTRL1: 0x20)
    // 0x0F = 100Hz ODR, 12.5 BW, Normal Mode, All axes enabled
    write_reg(dev, 0x20, 0x0F);
    read_reg(dev, 0x20, &verify);
    printf("REG_CTRL1 = 0x%02X\r\n", verify);

    // 3. Disable High Pass Filter (CTRL2: 0x21)
    write_reg(dev, 0x21, 0x00);
    read_reg(dev, 0x21, &verify);
    printf("REG_CTRL2 = 0x%02X\r\n", verify);

    // 4. Set Scale and Disable BDU for now (CTRL4: 0x23)
    // We set BDU=0 (Bit 7) to ensure registers update continuously
    // regardless of when we read them.
    uint8_t ctrl4 = 0x00;
    switch(scale) {
        case L3G4200D_SCALE_250DPS:  ctrl4 |= (0x00 << 4); dev->sensitivity = 8.75f; break;
        case L3G4200D_SCALE_500DPS:  ctrl4 |= (0x01 << 4); dev->sensitivity = 17.5f; break;
        case L3G4200D_SCALE_2000DPS: ctrl4 |= (0x02 << 4); dev->sensitivity = 70.0f; break;
    }
    write_reg(dev, 0x23, ctrl4);
    read_reg(dev, 0x23, &verify);
    printf("REG_CTRL4 = 0x%02X\r\n", verify);

    // 5. Verification: Read back CTRL1 to make sure the write "stuck"
    read_reg(dev, 0x20, &verify);
    if (verify != 0x0F) return HAL_ERROR;

    return HAL_OK;
}

HAL_StatusTypeDef L3G4200D_ReadRaw(L3G4200D_t *dev, int16_t *x, int16_t *y, int16_t *z)
{
	uint8_t tx[7] = {
	    REG_OUT_X_L | READ_BIT | AUTO_INC,
	    0,0,0,0,0,0
	};
    uint8_t rx[7];

    cs_low(dev);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 7, HAL_MAX_DELAY);
    cs_high(dev);

    *x = (int16_t)(rx[2] << 8 | rx[1]);
    *y = (int16_t)(rx[4] << 8 | rx[3]);
    *z = (int16_t)(rx[6] << 8 | rx[5]);

    return status;
}

HAL_StatusTypeDef L3G4200D_ReadDPS(L3G4200D_t *dev, float *x, float *y, float *z)
{
    int16_t rx, ry, rz;

    L3G4200D_ReadRaw(dev, &rx, &ry, &rz);

    *x = rx * dev->sensitivity / 1000.0f;
    *y = ry * dev->sensitivity / 1000.0f;
    *z = rz * dev->sensitivity / 1000.0f;

    return HAL_OK;
}




