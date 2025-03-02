/*
 * A V4L2 driver for sc4336 Raw cameras.
 *
 * Copyright (c) 2021 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("lwj");
MODULE_DESCRIPTION("A low-level driver for sc4336 sensors");
MODULE_LICENSE("GPL");

#define MCLK				(27 * 1000 * 1000)
#define V4L2_IDENT_SENSOR	0xdc42
#define SENSOR_HOR_VER_CFG0_REG1 1

/*
 * The sc4336 i2c address
 */
#define I2C_ADDR			0x60      //0x60   0x64

#define SENSOR_NUM			0x1
#define SENSOR_NAME			"sc4336_mipi"

static int sc4336_sensor_vts;

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},

	{0x36e9, 0x80},
	{0x37f9, 0x80},
	{0x301f, 0x01},
	{0x30b8, 0x44},

	{0x320c, 0x0a},//HTS=2800
	{0x320d, 0xf0},
	//25FPS
	{0x320e, 0x07},//VTS=1800
	{0x320f, 0x08},

	//30FPS
	//0x320e,0x05,//VTS=1500
	//0x320f,0xdc,
	{0x3250, 0x40},
	{0x3253, 0x10},
	{0x3301, 0x0a},
	{0x3302, 0xff},
	{0x3305, 0x00},
	{0x3306, 0x90},
	{0x3308, 0x08},
	{0x330a, 0x01},
	{0x330b, 0xb0},
	{0x330d, 0xf0},
	{0x3333, 0x10},
	{0x335e, 0x06},
	{0x335f, 0x0a},
	{0x3364, 0x5e},
	{0x337d, 0x0e},
	{0x338f, 0x20},
	{0x3390, 0x08},
	{0x3391, 0x09},
	{0x3392, 0x0f},
	{0x3393, 0x18},
	{0x3394, 0x60},
	{0x3395, 0xff},
	{0x3396, 0x08},
	{0x3397, 0x09},
	{0x3398, 0x0f},
	{0x3399, 0x0a},
	{0x339a, 0x18},
	{0x339b, 0x60},
	{0x339c, 0xff},
	{0x33a2, 0x04},
	{0x33ad, 0x0c},
	{0x33b2, 0x40},
	{0x33b3, 0x30},
	{0x33f8, 0x00},
	{0x33f9, 0xa0},
	{0x33fa, 0x00},
	{0x33fb, 0xe0},
	{0x33fc, 0x09},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x09},
	{0x34a7, 0x1f},
	{0x34a8, 0x28},
	{0x34a9, 0x28},
	{0x34aa, 0x01},
	{0x34ab, 0xd0},
	{0x34ac, 0x02},
	{0x34ad, 0x10},
	{0x34f8, 0x1f},
	{0x34f9, 0x20},
	{0x3630, 0xc0},
	{0x3631, 0x84},
	{0x3633, 0x44},
	{0x3637, 0x4c},
	{0x3641, 0x38},
	{0x3670, 0x56},
	{0x3674, 0xc0},
	{0x3675, 0xa0},
	{0x3676, 0xa0},
	{0x3677, 0x84},
	{0x3678, 0x88},
	{0x3679, 0x8d},
	{0x367c, 0x09},
	{0x367d, 0x0b},
	{0x367e, 0x08},
	{0x367f, 0x0f},
	{0x3696, 0x44},
	{0x3697, 0x54},
	{0x3698, 0x54},
	{0x36a0, 0x0f},
	{0x36a1, 0x1f},
	{0x36b0, 0x81},
	{0x36b1, 0x83},
	{0x36b2, 0x85},
	{0x36b3, 0x8b},
	{0x36b4, 0x09},
	{0x36b5, 0x0b},
	{0x36b6, 0x0f},
	{0x370f, 0x01},
	{0x3722, 0x09},
	{0x3724, 0x21},
	{0x3771, 0x09},
	{0x3772, 0x05},
	{0x3773, 0x05},
	{0x377a, 0x0f},
	{0x377b, 0x1f},
	{0x3905, 0x8c},
	{0x391d, 0x04},
	{0x3926, 0x21},
	{0x3933, 0x80},
	{0x3934, 0x03},
	{0x3935, 0x00},
	{0x3936, 0x08},
	{0x3937, 0x74},
	{0x3938, 0x6f},
	{0x3939, 0x00},
	{0x393a, 0x00},
	{0x39dc, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x5d},
	{0x3e02, 0x40},
	{0x440e, 0x02},
	{0x4509, 0x28},
	{0x450d, 0x32},
	{0x4800, 0x44},
	{0x4816, 0x51},
	{0x5000, 0x06},
	{0x5799, 0x46},
	{0x579a, 0x77},
	{0x57d9, 0x46},
	{0x57da, 0x77},
	{0x5ae0, 0xfe},
	{0x5ae1, 0x40},
	{0x5ae2, 0x38},
	{0x5ae3, 0x30},
	{0x5ae4, 0x28},
	{0x5ae5, 0x38},
	{0x5ae6, 0x30},
	{0x5ae7, 0x28},
	{0x5ae8, 0x3f},
	{0x5ae9, 0x34},
	{0x5aea, 0x2c},
	{0x5aeb, 0x3f},
	{0x5aec, 0x34},
	{0x5aed, 0x2c},
	{0x36e9, 0x44},
	{0x37f9, 0x44},
	{0x3221, 0x66},
	{0x0100, 0x01},
};



/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sc4336_sensor_vts;
static int sc4336_fps_change_flag;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

	exphigh = (unsigned char) (0x0f & (exp_val>>16));
	expmid = (unsigned char) (0xff & (exp_val>>8));
	explow = (unsigned char) (0xf0 & (exp_val<<0));
	sensor_dbg("exp_val = %d\n", exp_val);
	sensor_write(sd, 0x3e02, explow);
	sensor_write(sd, 0x3e01, expmid);
	sensor_write(sd, 0x3e00, exphigh);
#if 0
	sensor_read(sd, 0x3e00, &exphigh);
	sensor_read(sd, 0x3e01, &expmid);
	sensor_read(sd, 0x3e02, &explow);
	sensor_print("exphigh = 0x%x, expmid = 0x%x, explow = 0x%x\n", exphigh, expmid, explow);
#endif
	info->exp = exp_val;
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	data_type anagain = 0x00;
	data_type gaindiglow = 0x80;
	data_type gaindighigh = 0x00;
	int gain_tmp;

	gain_tmp = gain_val << 3;
	if (gain_val < 32) {// 16 * 2
		anagain = 0x00;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp;
	} else if (gain_val < 64) {//16 * 4
		anagain = 0x08;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200/ 1;
	} else if (gain_val < 128) {//16 * 8
		anagain = 0x09;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 2;
	} else if (gain_val < 256) {//16 * 16
		anagain = 0x0b;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 4;
	} else if (gain_val < 512) {//16 * 32
		anagain = 0x0f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 8;
	} else if (gain_val < 1024) {//16 * 32 * 2
		anagain = 0x1f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp * 100 / 200 / 16;
	} else if (gain_val < 2048) {//16 * 32 * 4
		anagain = 0x1f;
		gaindighigh = 0x01;
		gaindiglow = gain_tmp * 100 / 200 / 32;
	} else if (gain_val < 4096) {//16 * 32 * 8
		anagain = 0x1f;
		gaindighigh = 0x03;
		gaindiglow = gain_tmp * 100 / 200 / 64;
	} else if (gain_val < 8192) {//16 * 32 * 16
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = gain_tmp * 100 / 200 / 128;
	} else {
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = 0xfc;
	}

	sensor_write(sd, 0x3e09, (unsigned char)anagain);
	sensor_write(sd, 0x3e07, (unsigned char)gaindiglow);
	sensor_write(sd, 0x3e06, (unsigned char)gaindighigh);

	sensor_dbg("sensor_set_anagain = %d, 0x%x, 0x%x, 0x%x Done!\n", gain_val, anagain, gaindighigh, gaindiglow);
	//sensor_dbg("digital_gain = 0x%x, 0x%x Done!\n", gaindighigh, gaindiglow);
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int shutter, frame_length;
	int exp_val, gain_val;
//	data_type read_high = 0, read_low = 0;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	if (!sc4336_fps_change_flag) {
		shutter = exp_val >> 4;
		if (shutter > sc4336_sensor_vts - 8) {//12
			frame_length = shutter + 8;
		} else
			frame_length = sc4336_sensor_vts;
		sensor_write(sd, 0x320f, (frame_length & 0xff));
		sensor_write(sd, 0x320e, (frame_length >> 8));

//		sensor_read(sd, 0x320e, &read_high);
//		sensor_read(sd, 0x320f, &read_low);
//		sensor_print("0x41 = 0x%x, 0x42 = 0x%x\n", read_high, read_low);
	}

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;

}

static int sensor_s_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
//	data_type read_high = 0, read_low = 0;
	int sc4336_sensor_target_vts = 0;

	sc4336_fps_change_flag = 1;
	sc4336_sensor_target_vts = wsize->pclk/fps->fps/wsize->hts;

	if (sc4336_sensor_target_vts <= wsize->vts) { // the max fps = 20fps
		sc4336_sensor_target_vts = wsize->vts;
	} else if (sc4336_sensor_target_vts >= (wsize->pclk/wsize->hts)) { // the min fps = 1fps
		sc4336_sensor_target_vts = (wsize->pclk/wsize->hts) - 8;
	}

	sc4336_sensor_vts = sc4336_sensor_target_vts;
	sensor_dbg("target_fps = %d, sc4336_sensor_target_vts = %d, 0x320e = 0x%x, 0x320f = 0x%x\n", fps->fps,
		sc4336_sensor_target_vts, sc4336_sensor_target_vts >> 8, sc4336_sensor_target_vts & 0xff);
	sensor_write(sd, 0x320f, (sc4336_sensor_target_vts & 0xff));
	sensor_write(sd, 0x320e, (sc4336_sensor_target_vts >> 8));
//	sensor_read(sd, 0x320e, &read_high);
//	sensor_read(sd, 0x320f, &read_low);
//	sensor_print("[get write_times] 0x41 = 0x%x, 0x42 = 0x%x\n", read_high, read_low);
	sc4336_fps_change_flag = 0;

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	printk("into set sensor hfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("sensor_s_hflip -- 0x3221 = 0x%x\n", get_value);

#if (SENSOR_HOR_VER_CFG0_REG1 == 1) //h60ga change
	sensor_dbg("###### SENSOR_HOR_VER_CFG0_REG1 #####\n");
	if (enable)
		set_value = get_value & 0xf9;
	else
		set_value = get_value | 0x06;
#else
	if (enable)
		set_value = get_value | 0x06;
	else
		set_value = get_value & 0xf9;
#endif

	sensor_write(sd, 0x3221, set_value);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	printk("into set sensor vfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("sensor_s_vflip -- 0x3221 = 0x%x\n", get_value);

#if (SENSOR_HOR_VER_CFG0_REG1 == 1) //h60ga change
	sensor_dbg("###### SENSOR_HOR_VER_CFG0_REG1 #####\n");
	if (enable)
		set_value = get_value & 0x9f;
	else
		set_value = get_value | 0x60;
#else
	if (enable)
		set_value = get_value | 0x60;
	else
		set_value = get_value & 0x9f;
#endif

	sensor_write(sd, 0x3221, set_value);
	return 0;

}

/*
 *set && get sensor flip
 */
 static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	data_type get_value;

	sensor_read(sd, 0x3221, &get_value);
	sensor_dbg("-- read value:0x%X --\n", get_value);
	switch (get_value & 0x66) {
	case 0x00:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--BGGR hfilp set read value:0x%X --\n", get_value &  0x66);
		break;
	case 0x06:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--GBRG hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	case 0x60:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--GRBG hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	case 0x66:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--RGGB hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_write(sd, 0x0100, rdval&0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval|0x01);
	return ret;
}

static int sensor_get_temp(struct v4l2_subdev *sd,
				struct sensor_temp *temp)
{
	static int is_ht = 1;
	data_type rdval_high = 0;
	data_type rdval_low = 0;
	int rdval_total = 0;
	int ret = 0;

	ret = sensor_read(sd, 0x399c, &rdval_high);
	ret = sensor_read(sd, 0x399d, &rdval_low);
	rdval_total |= rdval_high << 8;
	rdval_total |= rdval_low;
	sensor_dbg("rdval_total = 0x%x\n", rdval_total);
	rdval_total /= 5;
	temp->temp = rdval_total;
	sensor_dbg("sensor_get_temperature = %d\n", temp->temp);

	if ((1 == is_ht) && (rdval_total < 70)) {
		sensor_write(sd, 0x57AA, 0x2A);
		sensor_write(sd, 0x57AD, 0x0D);
		is_ht = 0;
	} else if ((0 == is_ht) && (rdval_total > 71)) {
		sensor_write(sd, 0x57AA, 0xCD);
		sensor_write(sd, 0x57AD, 0x00);
		is_ht = 1;
	}

	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		ret = sensor_s_sw_stby(sd, STBY_ON);
		if (ret < 0)
			sensor_err("soft stby falied!\n");
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		usleep_range(10000, 12000);
		ret = sensor_s_sw_stby(sd, STBY_OFF);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
		cci_unlock(sd);
		break;
	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(100, 120);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(5000, 7000);
		vin_set_mclk(sd, ON);
		usleep_range(5000, 7000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(5000, 7000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
		cci_unlock(sd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	switch (val) {
	case 0:
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	unsigned int SENSOR_ID = 0;
	data_type rdval;
	int cnt = 0;

	sensor_read(sd, 0x3107, &rdval);
	SENSOR_ID |= (rdval << 8);
	sensor_read(sd, 0x3108, &rdval);
	SENSOR_ID |= (rdval);
	sensor_print("V4L2_IDENT_SENSOR = 0x%x\n", SENSOR_ID);

	while ((SENSOR_ID != V4L2_IDENT_SENSOR) && (cnt < 5)) {
		sensor_read(sd, 0x3107, &rdval);
		SENSOR_ID |= (rdval << 8);
		sensor_read(sd, 0x3108, &rdval);
		SENSOR_ID |= (rdval);
		sensor_print("retry = %d, V4L2_IDENT_SENSOR = %x\n",
			cnt, SENSOR_ID);
		cnt++;
		}
	if (SENSOR_ID != V4L2_IDENT_SENSOR)
		return -ENODEV;

	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 2560;
	info->height = 1440;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 25;	/* 25fps */

	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins,
				sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_SET_FPS:
		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_SENSOR_GET_TEMP:
		ret = sensor_get_temp(sd, (struct sensor_temp *)arg);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.regs = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
	{
	.width = 2560,
	.height = 1440,
	.hoffset = 0,
	.voffset = 0,
	.hts = 2800,
	.vts = 1800,
	.pclk = 126000000,
	.mipi_bps = 630000000,
	.fps_fixed = 25,
	.bin_factor = 1,
	.intg_min = 2 << 4,//1
	.intg_max = (1800 - 8) << 4,
	.gain_min = 1 << 4,
	.gain_max = 512 << 4,
	.regs = sensor_default_regs,
	.regs_size = ARRAY_SIZE(sensor_default_regs),
	.set_size = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_g_gain(sd, &ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_g_exp(sd, &ctrl->val);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
	case V4L2_CID_HFLIP:
		return sensor_s_hflip(sd, ctrl->val);
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	data_type get_value = 0;

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sc4336_sensor_vts = wsize->vts;

	sensor_read(sd, 0x3221, &get_value);

	sensor_print("s_fmt set width = %d, height = %d, 0x3221 = 0x%x\n", wsize->width,
		     wsize->height, get_value);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
		     info->current_wins->width, info->current_wins->height,
		     info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable)
		return 0;

	return sensor_reg_init(info);
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
	.s_stream = sensor_s_stream,
	.g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 4);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
			      65536 * 16, 1, 1);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

static int sensor_dev_id;

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int i;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[i]);
	} else {
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[sensor_dev_id++]);
	}

	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->time_hs = 0x15;//0x09
	info->exp = 0;
	info->gain = 0;

	return 0;
}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	int i;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		sd = cci_dev_remove_helper(client, &cci_drv[i]);
	} else {
		sd = cci_dev_remove_helper(client, &cci_drv[sensor_dev_id++]);
	}

	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver[] = {
	{
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id,
	},
};
static __init int init_sensor(void)
{
	int i, ret = 0;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		ret = cci_dev_init_helper(&sensor_driver[i]);

	return ret;
}

static __exit void exit_sensor(void)
{
	int i;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		cci_dev_exit_helper(&sensor_driver[i]);
}

module_init(init_sensor);
module_exit(exit_sensor);
