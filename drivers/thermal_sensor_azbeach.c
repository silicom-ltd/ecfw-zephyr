/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/kernel.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/adc.h>
#include "thermal_sensor.h"
#include "board_config.h"

LOG_MODULE_REGISTER(thrmsens, CONFIG_THERMAL_SENSOR_LOG_LEVEL);

/* ADC device */
static const struct device *adc_dev;

static uint8_t adc_ch_bits;

int16_t adc_temp_val[ADC_CH_TOTAL];

static uint8_t num_of_adc_ch;


static int16_t conv_adc_temp(uint16_t adc_raw_val)
{
	/* Conversion lookup table index for 0 degree celsius temperature */
#define CONV_TBL_ZERO_C_IDX	80

	/* Conversion Lookup table maximum negative temperature */
#define MAX_NEGATIVE_TEMP	-40

	/* Conversion lookup table maximum positive temperature index */
#define MAX_POSITIVE_TEMP_IDX	330

	struct adc_temp_conv_table {
		int16_t temperature;
		uint16_t raw_val;
	};

	int cnt;
	int16_t temperature = 0;

/**
 *
 * Thermistor ADC Voltage Calculation :
 * -------------------------------------------
 *
 * +--------------------------------------------------------+
 * |                                                        |
 * |    ADC_VREF(3.3v)        Theoretical Voltage           |
 * |         +                --------------------          |
 * |         |                                    Rth       |
 * |         |                  Vadc = Vref * -----------   |
 * |         |                                 (Rp + Rth)   |
 * |         X                                              |
 * |     Rp  X 3.3k                                         |
 * |         X                                              |
 * |         X                10 Bit ADC Value              |
 * |         |                --------------------          |
 * |         |    Vadc                      Vadc            |
 * |         +------>           Vadc_10b = ------ * 1024    |
 * |         |                              Vref            |
 * |         X                                              |
 * |   Rth   X  10k                                         |
 * |       +--->                                            |
 * |         X                12 Bit ADC Value              |
 * |         |                --------------------          |
 * |         |                              Vadc            |
 * |        +--+                Vadc_12b = ------ * 4096    |
 * |         ++                             Vref            |
 * |          +                                             |
 * |                                                        |
 * +--------------------------------------------------------+
 *
 *
 * This thermistor lookup table is driven for Murata thermistor part:
 * NCP15XH103F03RC with below circuit parameters:
 *
 *  Thermistor	> Rth: 10 kOhm [at 25c]- This value varies with temperature.
 *  Pull up Rest> Rp = 3.3 kOhm
 *  ADC_VREF	> Vref = 3.3 v
 *  Resolution	> 10 bit
 *
 * Any of above parameters ( / part) changed, this table has to be updated.
 * The temperature values are in order of 10, ranging from -40c to 125c.
 *
 * Note:
 * Thermistor datasheet provides the values of Rth only at few limited
 * temperature values ranging from -40c to 125c. Hence for more granular
 * range, liner interpolation is used to calculate the rest of the Rth
 * values.
 *
 */

	static const struct adc_temp_conv_table temp_conv_tbl[] = {
		/* Temp,RAW Value. -400 -> -40 deg, -395 -> -39.5 deg, etc */

		/* Negative temperature readings */
		{-400, 0x36E}, {-395, 0x36C}, {-390, 0x36A}, {-385, 0x367},
		{-380, 0x365}, {-375, 0x362}, {-370, 0x35F}, {-365, 0x35D},
		{-360, 0x35A}, {-355, 0x357}, {-350, 0x353}, {-345, 0x350},
		{-340, 0x34D}, {-335, 0x349}, {-330, 0x345}, {-325, 0x342},
		{-320, 0x33E}, {-315, 0x33A}, {-310, 0x335}, {-305, 0x331},
		{-300, 0x32C}, {-295, 0x327}, {-290, 0x322}, {-285, 0x31C},
		{-280, 0x316}, {-275, 0x310}, {-270, 0x30A}, {-265, 0x303},
		{-260, 0x2FC}, {-255, 0x2F5}, {-250, 0x2ED}, {-245, 0x2EA},
		{-240, 0x2E7}, {-235, 0x2E4}, {-230, 0x2E1}, {-225, 0x2DD},
		{-220, 0x2DA}, {-215, 0x2D7}, {-210, 0x2D3}, {-205, 0x2D0},
		{-200, 0x2CC}, {-195, 0x2C9}, {-190, 0x2C5}, {-185, 0x2C1},
		{-180, 0x2BD}, {-175, 0x2B9}, {-170, 0x2B5}, {-165, 0x2B1},
		{-160, 0x2AC}, {-155, 0x2A8}, {-150, 0x2A3}, {-145, 0x29E},
		{-140, 0x29A}, {-135, 0x295}, {-130, 0x28F}, {-125, 0x28A},
		{-120, 0x285}, {-115, 0x27F}, {-110, 0x279}, {-105, 0x273},
		{-100, 0x26D}, { -95, 0x267}, { -90, 0x260}, { -85, 0x25A},
		{ -80, 0x253}, { -75, 0x24B}, { -70, 0x244}, { -65, 0x23C},
		{ -60, 0x234}, { -55, 0x22C}, { -50, 0x224}, { -45, 0x21B},
		{ -40, 0x211}, { -35, 0x208}, { -30, 0x1FE}, { -25, 0x1F4},
		{ -20, 0x1E9}, { -15, 0x1DE}, { -10, 0x1D2}, { -05, 0x1C6},

		/* Positive temperature readings */
		{  00, 0x1B9}, {  05, 0x1B6}, {  10, 0x1B3}, {  15, 0x1B0},
		{  20, 0x1AD}, {  25, 0x1A9}, {  30, 0x1A6}, {  35, 0x1A3},
		{  40, 0x1A0}, {  45, 0x19C}, {  50, 0x199}, {  55, 0x196},
		{  60, 0x192}, {  65, 0x18F}, {  70, 0x18B}, {  75, 0x188},
		{  80, 0x184}, {  85, 0x181}, {  90, 0x17D}, {  95, 0x179},
		{ 100, 0x176}, { 105, 0x172}, { 110, 0x16E}, { 115, 0x16A},
		{ 120, 0x166}, { 125, 0x162}, { 130, 0x15E}, { 135, 0x15A},
		{ 140, 0x156}, { 145, 0x152}, { 150, 0x14E}, { 155, 0x14A},
		{ 160, 0x145}, { 165, 0x141}, { 170, 0x13D}, { 175, 0x138},
		{ 180, 0x134}, { 185, 0x12F}, { 190, 0x12A}, { 195, 0x126},
		{ 200, 0x121}, { 205, 0x11C}, { 210, 0x117}, { 215, 0x112},
		{ 220, 0x10D}, { 225, 0x108}, { 230, 0x103}, { 235, 0x0FE},
		{ 240, 0x0F8}, { 245, 0x0F3}, { 250, 0x0EE}, { 255, 0x0EB},
		{ 260, 0x0E9}, { 265, 0x0E7}, { 270, 0x0E5}, { 275, 0x0E3},
		{ 280, 0x0E0}, { 285, 0x0DE}, { 290, 0x0DC}, { 295, 0x0D9},
		{ 300, 0x0D7}, { 305, 0x0D5}, { 310, 0x0D2}, { 315, 0x0D0},
		{ 320, 0x0CE}, { 325, 0x0CB}, { 330, 0x0C9}, { 335, 0x0C7},
		{ 340, 0x0C4}, { 345, 0x0C2}, { 350, 0x0BF}, { 355, 0x0BD},
		{ 360, 0x0BA}, { 365, 0x0B8}, { 370, 0x0B5}, { 375, 0x0B3},
		{ 380, 0x0B0}, { 385, 0x0AE}, { 390, 0x0AB}, { 395, 0x0A8},
		{ 400, 0x0A6}, { 405, 0x0A3}, { 410, 0x0A1}, { 415, 0x09E},
		{ 420, 0x09B}, { 425, 0x099}, { 430, 0x096}, { 435, 0x093},
		{ 440, 0x090}, { 445, 0x08E}, { 450, 0x08B}, { 455, 0x088},
		{ 460, 0x085}, { 465, 0x082}, { 470, 0x080}, { 475, 0x07D},
		{ 480, 0x07A}, { 485, 0x077}, { 490, 0x074}, { 495, 0x071},
		{ 500, 0x06E}, { 505, 0x06D}, { 510, 0x06C}, { 515, 0x06B},
		{ 520, 0x06A}, { 525, 0x069}, { 530, 0x068}, { 535, 0x067},
		{ 540, 0x066}, { 545, 0x065}, { 550, 0x064}, { 555, 0x063},
		{ 560, 0x062}, { 565, 0x061}, { 570, 0x060}, { 575, 0x05F},
		{ 580, 0x05E}, { 585, 0x05D}, { 590, 0x05C}, { 595, 0x05B},
		{ 600, 0x05A}, { 605, 0x059}, { 610, 0x058}, { 615, 0x057},
		{ 620, 0x056}, { 625, 0x055}, { 630, 0x054}, { 635, 0x053},
		{ 640, 0x052}, { 645, 0x051}, { 650, 0x050}, { 655, 0x04F},
		{ 660, 0x04D}, { 665, 0x04C}, { 670, 0x04B}, { 675, 0x04A},
		{ 680, 0x049}, { 685, 0x048}, { 690, 0x047}, { 695, 0x046},
		{ 700, 0x045}, { 705, 0x044}, { 710, 0x043}, { 715, 0x042},
		{ 720, 0x041}, { 725, 0x040}, { 730, 0x03E}, { 735, 0x03D},
		{ 740, 0x03C}, { 745, 0x03B}, { 750, 0x03A}, { 755, 0x039},
		{ 760, 0x039}, { 765, 0x038}, { 770, 0x038}, { 775, 0x037},
		{ 780, 0x037}, { 785, 0x036}, { 790, 0x036}, { 795, 0x035},
		{ 800, 0x034}, { 805, 0x034}, { 810, 0x033}, { 815, 0x033},
		{ 820, 0x032}, { 825, 0x032}, { 830, 0x031}, { 835, 0x031},
		{ 840, 0x030}, { 845, 0x02F}, { 850, 0x02F}, { 855, 0x02E},
		{ 860, 0x02E}, { 865, 0x02D}, { 870, 0x02D}, { 875, 0x02C},
		{ 880, 0x02B}, { 885, 0x02B}, { 890, 0x02A}, { 895, 0x02A},
		{ 900, 0x029}, { 905, 0x029}, { 910, 0x028}, { 915, 0x027},
		{ 920, 0x027}, { 925, 0x026}, { 930, 0x026}, { 935, 0x025},
		{ 940, 0x025}, { 945, 0x024}, { 950, 0x023}, { 955, 0x023},
		{ 960, 0x022}, { 965, 0x022}, { 970, 0x021}, { 975, 0x021},
		{ 980, 0x020}, { 985, 0x01F}, { 990, 0x01F}, { 995, 0x01E},
		{1000, 0x01E}, {1005, 0x01D}, {1010, 0x01D}, {1015, 0x01D},
		{1020, 0x01C}, {1025, 0x01C}, {1030, 0x01C}, {1035, 0x01C},
		{1040, 0x01B}, {1045, 0x01B}, {1050, 0x01B}, {1055, 0x01A},
		{1060, 0x01A}, {1065, 0x01A}, {1070, 0x01A}, {1075, 0x019},
		{1080, 0x019}, {1085, 0x019}, {1090, 0x018}, {1095, 0x018},
		{1100, 0x018}, {1105, 0x017}, {1110, 0x017}, {1115, 0x017},
		{1120, 0x017}, {1125, 0x016}, {1130, 0x016}, {1135, 0x016},
		{1140, 0x015}, {1145, 0x015}, {1150, 0x015}, {1155, 0x014},
		{1160, 0x014}, {1165, 0x014}, {1170, 0x014}, {1175, 0x013},
		{1180, 0x013}, {1185, 0x013}, {1190, 0x012}, {1195, 0x012},
		{1200, 0x012}, {1205, 0x011}, {1210, 0x011}, {1215, 0x011},
		{1220, 0x011}, {1225, 0x010}, {1230, 0x010}, {1235, 0x010},
		{1240, 0x00F}, {1245, 0x00F}, {1250, 0x00E},
	};

	/* 0 temperature */
	if (adc_raw_val == temp_conv_tbl[CONV_TBL_ZERO_C_IDX].raw_val) {
		temperature = 0;
	}
	/* -ve temperature overflow */
	else if (adc_raw_val >= temp_conv_tbl[0].raw_val) {
		temperature = temp_conv_tbl[0].temperature;
	}
	/* +ve temperature overflow */
	else if (adc_raw_val <= temp_conv_tbl[MAX_POSITIVE_TEMP_IDX].raw_val) {
		temperature = temp_conv_tbl[MAX_POSITIVE_TEMP_IDX].temperature;
	}

	else if (adc_raw_val > temp_conv_tbl[CONV_TBL_ZERO_C_IDX].raw_val) {
		/* -ve temperature */
		for (cnt = 0; cnt <= CONV_TBL_ZERO_C_IDX; cnt++) {
			if (adc_raw_val >= temp_conv_tbl[cnt].raw_val) {
				temperature = temp_conv_tbl[cnt].temperature;
				break;
			}
		}
	} else {
		/* +ve temperature */
		for (cnt = CONV_TBL_ZERO_C_IDX; cnt <= MAX_POSITIVE_TEMP_IDX;
				cnt++) {
			if (adc_raw_val >= temp_conv_tbl[cnt].raw_val) {
				temperature = temp_conv_tbl[cnt].temperature;
				break;
			}
		}
	}

	return temperature;
}

int thermal_sensors_init(uint8_t adc_channel_bits)
{
	int ret = 0;
	int adc_ch = ADC_CH_00;

	num_of_adc_ch = 0;
	adc_ch_bits = adc_channel_bits;

	if (adc_channel_bits == 0) {
		LOG_ERR("No adc sensor to enable");
		return -ENOTSUP;
	}

	adc_dev = DEVICE_DT_GET(ADC_CH_BASE);

	if (!adc_dev) {
		LOG_ERR("Sensor device Invalid");
		return -ENOTSUP;
	}

	struct adc_channel_cfg adc_cfg;

	adc_cfg.gain = ADC_GAIN_1;
	adc_cfg.reference = ADC_REF_INTERNAL;
	adc_cfg.acquisition_time = ADC_ACQ_TIME_DEFAULT;
	adc_cfg.differential = 0;

	for (adc_ch = ADC_CH_00; adc_ch < ADC_CH_TOTAL; adc_ch++) {

		if (0 == (BIT(adc_ch) & adc_channel_bits)) {
			continue;
		}

		adc_cfg.channel_id = adc_ch;
		ret = adc_channel_setup(adc_dev, &adc_cfg);
		if (ret) {
			LOG_WRN("Sensor ch %d config failed", adc_ch);
		}

		/* Update count of adc channels enabled */
		num_of_adc_ch++;
	}

	return ret;
}


void thermal_sensors_update(void)
{
	if (!adc_dev) {
		LOG_WRN("ADC: Sensor device Invalid");
		return;
	}

	int ret;
	int16_t adc_raw_val[num_of_adc_ch];
	uint8_t ch, ch_cnt = 0;

	const struct adc_sequence sequence = {
		.channels	= adc_ch_bits,
		.buffer		= adc_raw_val,
		.buffer_size	= sizeof(adc_raw_val),
		.resolution	= 10,
	};

	ret = adc_read(adc_dev, &sequence);

	if (ret) {
		LOG_WRN("ADC Sensor reading failed %d", ret);
	}

	for (ch = ADC_CH_00; ch < ADC_CH_TOTAL; ch++) {
		if (adc_ch_bits & BIT(ch)) {
			adc_temp_val[ch] = conv_adc_temp(
					(adc_raw_val[ch_cnt++]));
		}

		LOG_DBG("ADC Ch %d : %d", ch, adc_temp_val[ch]);
	}
}

