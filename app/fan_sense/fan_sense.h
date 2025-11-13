/**
 * @file
 * @brief APIs for Fan Sensing
 */

#ifndef __FAN_SENSE_H__
#define __FAN_SENSE_H__

void fan_sense_init(void);
void fan1_sense_evt_processor(uint8_t evt);
void fan2_sense_evt_processor(uint8_t evt);
void fan3_sense_evt_processor(uint8_t evt);
void fan4_sense_evt_processor(uint8_t evt);

#endif /* __FAN_SENSE_H__ */
