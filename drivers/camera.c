#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/pinctrl/consumer.h>
#include <linux/device.h>
#include <media/v4l2-ctrls.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <media/v4l2-subdev.h>
#include <media/media-entity.h>
#include <media/v4l2-async.h>
#include <media/v4l2-mediabus.h>
#include <linux/rk-camera-module.h>
#define REG_NULL 0xffff
#define IMX415_REG_CTRL_MODE       0x3000
#define IMX415_MODE_STREAMING      0x00
#define IMX415_MODE_SW_STANDBY     0x01
#define IMX415_4LANES 4
#define IMX415_LINK_FREQ_891M 445500000ULL
#define IMX415_PIXEL_RATE    (IMX415_LINK_FREQ_891M / 10 * 2 * IMX415_4LANES)
#define IMX415_WIDTH		3864
#define IMX415_HEIGHT		2192
#define IMX415_VTS_DEF		2250
#define IMX415_HTS_DEF		8800
#define IMX415_EXPOSURE_DEF	2242
#define IMX415_EXPOSURE_MIN	4
#define IMX415_GAIN_MIN		0
#define IMX415_GAIN_MAX		240
#define IMX415_GAIN_DEF		0
#define CROP_START(SRC, DST) (((SRC) - (DST)) / 2 / 4 * 4)
#define DST_WIDTH_3840 3840
#define DST_HEIGHT_2160 2160
struct imx415_test{
    struct i2c_client *client;
    struct clk *xvclk;
    struct gpio_desc *reset_gpio;
    struct gpio_desc *power_gpio;
    struct v4l2_subdev sd;
    struct media_pad pad;
    struct v4l2_mbus_framefmt fmt;
    struct pinctrl *pinctrl;
    struct pinctrl_state *pins_default;
    struct pinctrl_state *pins_sleep;
    struct v4l2_ctrl_handler ctrl_handler;
    struct v4l2_ctrl *pixel_rate;
    struct v4l2_ctrl *link_freq;
    struct v4l2_ctrl *hblank;
    struct v4l2_ctrl *vblank;
    struct v4l2_ctrl *exposure;
    struct v4l2_ctrl *anal_gain;
    u32 cur_vts;
};
struct regval {
    u16 reg;
    u8 val;
};

static __maybe_unused const struct regval imx415_global_10bit_3864x2192_regs[] = {
	{0x3002, 0x00},
	{0x3008, 0x7F},
	{0x300A, 0x5B},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x30C1, 0x00},
	{0x30D9, 0x06},
	{0x3116, 0x24},
	{0x311E, 0x24},
	{0x32D4, 0x21},
	{0x32EC, 0xA1},
	{0x3452, 0x7F},
	{0x3453, 0x03},
	{0x358A, 0x04},
	{0x35A1, 0x02},
	{0x36BC, 0x0C},
	{0x36CC, 0x53},
	{0x36CD, 0x00},
	{0x36CE, 0x3C},
	{0x36D0, 0x8C},
	{0x36D1, 0x00},
	{0x36D2, 0x71},
	{0x36D4, 0x3C},
	{0x36D6, 0x53},
	{0x36D7, 0x00},
	{0x36D8, 0x71},
	{0x36DA, 0x8C},
	{0x36DB, 0x00},
	{0x3701, 0x00},
	{0x3724, 0x02},
	{0x3726, 0x02},
	{0x3732, 0x02},
	{0x3734, 0x03},
	{0x3736, 0x03},
	{0x3742, 0x03},
	{0x3862, 0xE0},
	{0x38CC, 0x30},
	{0x38CD, 0x2F},
	{0x395C, 0x0C},
	{0x3A42, 0xD1},
	{0x3A4C, 0x77},
	{0x3AE0, 0x02},
	{0x3AEC, 0x0C},
	{0x3B00, 0x2E},
	{0x3B06, 0x29},
	{0x3B98, 0x25},
	{0x3B99, 0x21},
	{0x3B9B, 0x13},
	{0x3B9C, 0x13},
	{0x3B9D, 0x13},
	{0x3B9E, 0x13},
	{0x3BA1, 0x00},
	{0x3BA2, 0x06},
	{0x3BA3, 0x0B},
	{0x3BA4, 0x10},
	{0x3BA5, 0x14},
	{0x3BA6, 0x18},
	{0x3BA7, 0x1A},
	{0x3BA8, 0x1A},
	{0x3BA9, 0x1A},
	{0x3BAC, 0xED},
	{0x3BAD, 0x01},
	{0x3BAE, 0xF6},
	{0x3BAF, 0x02},
	{0x3BB0, 0xA2},
	{0x3BB1, 0x03},
	{0x3BB2, 0xE0},
	{0x3BB3, 0x03},
	{0x3BB4, 0xE0},
	{0x3BB5, 0x03},
	{0x3BB6, 0xE0},
	{0x3BB7, 0x03},
	{0x3BB8, 0xE0},
	{0x3BBA, 0xE0},
	{0x3BBC, 0xDA},
	{0x3BBE, 0x88},
	{0x3BC0, 0x44},
	{0x3BC2, 0x7B},
	{0x3BC4, 0xA2},
	{0x3BC8, 0xBD},
	{0x3BCA, 0xBD},
	{0x4004, 0x48},
	{0x4005, 0x09},
	{REG_NULL, 0x00},
};
static __maybe_unused const struct regval imx415_linear_10bit_3864x2192_891M_regs[] = {
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3024, 0xCA},
	{0x3025, 0x08},
	{0x3028, 0x4C},
	{0x3029, 0x04},
	{0x302C, 0x00},
	{0x302D, 0x00},
	{0x3033, 0x05},
	{0x3050, 0x08},
	{0x3051, 0x00},
	{0x3054, 0x19},
	{0x3058, 0x3E},
	{0x3060, 0x25},
	{0x3064, 0x4a},
	{0x30CF, 0x00},
	{0x3118, 0xC0},
	{0x3260, 0x01},
	{0x400C, 0x00},
	{0x4018, 0x7F},
	{0x401A, 0x37},
	{0x401C, 0x37},
	{0x401E, 0xF7},
	{0x401F, 0x00},
	{0x4020, 0x3F},
	{0x4022, 0x6F},
	{0x4024, 0x3F},
	{0x4026, 0x5F},
	{0x4028, 0x2F},
	{0x4074, 0x01},
	{REG_NULL, 0x00},
};

static inline struct imx415_test *to_imx415_test(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx415_test, sd);
}

static int imx415_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
    int ret;
    u8 buf[3];
    buf[0] = reg >> 8;
    buf[1] = reg & 0xff;
    buf[2] = val;
    ret = i2c_master_send(client, buf, 3);
    if (ret != 3) {
		dev_err(&client->dev,
			"Failed to write reg 0x%04x=0x%02x, ret=%d\n",
			reg, val, ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

static int imx415_write_array(struct i2c_client *client, const struct regval *regs)
{
    int ret,i;
    for(i=0; regs[i].reg != REG_NULL; i++){
        ret = imx415_write_reg(client, regs[i].reg, regs[i].val);
        if(ret){
            dev_err(&client->dev, "Failed to write reg 0x%04x\n", regs[i].reg);
            return ret;
        }
    }
    return 0;
}

// static int imx415_read_reg(struct i2c_client *client, u16 reg, u8 *val)
// {
//     struct i2c_msg zzj[2];
//     u8 reg_buf[2];
//     int ret;
//     reg_buf[0] = reg >> 8;
//     reg_buf[1] = reg & 0xff;
//     zzj[0].addr = client->addr;
//     zzj[0].flags = 0;
//     zzj[0].len = 2;
//     zzj[0].buf = reg_buf;
//     zzj[1].addr = client->addr;
//     zzj[1].flags = 1;
//     zzj[1].len = 1;
//     zzj[1].buf = val;
//     ret = i2c_transfer(client->adapter, zzj, 2);
//     if (ret != 2) {
//         dev_err(&client->dev, "i2c read error: %d\n", ret);
//         return ret < 0 ? ret : -EIO;
//     }
//     return 0;
// }
static const s64 imx415_link_freq_menu[] = {
	IMX415_LINK_FREQ_891M,
};
static int imx415_set_ctrl(struct v4l2_ctrl *ctrl)
{
	struct imx415_test *priv =
		container_of(ctrl->handler, struct imx415_test, ctrl_handler);
	struct i2c_client *client = priv->client;
	u32 val;
	u32 shr0;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_VBLANK:
		priv->cur_vts = 2192 + ctrl->val;

		ret = imx415_write_reg(client, 0x3024, priv->cur_vts & 0xff);
		ret |= imx415_write_reg(client, 0x3025, (priv->cur_vts >> 8) & 0xff);
		ret |= imx415_write_reg(client, 0x3026, (priv->cur_vts >> 16) & 0x0f);
		break;

	case V4L2_CID_EXPOSURE:
		if (ctrl->val >= priv->cur_vts)
			val = priv->cur_vts - 8;
		else
			val = ctrl->val;

		shr0 = priv->cur_vts - val;

		ret = imx415_write_reg(client, 0x3050, shr0 & 0xff);
		ret |= imx415_write_reg(client, 0x3051, (shr0 >> 8) & 0xff);
		ret |= imx415_write_reg(client, 0x3052, (shr0 >> 16) & 0x0f);
		break;

	case V4L2_CID_ANALOGUE_GAIN:
		ret = imx415_write_reg(client, 0x3090, ctrl->val & 0xff);
		ret |= imx415_write_reg(client, 0x3091, (ctrl->val >> 8) & 0x07);
		break;

	default:
		break;
	}

	return ret;
}
static const struct v4l2_ctrl_ops imx415_ctrl_ops = {
	.s_ctrl = imx415_set_ctrl,
};
static int imx415_init_controls(struct imx415_test *priv)
{
	struct v4l2_ctrl_handler *handler;
	u32 hblank;
	u32 vblank_def;
	int ret;

	handler = &priv->ctrl_handler;
	ret = v4l2_ctrl_handler_init(handler, 6);
	if (ret)
		return ret;

	priv->cur_vts = IMX415_VTS_DEF;

	priv->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
			V4L2_CID_LINK_FREQ,
			ARRAY_SIZE(imx415_link_freq_menu) - 1,
			0,
			imx415_link_freq_menu);

	priv->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
			V4L2_CID_PIXEL_RATE,
			0,
			IMX415_PIXEL_RATE,
			1,
			IMX415_PIXEL_RATE);

	hblank = IMX415_HTS_DEF - IMX415_WIDTH;
	priv->hblank = v4l2_ctrl_new_std(handler, NULL,
			V4L2_CID_HBLANK,
			hblank,
			hblank,
			1,
			hblank);

	if (priv->hblank)
		priv->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	vblank_def = IMX415_VTS_DEF - IMX415_HEIGHT;
	priv->vblank = v4l2_ctrl_new_std(handler, &imx415_ctrl_ops,
			V4L2_CID_VBLANK,
			vblank_def,
			0xffff - IMX415_HEIGHT,
			1,
			vblank_def);

	priv->exposure = v4l2_ctrl_new_std(handler, &imx415_ctrl_ops,
			V4L2_CID_EXPOSURE,
			IMX415_EXPOSURE_MIN,
			IMX415_VTS_DEF - 8,
			1,
			IMX415_EXPOSURE_DEF);

	priv->anal_gain = v4l2_ctrl_new_std(handler, &imx415_ctrl_ops,
			V4L2_CID_ANALOGUE_GAIN,
			IMX415_GAIN_MIN,
			IMX415_GAIN_MAX,
			1,
			IMX415_GAIN_DEF);

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
		return ret;
	}

	priv->sd.ctrl_handler = handler;

	return 0;
}


static int imx415_test_get_selection(struct v4l2_subdev *sd,
    struct v4l2_subdev_pad_config *cfg,
    struct v4l2_subdev_selection *sel)
{
    if (sel->target != V4L2_SEL_TGT_CROP_BOUNDS)
        return -EINVAL;

    sel->r.left = CROP_START(3864, DST_WIDTH_3840);   // 12
    sel->r.width = DST_WIDTH_3840;                    // 3840
    sel->r.top = CROP_START(2192, DST_HEIGHT_2160);   // 16
    sel->r.height = DST_HEIGHT_2160;                  // 2160

    return 0;
}
static int imx415_start_stream(struct imx415_test *priv)
{
	struct i2c_client *client = priv->client;
	int ret;

	ret = imx415_write_array(client, imx415_global_10bit_3864x2192_regs);
	if (ret)
		return ret;

	ret = imx415_write_array(client, imx415_linear_10bit_3864x2192_891M_regs);
	if (ret)
		return ret;

	ret = __v4l2_ctrl_handler_setup(&priv->ctrl_handler);
	if (ret)
		return ret;

	ret = imx415_write_reg(client, IMX415_REG_CTRL_MODE, IMX415_MODE_STREAMING);
	if (ret)
		return ret;

	dev_info(&client->dev, "Streaming started\n");
	return 0;
}

static int imx415_test_enum_mbus_code(struct v4l2_subdev *sd,
     struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_mbus_code_enum *code)
{
    if (code->index != 0)
        return -EINVAL;
    code->code = MEDIA_BUS_FMT_SGBRG10_1X10;
    return 0;
}

static int imx415_stop_stream(struct i2c_client *client)
{
    int ret;
    ret = imx415_write_reg(client, IMX415_REG_CTRL_MODE, IMX415_MODE_SW_STANDBY);
    if (ret) {
        dev_err(&client->dev, "Failed to stop streaming\n");
        return ret;
    }
    dev_info(&client->dev, "Streaming stopped\n");
    return 0;
}

static int imx415_test_v4l2_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct imx415_test *priv = to_imx415_test(sd);
    if (enable) {
        return imx415_start_stream(priv);
    }
    else {
        return imx415_stop_stream(priv->client);
    }
}
static long imx415_test_ioctl(struct v4l2_subdev *sd,
			      unsigned int cmd, void *arg)
{
	struct rkmodule_inf *inf;
	long ret = 0;

	dev_info(&to_imx415_test(sd)->client->dev,
		 "zzj ioctl cmd=0x%x\n", cmd);

	switch (cmd) {
	case RKMODULE_GET_MODULE_INFO:
		inf = (struct rkmodule_inf *)arg;
		memset(inf, 0, sizeof(*inf));

		strlcpy(inf->base.sensor, "imx415",
			sizeof(inf->base.sensor));
		strlcpy(inf->base.module, "CMK-OT1522-FG3",
			sizeof(inf->base.module));
		strlcpy(inf->base.lens, "CS-P1150-IRC-8M-FAU",
			sizeof(inf->base.lens));

		dev_info(&to_imx415_test(sd)->client->dev,
			 "module info: sensor=%s module=%s lens=%s\n",
			 inf->base.sensor, inf->base.module, inf->base.lens);
		break;

	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
static int imx415_test_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *config)
{
    u32 val = 0;
    val = 1<< (IMX415_4LANES - 1)| V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
    config->type = V4L2_MBUS_CSI2;
    config->flags = val;
    return 0;
}

static int imx415_test_g_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *fi)
{
	fi->interval.numerator = 10000;
	fi->interval.denominator = 300000;
	return 0;
}

static int imx415_test_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
    struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index != 0)
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_SGBRG10_1X10)
		return -EINVAL;

	fse->min_width = 3864;
	fse->max_width = 3864;
	fse->min_height = 2192;
	fse->max_height = 2192;

	return 0;
}


static int imx415_test_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->index != 0)
		return -EINVAL;

	if (fie->code != MEDIA_BUS_FMT_SGBRG10_1X10)
		return -EINVAL;

	if (fie->width != 3864 || fie->height != 2192)
		return -EINVAL;

	fie->interval.numerator = 10000;
	fie->interval.denominator = 300000;

	return 0;
}

static int imx415_test_v4l2_get_fmt(struct v4l2_subdev *sd, 
    struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *fmt)
{
    struct imx415_test *priv = to_imx415_test(sd);
    fmt->format = priv->fmt;
    return 0;
}

static int imx415_test_v4l2_set_fmt(struct v4l2_subdev *sd,
    struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_format *fmt)
{
    struct imx415_test *priv = to_imx415_test(sd);
    priv->fmt.width = 3864;
    priv->fmt.height = 2192;
    priv->fmt.code = MEDIA_BUS_FMT_SGBRG10_1X10;
    priv->fmt.field = V4L2_FIELD_NONE;
    fmt->format = priv->fmt;
    return 0;
}


static int imx415_power_on(struct imx415_test *priv)
{
    int ret;
    if (!IS_ERR_OR_NULL(priv->pinctrl) &&!IS_ERR_OR_NULL(priv->pins_default))
        pinctrl_select_state(priv->pinctrl, priv->pins_default);
    
    if (priv->power_gpio){
        gpiod_set_value_cansleep(priv->power_gpio, 1);
        usleep_range(10000, 20000);
    }
    ret = clk_set_rate(priv->xvclk, 37125000);
	if (ret)
		dev_warn(&priv->client->dev, "failed to set xvclk rate, ret=%d\n", ret);

	ret = clk_prepare_enable(priv->xvclk);
	if (ret) {
		dev_err(&priv->client->dev, "failed to enable xvclk, ret=%d\n", ret);
		return ret;
	}

	dev_info(&priv->client->dev, "xvclk rate = %lu\n", clk_get_rate(priv->xvclk));
    usleep_range(10000, 20000);
    if (priv->reset_gpio){
            gpiod_set_value_cansleep(priv->reset_gpio, 1);
            usleep_range(10000, 20000);
            gpiod_set_value_cansleep(priv->reset_gpio, 0);
            usleep_range(10000, 20000);
    }
    return 0;
}

static void imx415_power_off(struct imx415_test *priv)
{
    if(priv->reset_gpio){
        gpiod_set_value_cansleep(priv->reset_gpio, 0);
        usleep_range(10000, 20000);
    }
    if(priv->xvclk){
        clk_disable_unprepare(priv->xvclk);
        usleep_range(10000, 20000);
    }
    if(priv->power_gpio){
        gpiod_set_value_cansleep(priv->power_gpio, 0);
        usleep_range(10000, 20000);
    }
}

static const struct v4l2_subdev_core_ops imx415_test_core_ops = {
	.ioctl = imx415_test_ioctl,
};
static const struct v4l2_subdev_video_ops imx415_test_video_ops = {
    .s_stream = imx415_test_v4l2_s_stream,
    .g_frame_interval = imx415_test_g_frame_interval,
    .g_mbus_config = imx415_test_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops imx415_test_pad_ops = {
    .enum_mbus_code = imx415_test_enum_mbus_code,
    .enum_frame_size = imx415_test_enum_frame_size,
    .enum_frame_interval = imx415_test_enum_frame_interval,
    .get_fmt = imx415_test_v4l2_get_fmt,
    .set_fmt = imx415_test_v4l2_set_fmt,
	.get_selection = imx415_test_get_selection,
};

static const struct v4l2_subdev_ops imx415_test_subdev_ops = {
    .core = &imx415_test_core_ops,
    .video = &imx415_test_video_ops,
    .pad = &imx415_test_pad_ops,
};

static int imx415_test_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
    u8 idl = 0;
    int ret;
    struct imx415_test *priv;
    struct v4l2_subdev *sd;
    priv = devm_kzalloc(&client->dev, sizeof(struct imx415_test), GFP_KERNEL);
    sd = &priv->sd;
    dev_info(&client->dev, "i2c address: 0x%02x\n", client->addr);
    printk("imx415 test driver probe\n");
    priv->client = client;
    priv->xvclk = devm_clk_get(&client->dev, "xvclk");
    if (IS_ERR(priv->xvclk)) {
        dev_err(&client->dev, "Failed to get xvclk\n");
        return PTR_ERR(priv->xvclk);
    }
    priv->reset_gpio = devm_gpiod_get_optional(&client->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(priv->reset_gpio)) {
        dev_err(&client->dev, "Failed to get reset GPIO\n");
        return PTR_ERR(priv->reset_gpio);
    }

    priv->power_gpio = devm_gpiod_get_optional(&client->dev, "power", GPIOD_OUT_LOW);
    if (IS_ERR(priv->power_gpio)) {
        dev_err(&client->dev, "Failed to get power GPIO\n");
        return PTR_ERR(priv->power_gpio);
    }
    priv->pinctrl = devm_pinctrl_get(&client->dev);
    if (!IS_ERR(priv->pinctrl)) {
	    priv->pins_default = pinctrl_lookup_state(priv->pinctrl, "rockchip,camera_default");
	    if (IS_ERR(priv->pins_default)) {
		    dev_warn(&client->dev, "could not get default pinstate\n");
		    priv->pins_default = NULL;
	    }
	    priv->pins_sleep = pinctrl_lookup_state(priv->pinctrl, "rockchip,camera_sleep");
	    if (IS_ERR(priv->pins_sleep)) {
		    dev_warn(&client->dev, "could not get sleep pinstate\n");
		    priv->pins_sleep = NULL;
	    }
    } 
    else {
	    dev_warn(&client->dev, "could not get pinctrl\n");
	    priv->pinctrl = NULL;
	    priv->pins_default = NULL;
	    priv->pins_sleep = NULL;
    }
    dev_info(&client->dev, "Sensor ID: 0x%02x\n", idl);
    v4l2_i2c_subdev_init(sd,client, &imx415_test_subdev_ops);
    snprintf(sd->name, sizeof(sd->name),"m00_b_imx415 %s", dev_name(&client->dev));
    sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
    priv->pad.flags = MEDIA_PAD_FL_SOURCE;
    sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
    priv->fmt.width = 3864;
    priv->fmt.height = 2192;
    priv->fmt.code = MEDIA_BUS_FMT_SGBRG10_1X10;
    priv->fmt.field = V4L2_FIELD_NONE;
    ret = media_entity_pads_init(&sd->entity, 1, &priv->pad);
    if (ret) {
	    dev_err(&client->dev, "media_entity_pads_init failed\n");
	    return ret;
    }
    ret = imx415_init_controls(priv);
	imx415_power_on(priv);
    if (ret) {
	    dev_err(&client->dev, "failed to init controls\n");
	    return ret;
    }
    ret = v4l2_async_register_subdev_sensor_common(sd);
    if (ret) {
	    dev_err(&client->dev, "register subdev failed\n");
	    media_entity_cleanup(&sd->entity);
	    return ret;
    }
    dev_info(&client->dev, "v4l2 subdev registered\n");
    return 0;
}
static int imx415_test_remove(struct i2c_client *client)
{
    struct v4l2_subdev *sd = i2c_get_clientdata(client);
    struct imx415_test *priv = to_imx415_test(sd);
    v4l2_ctrl_handler_free(&priv->ctrl_handler);
    dev_info(&client->dev, "imx415 test driver remove\n");
    v4l2_async_unregister_subdev(&priv->sd);
	media_entity_cleanup(&priv->sd.entity);
    imx415_power_off(priv);
    return 0;
}
static const struct of_device_id imx415_test_of_match[] = {
    { .compatible = "zzj,imx415-test" },
    { /*无*/ }
};
static const struct i2c_device_id imx415_test_id[] = {
    { "imx415", 0 },
    { /*无*/ }
};
static struct i2c_driver imx415_driver = {
    .driver = {
        .name = "zzj_imx415",
        .of_match_table = imx415_test_of_match,
    },
    .probe = imx415_test_probe,
    .remove = imx415_test_remove,
    .id_table = imx415_test_id,
};
module_i2c_driver(imx415_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zzj");
MODULE_DESCRIPTION("RK3568 camera driver");