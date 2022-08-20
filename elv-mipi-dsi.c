/* linux/drivers/video/fbdev/vpoutfb/elv_mipi_dsi.c
 *
 * Elvees MIPI-DSI Controller driver.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/pm.h>

#include <video/elv_mipi_dsi.h>
#include "elv-mipi-dsi.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

/*#ifdef	CONFIG_PM
static int dsi_autosuspend_delay = 15;		// Default delay value, in seconds 
module_param_named(autosuspend, dsi_autosuspend_delay, int, 0644);
MODULE_PARM_DESC(autosuspend, "default dsi autosuspend delay");

#else
#define dsi_autosuspend_delay		0
#endif*/

static inline void dsi_write(struct elv_mipi_dsi *dsi, u32 reg, u32 val)
{
	iowrite32(val, dsi->reg_base + reg);
}

static inline u32 dsi_read(struct elv_mipi_dsi *dsi, u32 reg)
{
	return ioread32(dsi->reg_base + reg);
}

static inline void dsi_modify(struct elv_mipi_dsi *dsi, u32 reg,
			      u32 mask, u32 val)
{
	u32 v;

	v = ioread32(dsi->reg_base + reg);
	v &= ~mask;
	v |= val;
	iowrite32(v, dsi->reg_base + reg);
}

static int dsi_max(int x, int y) {
  return (x) >= (y) ? (x) : (y);
}


#ifdef CONFIG_DEBUG_FS
#define DSI_REGS_BUFSIZE	2048
static ssize_t dsi_show_regs(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct elv_mipi_dsi *dsi = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(DSI_REGS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"%s registers:\n", dev_name(dsi->dev));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_DEVICE_READY_REG: \t\t\t0x%02x 0x%08x\n", DSI_DEVICE_READY_REG, dsi_read(dsi, DSI_DEVICE_READY_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_IRQ_STATUS_REG: \t\t\t0x%02x 0x%08x\n", DSI_IRQ_STATUS_REG, dsi_read(dsi, DSI_IRQ_STATUS_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_IRQ_ENABLE_REG: \t\t\t0x%02x 0x%08x\n", DSI_IRQ_ENABLE_REG, dsi_read(dsi, DSI_IRQ_ENABLE_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_FUNC_PRG_REG: \t\t\t0x%02x 0x%08x\n", DSI_FUNC_PRG_REG, dsi_read(dsi, DSI_FUNC_PRG_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_HS_TX_TIMEOUT_REG: \t\t\t0x%02x 0x%08x\n", DSI_HS_TX_TIMEOUT_REG, dsi_read(dsi, DSI_HS_TX_TIMEOUT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_LP_RX_TIMEOUT_REG: \t\t\t0x%02x 0x%08x\n", DSI_LP_RX_TIMEOUT_REG, dsi_read(dsi, DSI_LP_RX_TIMEOUT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,		
			"DSI_TURN_AROUND_TIMEOUT_REG: \t\t0x%02x 0x%08x\n", DSI_TURN_AROUND_TIMEOUT_REG, dsi_read(dsi, DSI_TURN_AROUND_TIMEOUT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_DPI_RESOLUTION_REG: \t\t0x%02x 0x%08x\n", DSI_DPI_RESOLUTION_REG, dsi_read(dsi, DSI_DPI_RESOLUTION_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_HSYNC_COUNT_REG: \t\t\t0x%02x 0x%08x\n", DSI_HSYNC_COUNT_REG, dsi_read(dsi, DSI_HSYNC_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_HORIZ_BACK_PORCH_COUNT_REG: \t0x%02x 0x%08x\n", DSI_HORIZ_BACK_PORCH_COUNT_REG, dsi_read(dsi, DSI_HORIZ_BACK_PORCH_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_HORIZ_FRONT_PORCH_COUNT_REG: \t0x%02x 0x%08x\n", DSI_HORIZ_FRONT_PORCH_COUNT_REG, dsi_read(dsi, DSI_HORIZ_FRONT_PORCH_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_HORIZ_ACTIVE_AREA_COUNT_REG: \t0x%02x 0x%08x\n", DSI_HORIZ_ACTIVE_AREA_COUNT_REG, dsi_read(dsi, DSI_HORIZ_ACTIVE_AREA_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_VSYNC_COUNT_REG: \t\t\t0x%02x 0x%08x\n", DSI_VSYNC_COUNT_REG, dsi_read(dsi, DSI_VSYNC_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_VERT_BACK_PORCH_COUNT_REG: \t\t0x%02x 0x%08x\n", DSI_VERT_BACK_PORCH_COUNT_REG, dsi_read(dsi, DSI_VERT_BACK_PORCH_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_VERT_FRONT_PORCH_COUNT_REG: \t0x%02x 0x%08x\n", DSI_VERT_FRONT_PORCH_COUNT_REG, dsi_read(dsi, DSI_VERT_FRONT_PORCH_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_HIGH_LOW_SWITCH_COUNT_REG: \t\t0x%02x 0x%08x\n", DSI_HIGH_LOW_SWITCH_COUNT_REG, dsi_read(dsi, DSI_HIGH_LOW_SWITCH_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,	
			"DSI_DPI_CONTROL_REG: \t\t\t0x%02x 0x%08x\n", DSI_DPI_CONTROL_REG, dsi_read(dsi, DSI_DPI_CONTROL_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_PLL_LOCK_COUNT_REG: \t\t0x%02x 0x%08x\n", DSI_PLL_LOCK_COUNT_REG, dsi_read(dsi, DSI_PLL_LOCK_COUNT_REG));			
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,	
			"DSI_INIT_COUNT_REG: \t\t\t0x%02x 0x%08x\n", DSI_INIT_COUNT_REG, dsi_read(dsi, DSI_INIT_COUNT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_MAX_RETURN_PACKET_REG: \t\t0x%02x 0x%08x\n", DSI_MAX_RETURN_PACKET_REG, dsi_read(dsi, DSI_MAX_RETURN_PACKET_REG));	
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,	
			"DSI_VIDEO_MODE_FORMAT_REG: \t\t0x%02x 0x%08x\n", DSI_VIDEO_MODE_FORMAT_REG, dsi_read(dsi, DSI_VIDEO_MODE_FORMAT_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_CLK_EOT_REG: \t\t\t0x%02x 0x%08x\n", DSI_CLK_EOT_REG, dsi_read(dsi, DSI_CLK_EOT_REG));				
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,	
			"DSI_POLARITY_REG: \t\t\t0x%02x 0x%08x\n", DSI_POLARITY_REG, dsi_read(dsi, DSI_POLARITY_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_CLK_LANE_SWT_REG: \t\t\t0x%02x 0x%08x\n", DSI_CLK_LANE_SWT_REG, dsi_read(dsi, DSI_CLK_LANE_SWT_REG));			
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,	
			"DSI_LP_BYTECLK_REG: \t\t\t0x%02x 0x%08x\n", DSI_LP_BYTECLK_REG, dsi_read(dsi, DSI_LP_BYTECLK_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_DPHY_PARAM_REG: \t\t\t0x%02x 0x%08x\n", DSI_DPHY_PARAM_REG, dsi_read(dsi, DSI_DPHY_PARAM_REG));			
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,	
			"DSI_CLK_LANE_TIMING_PARAM_REG: \t\t0x%02x 0x%08x\n", DSI_CLK_LANE_TIMING_PARAM_REG, dsi_read(dsi, DSI_CLK_LANE_TIMING_PARAM_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_RST_ENABLE_DFE_REG: \t\t0x%02x 0x%08x\n", DSI_RST_ENABLE_DFE_REG, dsi_read(dsi, DSI_RST_ENABLE_DFE_REG));		
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_TRIM0_REG: \t\t\t\t0x%02x 0x%08x\n", DSI_TRIM0_REG, dsi_read(dsi, DSI_TRIM0_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_TRIM1_REG: \t\t\t\t0x%02x 0x%08x\n", DSI_TRIM1_REG, dsi_read(dsi, DSI_TRIM1_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_TRIM2_REG: \t\t\t\t0x%02x 0x%08x\n", DSI_TRIM2_REG, dsi_read(dsi, DSI_TRIM2_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_TRIM3_REG: \t\t\t\t0x%02x 0x%08x\n", DSI_TRIM3_REG, dsi_read(dsi, DSI_TRIM3_REG));	
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_AUTO_ERR_REC_REG: \t\t\t0x%02x 0x%08x\n", DSI_AUTO_ERR_REC_REG, dsi_read(dsi, DSI_AUTO_ERR_REC_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_DIR_DPI_DIFF_REG: \t\t\t0x%02x 0x%08x\n", DSI_DIR_DPI_DIFF_REG, dsi_read(dsi, DSI_DIR_DPI_DIFF_REG));
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"DSI_DATA_LANE_POLARITY_SWAP_REG: \t0x%02x 0x%08x\n", DSI_DATA_LANE_POLARITY_SWAP_REG, dsi_read(dsi, DSI_DATA_LANE_POLARITY_SWAP_REG));					
	len += snprintf(buf + len, DSI_REGS_BUFSIZE - len,
			"=================================\n");

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations dsi_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= dsi_show_regs,
	.llseek		= default_llseek,
};

static int dsi_debugfs_init(struct elv_mipi_dsi *dsi)
{
	dsi->debugfs = debugfs_create_dir("mipi_dsi", NULL);	
	if (!dsi->debugfs)
		return -ENOMEM;

	debugfs_create_file("registers", S_IFREG | S_IRUGO,
		dsi->debugfs, (void *)dsi, &dsi_regs_ops);
	return 0;
}

static void dsi_debugfs_remove(struct elv_mipi_dsi *dsi)
{
	debugfs_remove_recursive(dsi->debugfs);
}

#else
static inline int dsi_debugfs_init(struct elv_mipi_dsi *dsi)
{
	return 0;
}

static inline void dsi_debugfs_remove(struct elv_mipi_dsi *dsi)
{
}
#endif /* CONFIG_DEBUG_FS */

static void elv_mipi_dsi_turn_on(struct elv_mipi_dsi *dsi)
{
	dsi_write(dsi, DSI_DEVICE_READY_REG, DEVICE_ENABLE);	
	dsi->ulp_mode = 0;
}

static void elv_mipi_dsi_normal_mode(struct elv_mipi_dsi *dsi)
{
	/*dsi_write(dsi, DSI_DEVICE_READY_REG, (DEVICE_EXIT_MODE | DEVICE_ENABLE));
	msleep(2);
	dsi_write(dsi, DSI_DEVICE_READY_REG, (DEVICE_NORMAL_MODE | DEVICE_ENABLE));
	dsi_write(dsi, DSI_DPI_CONTROL_REG, TURN_ON_PERIPHERAL);*/
	
	dsi_write(dsi, DSI_RST_ENABLE_DFE_REG, DFE_RST_ENABLE);
	dsi_write(dsi, DSI_DEVICE_READY_REG, DEVICE_ENABLE);
	dsi_write(dsi, DSI_DPI_CONTROL_REG, TURN_ON_PERIPHERAL);
	
	dsi->ulp_mode = 0;
}

static void elv_mipi_dsi_ulp_mode(struct elv_mipi_dsi *dsi)
{
	/*dsi_write(dsi, DSI_DPI_CONTROL_REG, TURN_OFF_PERIPHERAL);
	dsi_write(dsi, DSI_DEVICE_READY_REG, (DEVICE_ULP_MODE | DEVICE_ENABLE));*/
	
	dsi_write(dsi, DSI_DPI_CONTROL_REG, TURN_OFF_PERIPHERAL);
	dsi_write(dsi, DSI_DEVICE_READY_REG, 0);
	
	dsi->ulp_mode = 1;
}

static ssize_t elv_mipi_dsi_ulp_mode_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct elv_mipi_dsi *dsi = dev_get_drvdata(dev);
    int len;

    len = sprintf(buf, "%lu\n", dsi->ulp_mode);
    if (len <= 0)
        dev_err(dev, "Invalid sprintf len: %d\n", len);

    return len;
}

static ssize_t elv_mipi_dsi_ulp_mode_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct elv_mipi_dsi *dsi = dev_get_drvdata(dev);
    int ret;
    long val;

    ret = kstrtol(buf, 10, &val);
    if (ret)
		return ret;
	
	if (val == 0) 
		elv_mipi_dsi_normal_mode(dsi);
	else if (val == 1) 
		elv_mipi_dsi_ulp_mode(dsi);
	else {
		dev_err(dev, "Invalid value: %lu\n", val);
		return -ENXIO;
	}
			
    return count;
}

static DEVICE_ATTR(ulp_mode, S_IRUGO | S_IWUSR, elv_mipi_dsi_ulp_mode_show,
                   elv_mipi_dsi_ulp_mode_store);

static void elv_mipi_dsi_config_dsi(struct elv_mipi_dsi *dsi)
{	
	struct mipi_dsi_config *dsi_config = &dsi->dsi_config;
	
	dsi_config->data_lanes = DSI_DataLanes_2;
	/*dsi_config->video_format = DSI_video_format_RGB666;*/	
	dsi_config->video_format = DSI_video_format_RGB888;
	dsi_config->ch_video_mode = DSI_virt_ch_0;
	
	/*dsi_write(dsi, DSI_FUNC_PRG_REG, (DATA_LANES_2 | RGB666 | 
		VM_CHAN_NO_0));	*/
	dsi_write(dsi, DSI_FUNC_PRG_REG, (DATA_LANES_2 | RGB888 | 
		VM_CHAN_NO_0));
		
	//dsi_config->video_mode = DSI_vd_mode_burst;	
	dsi_config->video_mode = DSI_vd_mode_non_burst_sync_pulse;
		
	//dsi_write(dsi, DSI_VIDEO_MODE_FORMAT_REG, BURST_MODE);
	dsi_write(dsi, DSI_VIDEO_MODE_FORMAT_REG, NON_BURST_WITH_SYNC_PULSES); 
	dsi_write(dsi, DSI_CLK_EOT_REG, DISABLE_VIDEO_BTA);
	//dsi_write(dsi, DSI_CLK_EOT_REG, ENABLE_VIDEO_BTA);
	dsi_write(dsi, DSI_AUTO_ERR_REC_REG, ECC_MUL_ERR_CLR);
}

static void elv_mipi_dsi_set_base_timings(struct elv_mipi_dsi *dsi)
{
	dsi_write(dsi, DSI_HS_TX_TIMEOUT_REG, 0xFFFFFF);
	dsi_write(dsi, DSI_LP_RX_TIMEOUT_REG, 0xFFFFFF);
	dsi_write(dsi, DSI_TURN_AROUND_TIMEOUT_REG, 0x1F);
	dsi_write(dsi, DSI_DEVICE_RESET_REG, 0xFF);
	dsi_write(dsi, DSI_INIT_COUNT_REG, 0x7D0);
}

static void elv_mipi_dsi_ddr_clk_calc(struct elv_mipi_dsi *dsi)
{	
	struct mipi_dsi_config *dsi_config = &dsi->dsi_config;	
	int ddr_clk_freq, ddr_clk_freq_rez;	
	int pixel_format = 0, video_mode_format = 0, lane_count = 1;
	//float pclk_freq, t_pclk;
	u32 pclk_freq, t_pclk;
	
	//pclk_freq = (float)AXI_CLK_MHZ/(PCLK_DIV + 1);
	pclk_freq = AXI_CLK_MHZ/(PCLK_DIV + 1);
	//pclk_freq = 24; // hlen * vlen * 60 fps
	dsi_config->pclk_freq = pclk_freq;
  
	//t_pclk = (float)1000/pclk_freq;
	t_pclk = 1000/pclk_freq;
	dsi_config->t_pclk = t_pclk;
	
	switch (dsi_config->video_format) {
		case DSI_video_format_RGB565:
			pixel_format = 16;
			break;
		case DSI_video_format_RGB666:
			pixel_format = 18;
			break;
		case DSI_video_format_RGB666_lp:
		case DSI_video_format_RGB888:
			pixel_format = 24;		
			break;
		default:
			break;
  }
  
  if (dsi_config->video_mode == DSI_vd_mode_burst)
    video_mode_format = 2;
  else
    video_mode_format = 1;
  lane_count = dsi_config->data_lanes;

  ddr_clk_freq = (pclk_freq * pixel_format * video_mode_format) / (2*lane_count);  
  
  if ((ddr_clk_freq % 12) != 0) {
//     if ((ddr_clk_freq % 12) > 5)
      ddr_clk_freq_rez = (ddr_clk_freq/12+1)*12;
//     else
//       ddr_clk_freq_rez = (ddr_clk_freq/12)*12;
  } else 
      ddr_clk_freq_rez = ddr_clk_freq;
  
  if (ddr_clk_freq_rez > DDR_CLK_FREQ_MAX) {
    ddr_clk_freq_rez = DDR_CLK_FREQ_MAX;
    dev_err(dsi->dev, "Error ddr_clk_freq value!\n");
  }
  
  //dsi_config->t_byteclk = (float)4000/ddr_clk_freq;
  dsi_config->t_byteclk = 4000/ddr_clk_freq;
  dsi_config->ddr_freq = ddr_clk_freq_rez;
  /*dev_info(dsi->dev, "ddr_clk_freq = %i\n", dsi_config->ddr_freq);
  dev_info(dsi->dev, "pclk_freq = %i\n", dsi_config->pclk_freq);
  dev_info(dsi->dev, "t_pclk = %i\n", dsi_config->t_pclk);
  dev_info(dsi->dev, "t_byteclk = %i\n", dsi_config->t_byteclk);*/
}

static void elv_mipi_dsi_set_dpi_resolution(struct elv_mipi_dsi *dsi)
{
	/* Все значения записываются уменьшенные на единицу, измеряются в количестве тактов частоты PIXCLK
	* hsw - это HSYNC pulse width  
	* hgdel_hbp - это Horizontal back porch, (HBP)
	* hgate_haa - это количество активных пикселей в строке - Horizontal active area (HDISP)
	* hlen - это var->xres + var->left_margin + var->right_margin + var->hsync_len - 1
	* vsw - это VSYNC pulse width
	* vgdel_vbp - это Vertical back porch (VBP)
	* vgate_vaa - это количество активных строк в кадре - Vertical active area (VDISP)
	* vlen - это var->yres + var->upper_margin + var->lower_margin + var->vsync_len - 1
	*/
	
	struct mipi_dsi_config *dsi_config = &dsi->dsi_config;
	short int hsw = 11, hgdel_hbp = 11, hgate_haa = 479, hlen = 515; //(480+12+12+12-1)  
	/*short int hsw = 14, hgdel_hbp = 14, hgate_haa = 479, hlen = 539; //(480+15+15+30-1)*/
	/*short int vsw = 3, vgdel_vbp = 3, vgate_vaa = 799, vlen = 811; //(800+4+4+4-1)*/
	short int vsw = 5, vgdel_vbp = 5, vgate_vaa = 799, vlen = 817; 	//(800+6+6+6-1) Значение (6 = 5-1) должно совпадать со значением 
																	// из команды SETDISP (amount of scan line) см. panel-hx8369a-spi.c
	
	dsi_config->DPI_resolution_h = hgate_haa + 1;
	dsi_config->DPI_resolution_v = vgate_vaa + 1;
	dsi_config->HSYNC_count = hsw + 1;
	dsi_config->HSYNC_bpc = hgdel_hbp + 1;
	dsi_config->HSYNC_fpc = (hlen + 1) - (hgate_haa + 1) - (hgdel_hbp + 1) - (hsw + 1);
	dsi_config->HSYNC_aac = hgate_haa + 1;
	dsi_config->VSYNC_count = vsw + 1;
	dsi_config->VSYNC_bpc = vgdel_vbp + 1;
	dsi_config->VSYNC_fpc = (vlen + 1) - (vgate_vaa + 1) - (vgdel_vbp + 1) - (vsw + 1);
	
	dsi_write(dsi, DSI_DPI_RESOLUTION_REG, 
		(dsi_config->DPI_resolution_h | 
		(dsi_config->DPI_resolution_v << 16)));
	
	/* Далее в регистры мы пишем константы вместо рассчитанных значений, т.к. расчет является 
	   неточным из-за отсутствия нормальной поддержки float в ядре */
	
	// По расчету у нас получилось 16 вместо 15
	dsi_write(dsi, DSI_HSYNC_COUNT_REG, 15);
	//dsi_write(dsi, DSI_HSYNC_COUNT_REG, DIV_ROUND_UP(dsi_config->HSYNC_count*dsi_config->t_pclk, dsi_config->t_byteclk));
	//dsi_write(dsi, DSI_HSYNC_COUNT_REG, (u32)dsi_config->HSYNC_count*dsi_config->t_pclk/dsi_config->t_byteclk +1);
	// DSICONTROLLER->HSYNC_COUNT_REG = ceil((float)l_DSI_dpi_resolution->HSYNC_count*t_pclk/t_byteclk);
	
	// У нас получилось 16 вместо 15
	dsi_write(dsi, DSI_HORIZ_BACK_PORCH_COUNT_REG, 15);
	//dsi_write(dsi, DSI_HORIZ_BACK_PORCH_COUNT_REG, dsi_max(DIV_ROUND_UP(dsi_config->HSYNC_bpc*dsi_config->t_pclk, dsi_config->t_byteclk), HSYNC_bpc_min)); 
	//dsi_write(dsi, DSI_HORIZ_BACK_PORCH_COUNT_REG, dsi_max((u32)dsi_config->HSYNC_bpc*dsi_config->t_pclk/dsi_config->t_byteclk +1, HSYNC_bpc_min)); 
	// DSICONTROLLER->HORIZ_BACK_PORCH_COUNT_REG = dsi_max(ceil((float)l_DSI_dpi_resolution->HSYNC_bpc*t_pclk/t_byteclk), HSYNC_bpc_min);
	
	// У нас получилось 16 вместо 15
	dsi_write(dsi, DSI_HORIZ_FRONT_PORCH_COUNT_REG, 15);
	//dsi_write(dsi, DSI_HORIZ_FRONT_PORCH_COUNT_REG, dsi_max(DIV_ROUND_UP(dsi_config->HSYNC_fpc*dsi_config->t_pclk, dsi_config->t_byteclk), HSYNC_fpc_min));
	//dsi_write(dsi, DSI_HORIZ_FRONT_PORCH_COUNT_REG, dsi_max((u32)dsi_config->HSYNC_fpc*dsi_config->t_pclk/dsi_config->t_byteclk +1, HSYNC_fpc_min)); 
	// DSICONTROLLER->HORIZ_FRONT_PORCH_COUNT_REG = dsi_max(ceil((float)l_DSI_dpi_resolution->HSYNC_fpc*t_pclk/t_byteclk), HSYNC_fpc_min);
  
	// У нас получилось 729 вместо 720
	dsi_write(dsi, DSI_HORIZ_ACTIVE_AREA_COUNT_REG, 720);
	//dsi_write(dsi, DSI_HORIZ_ACTIVE_AREA_COUNT_REG, DIV_ROUND_UP(dsi_config->HSYNC_aac*dsi_config->t_pclk, dsi_config->t_byteclk));
	//dsi_write(dsi, DSI_HORIZ_ACTIVE_AREA_COUNT_REG, (u32)dsi_config->HSYNC_aac*dsi_config->t_pclk/dsi_config->t_byteclk +1);
	// DSICONTROLLER->HORIZ_ACTIVE_AREA_COUNT_REG = ceil((float)l_DSI_dpi_resolution->HSYNC_aac*t_pclk/t_byteclk);
	
	dsi_write(dsi, DSI_VSYNC_COUNT_REG, dsi_config->VSYNC_count);  
	// DSICONTROLLER->VSYNC_COUNT_REG = l_DSI_dpi_resolution->VSYNC_count;
	
	dsi_write(dsi, DSI_VERT_BACK_PORCH_COUNT_REG, dsi_max(dsi_config->VSYNC_bpc, VSYNC_bpc_min));
	// DSICONTROLLER->VERT_BACK_PORCH_COUNT_REG = dsi_max(l_DSI_dpi_resolution->VSYNC_bpc, VSYNC_bpc_min);
	
	dsi_write(dsi, DSI_VERT_FRONT_PORCH_COUNT_REG, dsi_max(dsi_config->VSYNC_fpc, VSYNC_fpc_min));
	// DSICONTROLLER->VERT_FRONT_PORCH_COUNT_REG = dsi_max(l_DSI_dpi_resolution->VSYNC_fpc, VSYNC_fpc_min);
}

static void elv_mipi_dsi_set_dphy_timings(struct elv_mipi_dsi *dsi)
{
	struct mipi_dsi_config *dsi_config = &dsi->dsi_config;
	//   Note: UI is any DDR half cycle.
	//   UI = 500/ddr_clk_freq;
	int HS_PREP_tbl, HS_ZERO_tbl, HS_TRAIL_tbl, HS_EXIT_tbl = 115;
	int n = 1;	// Where n = 1 for Forward-direction HS mode and n = 4 for Reverse-direction HS mode
	int dln_cnt_hs_prep, dln_cnt_hs_zero, dln_cnt_hs_trail, dln_cnt_hs_exit,
    cln_cnt_prep, cln_cnt_zero, cln_cnt_hs_trail, cln_cnt_hs_exit,
    lp_byteclk, high_ls_count, hs_to_lp_switch_count, lp_to_hs_switch_count;
    int CLK_PREP_tbl = 60, CLK_ZERO_tbl, CLK_TRAIL_tbl = 60, CLK_EXIT_tbl = 60;
    
    /* Константы 115, 60, 170, 30 и т.п. взяты из примера "MIPI DSI test" от Элвиса, в элвисовский пример
     * они попали из документа "MIPI Alliance Specification for D-PHY Version 1.00.00 - 14 May 2009", из
     * переписки с Элвисом: "Конкретнее - эти тайминги взяты из таблицы 14 на стр. 53 (п. 5.9) указанного документа.
     * В нашем примере выбраны средние допустимые значения из этой таблицы, либо, если указана только нижняя граница, 
     * то значения с небольшим отступом от этой границы".
     */ 
  
	HS_PREP_tbl = 60 + DIV_ROUND_UP(4*500, dsi_config->ddr_freq);
	//HS_PREP_tbl = (int)(60 + 4*500/dsi_config->ddr_freq);
	// HS_PREP_tbl = ceil(60 + (float)4*500/ddr_clk_freq);
	
	HS_ZERO_tbl = 170 + DIV_ROUND_UP(10*500, dsi_config->ddr_freq) - HS_PREP_tbl;	
	//HS_ZERO_tbl = (int)(170 + 10*500/dsi_config->ddr_freq) - HS_PREP_tbl;	
	// HS_ZERO_tbl = ceil(170 + (float)10*500/ddr_clk_freq) - HS_PREP_tbl;
	
	HS_TRAIL_tbl = dsi_max( DIV_ROUND_UP(n*8*500, dsi_config->ddr_freq), (60 + DIV_ROUND_UP(n*4*500, dsi_config->ddr_freq)) ) + 30;
	//HS_TRAIL_tbl = dsi_max((int)(n*8*500/dsi_config->ddr_freq), (int)(60 + n*4*500/dsi_config->ddr_freq)) + 30;	
	// HS_TRAIL_tbl = dsi_max(ceil((float)n*8*500/ddr_clk_freq), ceil(60 + (float)n*4*500/ddr_clk_freq)) + 30;
  	
	// В обоих случаях получились отрицательные числа, которые приводятся к 0 ниже
	dln_cnt_hs_prep = DIV_ROUND_CLOSEST( abs( (HS_PREP_tbl - DIV_ROUND_CLOSEST(9*1000, dsi_config->ddr_freq ))), dsi_config->t_byteclk ) - 1;
	//dln_cnt_hs_prep = (int)(abs(HS_PREP_tbl - 9*1000/dsi_config->ddr_freq)/dsi_config->t_byteclk - 1);	
	// dln_cnt_hs_prep =	round((float)abs(HS_PREP_tbl - 9*1000/ddr_clk_freq)/t_byteclk - 1);
	
	dln_cnt_hs_zero = DIV_ROUND_UP(HS_ZERO_tbl, dsi_config->t_byteclk) - 1;
	//dln_cnt_hs_zero = (int)(HS_ZERO_tbl/dsi_config->t_byteclk - 1);
	// dln_cnt_hs_zero =	ceil((float)HS_ZERO_tbl/t_byteclk - 1);
	
	dln_cnt_hs_trail =	DIV_ROUND_UP(HS_TRAIL_tbl, dsi_config->t_byteclk) - 2;
	//dln_cnt_hs_trail =	(int)(HS_TRAIL_tbl/dsi_config->t_byteclk - 2);	
	// dln_cnt_hs_trail =	ceil((float)HS_TRAIL_tbl/t_byteclk - 2);
	
	dln_cnt_hs_exit =	DIV_ROUND_UP(HS_EXIT_tbl, dsi_config->t_byteclk) - 1;
	//dln_cnt_hs_exit =	(int)(HS_EXIT_tbl/dsi_config->t_byteclk - 1);
	//dln_cnt_hs_exit =	ceil((float)HS_EXIT_tbl/t_byteclk - 1);
	
	if (dln_cnt_hs_prep < 0)
		dln_cnt_hs_prep = 0;
	if (dln_cnt_hs_zero < 0)
		dln_cnt_hs_zero = 0;
	if (dln_cnt_hs_trail < 0)
		dln_cnt_hs_trail = 0;
	if (dln_cnt_hs_exit < 0)
		dln_cnt_hs_exit = 0;
  
	
	CLK_ZERO_tbl = 330 - CLK_PREP_tbl;
  
	cln_cnt_prep =	DIV_ROUND_UP(CLK_PREP_tbl, dsi_config->t_byteclk) - 1;
	//cln_cnt_prep =	(int)(CLK_PREP_tbl/dsi_config->t_byteclk - 1);
	//cln_cnt_prep =	ceil((float)CLK_PREP_tbl/t_byteclk - 1);
	
	cln_cnt_zero =	DIV_ROUND_UP(CLK_ZERO_tbl, dsi_config->t_byteclk) - 1;
	//cln_cnt_zero =	(int)(CLK_ZERO_tbl/dsi_config->t_byteclk - 1);
	//cln_cnt_zero =	ceil((float)CLK_ZERO_tbl/t_byteclk - 1);

	cln_cnt_hs_trail =	DIV_ROUND_UP((CLK_TRAIL_tbl - DIV_ROUND_CLOSEST(3*1000, dsi_config->ddr_freq)), dsi_config->t_byteclk) + 3;
	//cln_cnt_hs_trail =	(int)((CLK_TRAIL_tbl - 3*1000/dsi_config->ddr_freq)/dsi_config->t_byteclk) + 3;
	//cln_cnt_hs_trail =	ceil((float)(CLK_TRAIL_tbl - 3*1000/ddr_clk_freq)/t_byteclk) + 3;
	
	cln_cnt_hs_exit =	DIV_ROUND_UP((CLK_EXIT_tbl - DIV_ROUND_CLOSEST(2*1000, dsi_config->ddr_freq)), dsi_config->t_byteclk) - 1;
	//cln_cnt_hs_exit =	(int)((CLK_EXIT_tbl - 2*1000/dsi_config->ddr_freq)/dsi_config->t_byteclk - 1);	
	//cln_cnt_hs_exit =	ceil((float)(CLK_EXIT_tbl - 2*1000/ddr_clk_freq)/t_byteclk - 1);
	
	if (cln_cnt_prep < 0)
		cln_cnt_prep = 0;
	if (cln_cnt_zero < 0)
		cln_cnt_zero = 0;
	if (cln_cnt_hs_trail < 0)
		cln_cnt_hs_trail = 0;
	if (cln_cnt_hs_exit < 0)
		cln_cnt_hs_exit = 0;
  
  lp_byteclk = DIV_ROUND_UP(dsi_config->ddr_freq, (12*4));
  //lp_byteclk = (int)(dsi_config->ddr_freq/(12*4));
  //lp_byteclk = ceil((float)ddr_clk_freq/(12*4));
  
  /* Далее в регистры мы пишем константы вместо рассчитанных значений, т.к. расчет является 
	 неточным из-за отсутствия нормальной поддержки float в ядре */
  
  high_ls_count = 101;/*4*lp_byteclk + dln_cnt_hs_prep + dln_cnt_hs_zero + 4*dsi_config->t_byteclk;*/
  //high_ls_count = 4*lp_byteclk + dln_cnt_hs_prep + dln_cnt_hs_zero + 4*(int)t_byteclk;
  
  hs_to_lp_switch_count = 89;/*cln_cnt_hs_trail + cln_cnt_hs_exit + 3*dsi_config->t_byteclk;*/
  //hs_to_lp_switch_count = cln_cnt_hs_trail + cln_cnt_hs_exit + 3*(int)t_byteclk;
  
  lp_to_hs_switch_count = 162;/*4*lp_byteclk + cln_cnt_prep + cln_cnt_zero + DIV_ROUND_UP(8*500, dsi_config->ddr_freq) + 4*dsi_config->t_byteclk;*/
  //lp_to_hs_switch_count = 4*lp_byteclk + cln_cnt_prep + cln_cnt_zero + (int)((float)8*500/dsi_config->ddr_freq) + 4*(int)dsi_config->t_byteclk;
  //lp_to_hs_switch_count = 4*lp_byteclk + cln_cnt_prep + cln_cnt_zero + (int)ceil((float)8*500/ddr_clk_freq) + 4*(int)t_byteclk;
    
  dsi_write(dsi, DSI_DPHY_PARAM_REG, (dln_cnt_hs_prep | (dln_cnt_hs_zero << 8) | (dln_cnt_hs_trail << 16) | (dln_cnt_hs_exit << 24)));
  /*buf_reg = DSICONTROLLER->DPHY_PARAM_REG;
  buf_reg = SET_DSI_CONTROLLER_MODEL_DPHY_PARAM_REG_HS_PREP(buf_reg, dln_cnt_hs_prep);
  buf_reg = SET_DSI_CONTROLLER_MODEL_DPHY_PARAM_REG_HS_ZERO(buf_reg, dln_cnt_hs_zero);
  buf_reg = SET_DSI_CONTROLLER_MODEL_DPHY_PARAM_REG_HS_TRAIL(buf_reg, dln_cnt_hs_trail);
  buf_reg = SET_DSI_CONTROLLER_MODEL_DPHY_PARAM_REG_HS_EXIT(buf_reg, dln_cnt_hs_exit);
  DSICONTROLLER->DPHY_PARAM_REG = buf_reg;*/
  
  dsi_write(dsi, DSI_CLK_LANE_TIMING_PARAM_REG, (cln_cnt_prep | (cln_cnt_zero << 8) | (cln_cnt_hs_trail << 16) | (cln_cnt_hs_exit << 24)));
  /*buf_reg = DSICONTROLLER->CLK_LANE_TIMING_PARAM_REG;
  buf_reg = SET_DSI_CONTROLLER_MODEL_CLK_LANE_TIMING_PARAM_REG_HS_PREP(buf_reg, cln_cnt_prep);
  buf_reg = SET_DSI_CONTROLLER_MODEL_CLK_LANE_TIMING_PARAM_REG_HS_ZERO(buf_reg, cln_cnt_zero);
  buf_reg = SET_DSI_CONTROLLER_MODEL_CLK_LANE_TIMING_PARAM_REG_HS_TRAIL(buf_reg, cln_cnt_hs_trail);
  buf_reg = SET_DSI_CONTROLLER_MODEL_CLK_LANE_TIMING_PARAM_REG_HS_EXIT(buf_reg, cln_cnt_hs_exit);
  DSICONTROLLER->CLK_LANE_TIMING_PARAM_REG = buf_reg;*/
  
  dsi_write(dsi, DSI_LP_BYTECLK_REG, lp_byteclk);
  dsi_write(dsi, DSI_HIGH_LOW_SWITCH_COUNT_REG, high_ls_count);
  /*DSICONTROLLER->LP_BYTECLK_REG = SET_DSI_CONTROLLER_MODEL_LP_BYTECLK_REG_BYTECLK(DSICONTROLLER->LP_BYTECLK_REG, lp_byteclk);
  DSICONTROLLER->HIGH_LOW_SWITCH_COUNT_REG = SET_DSI_CONTROLLER_MODEL_HIGH_LOW_SWITCH_COUNT_REG_DATA_LANE_SWITCH_COUNT(DSICONTROLLER->HIGH_LOW_SWITCH_COUNT_REG, high_ls_count);*/
  
  dsi_write(dsi, DSI_CLK_LANE_SWT_REG, (hs_to_lp_switch_count | (lp_to_hs_switch_count << 16)));
  /*buf_reg = DSICONTROLLER->CLK_LANE_SWT_REG;
  buf_reg = SET_DSI_CONTROLLER_MODEL_CLK_LANE_SWT_REG_HS_TO_LP_SWITCHING_COUNTER(buf_reg, hs_to_lp_switch_count);
  buf_reg = SET_DSI_CONTROLLER_MODEL_CLK_LANE_SWT_REG_LP_TO_HS_SWITCHING_COUNTER(buf_reg, lp_to_hs_switch_count);
  DSICONTROLLER->CLK_LANE_SWT_REG = buf_reg;*/
}

static void elv_mipi_dsi_set_pll_div_ratio(struct elv_mipi_dsi *dsi)
{
	struct mipi_dsi_config *dsi_config = &dsi->dsi_config;
	unsigned int div_ratio, l_div_ratio, other_bits;
	
	div_ratio = dsi_config->ddr_freq/12;
	//dev_info(dsi->dev, "div_ratio = %i\n", div_ratio);
	
	if (div_ratio < div_ratio_min)
		l_div_ratio = div_ratio_min;
	else if (div_ratio > div_ratio_max)
		l_div_ratio = div_ratio_max;
	else
		l_div_ratio = div_ratio;	
		
	other_bits = 0u | BIT(11) | BIT(12) | BIT(13) | BIT(16) | BIT(18) | BIT(20) | BIT(21);
	dsi_write(dsi, DSI_TRIM1_REG, (l_div_ratio >> 1) | ((l_div_ratio & 0x01) << 6) | other_bits) ;
  	/*buf_reg = DSICONTROLLER->TRIM_REG1;
	buf_reg = SET_DSI_CONTROLLER_MODEL_TRIM_REG1_CNT_A(buf_reg, l_div_ratio >> 1);
	buf_reg = SET_DSI_CONTROLLER_MODEL_TRIM_REG1_CNT_B(buf_reg, l_div_ratio & 0x01);
	DSICONTROLLER->TRIM_REG1 = buf_reg;*/
}

static void elv_mipi_dsi_start_dsi(struct elv_mipi_dsi *dsi)
{
	dsi_write(dsi, DSI_RST_ENABLE_DFE_REG, DFE_RST_ENABLE);
	dsi_write(dsi, DSI_DEVICE_READY_REG, DEVICE_ENABLE);
	dsi_write(dsi, DSI_DPI_CONTROL_REG, TURN_ON_PERIPHERAL);  // Добавлено для корректной работы DSI контроллера в Linux после его использования в u-boot
}

static int elv_mipi_dsi_is_enable(struct elv_mipi_dsi *dsi)
{
	u32 ret;
	
	ret = dsi_read(dsi, DSI_DEVICE_READY_REG);
	return (ret & DEVICE_ENABLE);
}

static void elv_mipi_dsi_init_dsi(struct elv_mipi_dsi *dsi)
{	
	// Добавлено для корректной работы DSI контроллера в Linux после его использования в u-boot	
	if (elv_mipi_dsi_is_enable(dsi))
		elv_mipi_dsi_ulp_mode(dsi); 
	
	elv_mipi_dsi_config_dsi(dsi);
	elv_mipi_dsi_set_base_timings(dsi);
	elv_mipi_dsi_ddr_clk_calc(dsi);
	elv_mipi_dsi_set_dpi_resolution(dsi);
	elv_mipi_dsi_set_dphy_timings(dsi);
	elv_mipi_dsi_set_pll_div_ratio(dsi);
	elv_mipi_dsi_start_dsi(dsi);
	elv_mipi_dsi_turn_on(dsi);
}

static irqreturn_t dsi_irq_handler(int irq, void *dev_id)
{
	struct elv_mipi_dsi *dsi = (struct elv_mipi_dsi *)dev_id;
	u32 irqstatus;

	irqstatus = dsi_read(dsi, DSI_IRQ_STATUS_REG);

	/*if (irqstatus & INT_OUTFIFO)
		tasklet_schedule(&par->reset_tasklet);*/
	
	dsi_write(dsi, DSI_IRQ_STATUS_REG, irqstatus);

	return IRQ_HANDLED;
}

static struct attribute *elv_mipi_dsi_attrs[] = {
    &dev_attr_ulp_mode.attr,
    NULL
};

static struct attribute_group elv_mipi_dsi_attr_group = {
    .name = "elv_mipi_dsi",
    .attrs = elv_mipi_dsi_attrs,
};

#ifdef	CONFIG_PM

static int dsi_dev_suspend(struct device *dev)
{
	struct elv_mipi_dsi *dsi = (struct elv_mipi_dsi *)dev->driver_data;
		
	elv_mipi_dsi_ulp_mode(dsi);
	/*dev_info(dev, "DSI suspend ok!\n");*/
	return 0;
}

static int dsi_dev_resume(struct device *dev)
{
	struct elv_mipi_dsi *dsi = (struct elv_mipi_dsi *)dev->driver_data;
	
	elv_mipi_dsi_normal_mode(dsi);
	/*dev_info(dev, "DSI resume ok!\n");*/
	return 0;
}

static const struct dev_pm_ops dsi_device_pm_ops = {
	.suspend =	dsi_dev_suspend,
	.freeze =	dsi_dev_suspend,
	.resume =	dsi_dev_resume,
};

#endif

/*static UNIVERSAL_DEV_PM_OPS(dsi_device_pm_ops, dsi_dev_suspend,
                            dsi_dev_resume, NULL);*/

int elv_mipi_dsi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct elv_mipi_dsi *dsi;
	/*struct mipi_dsi_config *dsi_config;*/
	int ret = -EINVAL;
	/*unsigned long clk_rate;*/
	struct device_node *np;
	struct device_node *spi_node;
    struct spi_device *spi;

	//dev_info(&pdev->dev, "MIPI DSI probe...\n");	

	dsi = devm_kzalloc(&pdev->dev, sizeof(struct elv_mipi_dsi),
				GFP_KERNEL);
	if (!dsi) {
		dev_err(&pdev->dev, "Failed to allocate DSI object!\n");
		return -ENOMEM;
	}
			
	dsi->dev = &pdev->dev;	
	
	np = dsi->dev->of_node;
	
	spi_node = of_parse_phandle(np, "spi-control", 0);
    if (!spi_node) {
		dev_err(&pdev->dev, "Failed to get spi_node!\n");
        return -ENODEV;
    }
	
	spi = of_find_spi_device_by_node(spi_node);
    if (!spi) {
		//dev_err(&pdev->dev, "Failed to find_spi_device_by_node!\n");		
        return -EPROBE_DEFER;
    }
		
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	dsi->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dsi->reg_base)) {
		ret = PTR_ERR(dsi->reg_base);
		return ret;
	}
	
	dsi->irq = platform_get_irq(pdev, 0);
	if (IS_ERR_VALUE(dsi->irq)) {
		dev_err(&pdev->dev, "Failed to request DSI irq resource\n");
		return -EINVAL;
	}

	ret = devm_request_irq(&pdev->dev, dsi->irq, dsi_irq_handler,
			       0, "elvees-mipi-dsi", dsi);
	if (ret) {
		dev_err(&pdev->dev, "Cannot request irq handler\n");
		return ret;
	}
	
	dsi->dphy_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(dsi->dphy_clk)) {
		dev_err(&pdev->dev, "Failed to get D-PHY clock\n");
		return -ENOMEM;
	}	

	ret = clk_prepare_enable(dsi->dphy_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not prepare or enable D-PHY clock\n");
		clk_put(dsi->dphy_clk);
		goto error;
	}	
	/* else {
		clk_rate = clk_get_rate(dsi->dphy_clk);
		dev_info(&pdev->dev, "D-PHY clk enabled: %lu\n", clk_rate);
	}*/
		
	elv_mipi_dsi_init_dsi(dsi);
	
	platform_set_drvdata(pdev, dsi);
	
	dsi_debugfs_init(dsi);
	
	ret = sysfs_create_group(&pdev->dev.kobj, &elv_mipi_dsi_attr_group);
    if (ret) {
        dev_err(&pdev->dev, "sysfs creation elv_mipi_dsi failed\n");
        return ret;
    }

	//dev_info(&pdev->dev, "%s() completed successfully\n", __func__);
	dev_info(&pdev->dev, "MIPI DSI driver loaded successfully\n");

	return 0;

error:
	clk_disable(dsi->dphy_clk);
	return ret;
}

int elv_mipi_dsi_remove(struct platform_device *pdev)
{
	struct elv_mipi_dsi *dsi = platform_get_drvdata(pdev);
	
	clk_disable(dsi->dphy_clk);
	dsi_debugfs_remove(dsi);
	sysfs_remove_group(&pdev->dev.kobj, &elv_mipi_dsi_attr_group);
	return 0;
}

static const struct of_device_id elv_mipi_dsi_of_match[] = {
        { .compatible = "elvees,elv-mipi-dsi" },
        { /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, elv_mipi_dsi_of_match);

static struct platform_driver elv_mipi_dsi_driver = {
	.probe = elv_mipi_dsi_probe,
	.remove = elv_mipi_dsi_remove,
	.driver = {
		   .name = "mipi-dsi",
		   .of_match_table = of_match_ptr(elv_mipi_dsi_of_match),
#ifdef CONFIG_PM		   
		   .pm = &dsi_device_pm_ops,
#endif
	},
};

module_platform_driver(elv_mipi_dsi_driver);

MODULE_AUTHOR("Michael Sadikov");
MODULE_DESCRIPTION("Elvees MIPI-DSI controller driver");
MODULE_LICENSE("GPL");
