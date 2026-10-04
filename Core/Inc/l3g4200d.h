/*
 * l3g4200d.h
 *
 *  Created on: Feb 22, 2026
 *      Author: oliver
 */

#ifndef SRC_L3G4200D_H_
#define SRC_L3G4200D_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>

typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    float sensitivity;
} L3G4200D_t;

typedef enum
{
    L3G4200D_SCALE_250DPS,
    L3G4200D_SCALE_500DPS,
    L3G4200D_SCALE_2000DPS
} L3G4200D_Scale_t;

HAL_StatusTypeDef L3G4200D_Init(L3G4200D_t *dev, L3G4200D_Scale_t scale);
uint8_t L3G4200D_ReadID(L3G4200D_t *dev);
HAL_StatusTypeDef L3G4200D_ReadRaw(L3G4200D_t *dev, int16_t *x, int16_t *y, int16_t *z);
HAL_StatusTypeDef L3G4200D_ReadDPS(L3G4200D_t *dev, float *x, float *y, float *z);
HAL_StatusTypeDef read_reg(L3G4200D_t *dev, uint8_t reg, uint8_t *data);
HAL_StatusTypeDef write_reg(L3G4200D_t *dev, uint8_t reg, uint8_t data);
HAL_StatusTypeDef L3G4200D_EnableDRDY(L3G4200D_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* SRC_L3G4200D_H_ */
