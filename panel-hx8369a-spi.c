/*
 * Himax HX8369A panel driver.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

#include <linux/lcd.h>
#include <linux/spi/spi.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#define ENTER_SLEEPMODE	0x10
#define EXIT_SLEEPMODE	0x11
#define SETDISPLAY_OFF	0x28
#define SETDISPLAY_ON	0x29

#define SETCLUMN_ADDR	0x2a
#define SETPAGE_ADDR 	0x2b
#define SETPIXEL_FMT 	0x3a
#define WRDISBV	     	0x51
#define WRCTRLD	     	0x53
#define WRCABC	     	0x55
#define SETPOWER     	0xb1
#define SETDISP	     	0xb2
#define SETRGB	     	0xb3
#define SETCYC	     	0xb4
#define SETVCOM	     	0xb6
#define SETEXTC	     	0xb9
#define SETMIPI	     	0xba
#define SETPANEL     	0xcc
#define SETGIP	     	0xd5
#define SETTEMP	     	0xd8
#define SETGAMMA     	0xe0

#define READ_MEM     	0x3e
#define GET_POWER_MODE 	0x0a

enum hx8369a_mpu_interface {
	HX8369A_DBI_TYPE_A_8BIT,
	HX8369A_DBI_TYPE_A_9BIT,
	HX8369A_DBI_TYPE_A_16BIT,
	HX8369A_DBI_TYPE_A_18BIT,
	HX8369A_DBI_TYPE_B_8BIT,
	HX8369A_DBI_TYPE_B_9BIT,
	HX8369A_DBI_TYPE_B_16BIT,
	HX8369A_DBI_TYPE_B_18BIT,
	HX8369A_DSI_CMD_MODE,
	HX8369A_DBI_TYPE_B_24BIT,
	HX8369A_DSI_VIDEO_MODE,
	HX8369A_MDDI,
	HX8369A_DPI_DBI_TYPE_C_OPT1,
	HX8369A_DPI_DBI_TYPE_C_OPT2,
	HX8369A_DPI_DBI_TYPE_C_OPT3
};

enum hx8369a_resolution {
	HX8369A_RES_480_864,
	HX8369A_RES_480_854,
	HX8369A_RES_480_800,
	HX8369A_RES_480_640,
	HX8369A_RES_360_640,
	HX8369A_RES_480_720,
	HX8369A_RES_DISABLE
};

struct hx8369a {
	struct device *dev;
	struct spi_device *spi;
	struct lcd_device *lcdev;
	struct display_timing timings; // не используется
	
	u32 power_on_delay; // не используется
	u32 reset_delay;	// не используется
	int reset;			// GPIO, отвечающий за сброс контроллера дисплея
	int vm_gpio;		// GPIO, отвечающий за переключение режимов SPI/DSI-video-mode
	
	u32 data_lanes;
	int pixel_format;	// цветность изображения, bits per pixel
	struct videomode vm;// не используется 
	u8 res_sel;			// разрешение дисплея
};

u8 read_temp_cmd[] = {SETTEMP,
					0x00, 0x12, 0x63,
					0xA7, 0x09, 0x67, 0x50,
					0x4E, 0x17, 0x75};

static int hx8369a_spi_write_then_read(struct hx8369a *dsi_panel,
				u8 *txbuf, u16 txlen,
				u8 *rxbuf, u16 rxlen)
{
	struct spi_device *spi = dsi_panel->spi;
	struct spi_message msg;
	struct spi_transfer xfer;
	u8 *local_txbuf = NULL;
	int ret = 0, j=0;

	memset(&xfer, 0, sizeof(xfer));
	spi_message_init(&msg);

	if (txlen) {
		int i;

		local_txbuf = kcalloc(txlen*2, sizeof(*local_txbuf), GFP_KERNEL);		

		if (!local_txbuf)
			return -ENOMEM;

		for (i = 0; i < txlen; i++) {
			if (i == 0) {
				local_txbuf[j] = 0; j++;
				local_txbuf[j] = txbuf[i]; j++; 
			} else {
				local_txbuf[j] = 1; j++;
				local_txbuf[j] = txbuf[i]; j++;
			}
		}

		xfer.len = 2*txlen;
		xfer.bits_per_word = 8;
		xfer.tx_buf = local_txbuf;
		spi_message_add_tail(&xfer, &msg);
	}
	
	ret = spi_sync(spi, &msg);
	if (ret < 0)
		dev_err(dsi_panel->dev, "Couldn't send SPI data\n");

	if (txlen)
		kfree(local_txbuf);

	return ret;
}

static int hx8369a_spi_write_then_read3(struct hx8369a *dsi_panel,
				u8 *txbuf, u8 txlen,
				u8 *rxbuf, u8 rxlen)
{
	struct spi_device *spi = dsi_panel->spi;
	struct spi_message msg;
	struct spi_transfer xfer;
	u8 *local_txbuf = NULL;
	u8 *str = NULL, *str2 = NULL;
	int ret = 0, i, j=0;

	memset(&xfer, 0, sizeof(xfer));
	spi_message_init(&msg);

	if (txlen) {		

		local_txbuf = kcalloc((txlen*2+rxlen), sizeof(*local_txbuf), GFP_KERNEL);

		if (!local_txbuf)
			return -ENOMEM;		
		memset(local_txbuf, 1, (txlen*2+rxlen));

		for (i = 0; i < txlen; i++) {
			if (i == 0) {
				local_txbuf[j] = 0; j++;
				local_txbuf[j] = txbuf[i]; j++;
			} else {
				local_txbuf[j] = 1; j++; 
				local_txbuf[j] = txbuf[i]; j++;
			}
		}

		xfer.len = 2*txlen + rxlen;
		xfer.bits_per_word = 8;
		xfer.tx_buf = local_txbuf;		
	}
	
	else {
		dev_err(dsi_panel->dev, "It is necessary to specify the number of bytes for transfer\n");
		ret =-1;
		return ret;
	}

	if (rxlen) 		
		xfer.rx_buf = local_txbuf + txlen*2;
		
	spi_message_add_tail(&xfer, &msg);
	
	/*str = kcalloc((2*txlen + rxlen)*8, sizeof(*local_txbuf), GFP_KERNEL);
	str2 = kcalloc(8, sizeof(*local_txbuf), GFP_KERNEL);
	*str = '\0';
	*str2 = '\0';
	for (i = 0; i < (txlen*2 + rxlen); i++) {
		sprintf(str2, "0x%02X ", local_txbuf[i]);
		strcat(str, str2);
	}
		
	dev_info(dsi_panel->dev, "%s\n", str);
	
	kfree(str);
	kfree(str2);*/
	
	ret = spi_sync(spi, &msg);
	if (ret < 0)
		dev_err(dsi_panel->dev, "Couldn't send SPI data\n");
		
	if (ret == 0)
		memcpy(rxbuf, xfer.rx_buf, rxlen);		

	if (txlen)
		kfree(local_txbuf);

	return ret;
}

static inline int hx8369a_spi_write_array(struct hx8369a *dsi_panel,
					u8 *value, u8 len)
{
	return hx8369a_spi_write_then_read(dsi_panel, value, len, NULL, 0);
}

static inline int hx8369a_spi_write_byte(struct hx8369a *dsi_panel,
					u8 value)
{
	return hx8369a_spi_write_then_read(dsi_panel, &value, 1, NULL, 0);
}

static void hx8369a_spi_write(struct hx8369a *dsi_panel, u8 *value, u8 len)
{
	hx8369a_spi_write_array(dsi_panel, value, len);
}

static inline int hx8369a_spi_read_bytes(struct hx8369a *dsi_panel,
					u8 value, u8 *read_bytes, u8 len)
{
	u8 *val = &value;
	return hx8369a_spi_write_then_read3(dsi_panel, val, 1, read_bytes, len);
}

static inline int hx8369a_spi_read_temperature(struct hx8369a *dsi_panel,
					u8 *cmd, u8 cmdlen, u8 *read_bytes, u8 rxlen)
{
	return hx8369a_spi_write_then_read3(dsi_panel, cmd, cmdlen, read_bytes, rxlen);
}

#define hx8369a_spi_write_seq(dsi_panel, seq...) \
({\
	u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	hx8369a_spi_write(dsi_panel, d, ARRAY_SIZE(d));\
})

#define hx8369a_spi_write_seq_static(dsi_panel, seq...) \
({\
	static u8 d[] = { seq };\
	hx8369a_spi_write(dsi_panel, d, ARRAY_SIZE(d));\
})

static void hx8369a_dsi_set_extension_command(struct hx8369a *dsi_panel)
{
	hx8369a_spi_write_seq_static(dsi_panel, SETEXTC, 0xff, 0x83, 0x69);
}

static void hx8369a_dsi_set_display_related_register(struct hx8369a *dsi_panel)
{
	/*u8 sec_p = (dsi_panel->res_sel << 4) | 0x03;

	hx8369a_spi_write_seq(dsi_panel, SETDISP, 0x00, sec_p, 0x03,
		0x03, 0x70, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
		0x03, 0x03, 0x00, 0x01);*/
		
	hx8369a_spi_write_seq(dsi_panel, SETDISP, 
							0x00, 0x2B /*0x28*/, /*0x02*/ 0x04, // значения 0x04 соответствуют 6-ти линиям front porch и back porch -
							/*0x02*/ 0x04,  0x70, 0x00, 0xFF,   // FP[7:0]: Specify the amount of scan line for front porch (FP).
																// BP[7:0] : Specify the amount of scan line for back porch(BP).
							0x00, 0x00, 0x00, 0x00,
							/*0x02, 0x02*/ 0x04, 0x04, 0x00, 0x01);
}

static void hx8369a_dsi_set_display_rgb(struct hx8369a *dsi_panel)
{
	
	hx8369a_spi_write_seq_static(dsi_panel, SETRGB, 0x01);
}

static void hx8369a_dsi_set_display_waveform_cycle(struct hx8369a *dsi_panel)
{
	//hx8369a_spi_write_seq_static(dsi_panel, SETCYC, 0x00, 0x1d, 0x5f, 0x0e, 0x06);
	
	hx8369a_spi_write_seq_static(dsi_panel, SETCYC, 0x00, 0x18, 0x80, 0x06, 0x02);
}

static void hx8369a_dsi_set_gip(struct hx8369a *dsi_panel)
{
	/*hx8369a_spi_write_seq_static(dsi_panel, SETGIP, 0x00, 0x04, 0x03, 0x00, 0x01,
				0x05, 0x1c, 0x70, 0x01, 0x03, 0x00, 0x00, 0x40,
				0x06, 0x51, 0x07, 0x00, 0x00, 0x41, 0x06, 0x50,
				0x07, 0x07, 0x0f, 0x04);*/
	
	hx8369a_spi_write_seq_static(dsi_panel, SETGIP,
									0x00, 0x03, 0x03,
									0x00, 0x01, 0x04, 0x28,
									0x70, 0x11, 0x13, 0x00,
									0x00, 0x40, 0x06, 0x51,
									0x07, 0x00, 0x00, 0x41,
									0x06, 0x50, 0x07, 0x07,
									0x0F, 0x04, 0x00);
}

static void hx8369a_dsi_set_power(struct hx8369a *dsi_panel)
{
	/*hx8369a_spi_write_seq_static(dsi_panel, SETPOWER, 0x01, 0x00, 0x34, 0x06, 0x00,
				0x0f, 0x0f, 0x2a, 0x32, 0x3f, 0x3f, 0x07, 0x3a,
				0x01, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6);*/
				
	hx8369a_spi_write_seq_static(dsi_panel, SETPOWER, 			
									0x85, 0x00, 0x25,
									0x03, 0x00, 0x10, 0x10,
									0x2A, 0x32, 0x3F, 0x3F,
									0x01, 0x22, 0x01, 0xE6,
									0xE6, 0xE6, 0xE6, 0xE6);
}

static void hx8369a_dsi_set_vcom_voltage(struct hx8369a *dsi_panel)
{
	//hx8369a_spi_write_seq_static(dsi_panel, SETVCOM, 0x56, 0x56);
	
	hx8369a_spi_write_seq_static(dsi_panel, SETVCOM, 0x3A, 0x3A);
}

static void hx8369a_dsi_set_panel(struct hx8369a *dsi_panel)
{
	//hx8369a_spi_write_seq_static(dsi_panel, SETPANEL, 0x00);
	
	hx8369a_spi_write_seq_static(dsi_panel, SETPANEL, 0x03);
}

static void hx8369a_dsi_set_gamma_curve(struct hx8369a *dsi_panel)
{
	/*hx8369a_spi_write_seq_static(dsi_panel, SETGAMMA, 0x00, 0x1d, 0x22, 0x38, 0x3d,
				0x3f, 0x2e, 0x4a, 0x06, 0x0d, 0x0f, 0x13, 0x15,
				0x13, 0x16, 0x10, 0x19, 0x00, 0x1d, 0x22, 0x38,
				0x3d, 0x3f, 0x2e, 0x4a, 0x06, 0x0d, 0x0f, 0x13,
				0x15, 0x13, 0x16, 0x10, 0x19);*/
				
	hx8369a_spi_write_seq_static(dsi_panel, SETGAMMA,			
									0x00, 0x13, 0x19,
									0x38, 0x3D, 0x3F, 0x28,
									0x46, 0x07, 0x0D, 0x0E,
									0x12, 0x15, 0x12, 0x14,
									0x0F, 0x17, 0x00, 0x13,
									0x19, 0x38, 0x3D, 0x3F,
									0x28, 0x46, 0x07, 0x0D,
									0x0E, 0x12, 0x15, 0x12,
									0x14, 0x0F, 0x17);
}

static void hx8369a_dsi_set_temp_control(struct hx8369a *dsi_panel)
{
	hx8369a_spi_write_seq_static(dsi_panel, SETTEMP,
									0x00, 0x12, 0x63,
									0xA7, 0x09, 0x67, 0x50,
									0x4E, 0x17, 0x75);
}

static void hx8369a_dsi_set_mipi(struct hx8369a *dsi_panel)
{
	/*u8 eleventh_p = dsi_panel->data_lanes == 2 ? 0x11 : 0x10;

	hx8369a_spi_write_seq(dsi_panel, SETMIPI, 0x00, 0xa0, 0xc6, 0x00, 0x0a, 0x00,
			0x10, 0x30, 0x6f, 0x02, eleventh_p, 0x18, 0x40);*/
			
	hx8369a_spi_write_seq(dsi_panel, SETMIPI,		
							0x00, 0xA0, 0xC6,
							0x00, 0x0A, 0x00, 0x10,
							0x30, 0x6F, 0x02, 0x11,
							0x18, 0x40);
}

static void hx8369a_dsi_set_interface_pixel_fomat(struct hx8369a *dsi_panel)
{
	/*u8 bpp;

	if (dsi->format == MIPI_DSI_FMT_RGB888)
		bpp = 0x77;
	else if (dsi->format == MIPI_DSI_FMT_RGB565)
		bpp = 0x55;
	else
		bpp = 0x66;*/

	hx8369a_spi_write_seq(dsi_panel, SETPIXEL_FMT, dsi_panel->pixel_format);
}

/*static void hx8369a_dsi_write_display_brightness(struct hx8369a *dsi_panel)
{
	hx8369a_spi_write_seq_static(dsi_panel, WRDISBV, 0xff);
}*/

/*static void hx8369a_dsi_write_cabc(struct hx8369a *dsi_panel)
{
	hx8369a_spi_write_seq_static(dsi_panel, WRCABC, 0x01);
}*/

/*static void hx8369a_dsi_write_control_display(struct hx8369a *dsi_panel)
{
	hx8369a_spi_write_seq_static(dsi_panel, WRCTRLD, 0x24);
}*/

static void hx8369a_dsi_panel_init(struct hx8369a *dsi_panel)
{
	// Последовательность подачи команд (инициализации) взята из документа "TFT480800-16-E APPLICATION NOTE"
	
	hx8369a_dsi_set_extension_command(dsi_panel);
	hx8369a_dsi_set_power(dsi_panel);
	hx8369a_dsi_set_display_related_register(dsi_panel);
	hx8369a_dsi_set_mipi(dsi_panel);
	hx8369a_dsi_set_display_waveform_cycle(dsi_panel);
	hx8369a_dsi_set_vcom_voltage(dsi_panel);
	hx8369a_dsi_set_gip(dsi_panel);
	hx8369a_dsi_set_gamma_curve(dsi_panel);
	msleep(10);
	hx8369a_dsi_set_interface_pixel_fomat(dsi_panel);
	hx8369a_dsi_set_display_rgb(dsi_panel);
	hx8369a_dsi_set_temp_control(dsi_panel);
	
	hx8369a_dsi_set_panel(dsi_panel);		
	/*hx8369a_dsi_set_column_address(dsi_panel);
	hx8369a_dsi_set_page_address(dsi_panel);
	hx8369a_dsi_write_display_brightness(dsi_panel);
	hx8369a_dsi_write_cabc(dsi_panel);
	hx8369a_dsi_write_control_display(dsi_panel);*/
}

static int hx8369a_enter_standby(struct hx8369a *dsi_panel)
{
	int ret;

	ret = hx8369a_spi_write_byte(dsi_panel, SETDISPLAY_OFF);
	if (ret < 0)
		return ret;

	usleep_range(10000, 12000);

	ret = hx8369a_spi_write_byte(dsi_panel, ENTER_SLEEPMODE);
	if (ret < 0)
		return ret;

	
	 // The controller needs 120ms when entering in sleep mode before we can
	 // send the command to go off sleep mode
	 
	msleep(120);

	return 0;
}

static int hx8369a_exit_standby(struct hx8369a *dsi_panel)
{
	int ret;

	ret = hx8369a_spi_write_byte(dsi_panel, EXIT_SLEEPMODE);
	if (ret < 0) {
		dev_err(dsi_panel->dev, "Failed to exit sleep mode: %d!\n", ret);
		return ret;
	}
	
	 // The controller needs 120ms when exiting from sleep mode before we
	 // can send the command to enter in sleep mode
	 
	msleep(120);

	ret = hx8369a_spi_write_byte(dsi_panel, SETDISPLAY_ON);
	if (ret < 0) {
		dev_err(dsi_panel->dev, "Failed to set display on: %d!\n", ret);
		return ret;
	}

	return 0;
}

static int hx8369a_lcd_get_sleepmode(struct lcd_device *lcdev)
{
	u8 mem[4];
	int ret;
	struct hx8369a *dp = (struct hx8369a *)lcdev->priv;
		
	gpio_set_value(dp->vm_gpio, 1); // spi
	msleep(5);
	
	hx8369a_spi_read_bytes(dp, GET_POWER_MODE, mem, 4);	
	ret = mem[2];
	
	gpio_set_value(dp->vm_gpio, 0); // video
	return ret;
}

static int hx8369a_lcd_set_sleepmode(struct lcd_device *lcdev, int mode)
{
	struct hx8369a *dp = (struct hx8369a *)lcdev->priv;
		
	if (mode == 1) {
		gpio_set_value(dp->vm_gpio, 1); // spi
		msleep(5);
		hx8369a_enter_standby(dp);
	} else if (mode == 0) {
		gpio_set_value(dp->vm_gpio, 1); // spi
		msleep(5);
		hx8369a_exit_standby(dp);
		msleep(5);		
		gpio_set_value(dp->vm_gpio, 0); // video
	} else 
		dev_err(&lcdev->dev, "Invalid value: %i\n", mode);
		
	return 0;
}

static struct lcd_ops hx8369a_lcd_ops = {
         .get_sleepmode      = hx8369a_lcd_get_sleepmode,
         .set_sleepmode      = hx8369a_lcd_set_sleepmode,
};

static void hx8369a_reset(struct hx8369a *dsi_panel)
{	
	msleep(120);
	
	gpio_set_value(dsi_panel->reset, 1);
	msleep(50);
	gpio_set_value(dsi_panel->reset, 0);
	msleep(50);
	gpio_set_value(dsi_panel->reset, 1);

	// The controller needs 120ms to recover from reset 
	msleep(120);
}

static int hx8369a_dsi_set_sequence(struct hx8369a *dsi_panel)
{		
	hx8369a_dsi_panel_init(dsi_panel);
	hx8369a_exit_standby(dsi_panel);
	
	msleep(10);
		
	return 0;
}

static int hx8369a_parse_dt(struct hx8369a *dsi_panel)
{
	struct device *dev = dsi_panel->dev;
	struct device_node *np = dev->of_node;
	int ret;
	
	ret = of_property_read_u32(np, "data-lanes", &dsi_panel->data_lanes);
	if (ret || dsi_panel->data_lanes == 0 || dsi_panel->data_lanes > 2) {
		dev_err(dev, "Cannot get correct data lanes!\n");
		return -EINVAL;
	}
	
	dsi_panel->res_sel = HX8369A_RES_480_800;
	/*dsi_panel->pixel_format = 0x66; // 18 bpp, MIPI_DSI_FMT_RGB666)*/
	dsi_panel->pixel_format = 0x77; // 24 bpp, MIPI_DSI_FMT_RGB888)
	
	of_property_read_u32(np, "power-on-delay", &dsi_panel->power_on_delay);
	of_property_read_u32(np, "reset-delay", &dsi_panel->reset_delay);
	
	dsi_panel->reset = of_get_named_gpio(np, "reset-gpio", 0);
	if (!gpio_is_valid(dsi_panel->reset)) {
		dev_err(dev, "Missing dt property: reset-gpio!\n");
		return -EINVAL;
	}

	ret = gpio_request(dsi_panel->reset, "hx8369a-reset");
	if (ret) {
		dev_err(dev, "GPIO request failed!\n");
		return -EINVAL;
	}
	gpio_direction_output(dsi_panel->reset, 1);
	
	dsi_panel->vm_gpio = of_get_named_gpio(np, "vm-gpio", 0);
	if (!gpio_is_valid(dsi_panel->vm_gpio)) {
		dev_err(dev, "Missing dt property: vm_gpio!\n");
		return -EINVAL;
	}

	ret = gpio_request(dsi_panel->vm_gpio, "hx8369a-vm");
	if (ret) {
		dev_err(dev, "GPIO request failed!\n");
		return -EINVAL;
	}
	gpio_direction_output(dsi_panel->vm_gpio, 1);
	
	return 0;
}

static int hx8369a_dsi_spi_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct hx8369a *dsi_panel;
	struct lcd_device *lcdev;
	int ret=0;
	
	dsi_panel = devm_kzalloc(dev, sizeof(struct hx8369a), GFP_KERNEL);
	if (!dsi_panel)
		return -ENOMEM;
		
	ret = spi_setup(spi);
	if (ret < 0) {
		dev_err(&spi->dev, "SPI setup failed!\n");
		return ret;
	}

	dsi_panel->dev = dev;	
	dsi_panel->spi = spi;
			
	ret = hx8369a_parse_dt(dsi_panel);
	if (ret)
		return ret;	
	
	gpio_set_value(dsi_panel->vm_gpio, 1);
	
	hx8369a_reset(dsi_panel);
	
	ret = hx8369a_dsi_set_sequence(dsi_panel);	
	if (ret)
		return ret;
		
	gpio_set_value(dsi_panel->vm_gpio, 0);
	
	lcdev = devm_lcd_device_register(&spi->dev, "himax-panel", &spi->dev, dsi_panel, &hx8369a_lcd_ops);
	if (IS_ERR(lcdev)) {
		ret = PTR_ERR(lcdev);
		return ret;
	}
	
	lcdev->priv = dsi_panel;
	spi_set_drvdata(spi, lcdev);
	
	//dev_info(&spi->dev, "%s() completed successfully\n", __func__);
	dev_info(&spi->dev, "HX8369A LCD driver loaded successfully\n");
			  			
	return ret;
}

static int hx8369a_dsi_spi_remove(struct spi_device *spi)
{
	return 0;
}

static const struct of_device_id hx8369a_of_match[] = {
	{ .compatible = "himax,panel-hx8369a-dsi-spi" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, hx8369a_of_match);

static struct spi_driver hx8369a_dsi_spi_driver = {
	.probe = hx8369a_dsi_spi_probe,
	.remove = hx8369a_dsi_spi_remove,
	.driver = {
		.name = "panel-hx8369a",
		.of_match_table = hx8369a_of_match,
	},
};

static int __init hx8369a_init(void)
{
	int err;

	err = spi_register_driver(&hx8369a_dsi_spi_driver);
	if (err < 0)
		return err;

	return 0;
}
module_init(hx8369a_init);

static void __exit hx8369a_exit(void)
{
	spi_unregister_driver(&hx8369a_dsi_spi_driver);
}
module_exit(hx8369a_exit);

MODULE_AUTHOR("Michael Sadikov");
MODULE_DESCRIPTION("Himax HX8369A panel driver");
MODULE_LICENSE("GPL v2");
