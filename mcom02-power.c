#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/sysfs.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#define PMCTR_SYS_PWR_DOWN_REG		0x004
#define PMCTR_SYS_PWR_STATUS_REG	0x00c
#define PMCTR_SYS_PWR_IMASK_REG		0x010
#define PMCTR_SYS_PWR_IRSTAT_REG	0x014	
#define PMCTR_SYS_PWR_ISTAT_REG		0x018
#define PMCTR_SYS_PWR_ICLR_REG		0x01c
#define PMCTR_SYS_PWR_DELAY_REG		0x020
#define PMCTR_DDR_PIN_RET_REG		0x024
#define PMCTR_DDR_INIT_END_REG		0x028
#define PMCTR_WARM_RST_EN_REG		0x02c
#define PMCTR_WKP_IMASK_REG			0x030
#define PMCTR_WKP_IRSTAT_REG		0x034
#define PMCTR_WKP_ISTAT_REG			0x038
#define PMCTR_WKP_ICLR_REG			0x03c
#define PMCTR_SW_RST_REG			0x040
#define PMCTR_WARM_RST_STATUS_REG	0x044
#define PMCTR_PDM_RST_STATUS_REG	0x048	
#define PMCTR_CORE_PWR_UP_REG		0x080
#define PMCTR_CORE_PWR_DOWN_REG		0x084
#define PMCTR_CORE_PWR_STATUS_REG	0x08c
#define PMCTR_CORE_PWR_IMASK_REG	0x090
#define PMCTR_CORE_PWR_IRSTAT_REG	0x094
#define PMCTR_CORE_PWR_ISTAT_REG	0x098
#define PMCTR_CORE_PWR_ICLR_REG		0x09c
#define PMCTR_CORE_PWR_DELAY_REG 	0x0a0

#define DSP_UP						(1 << 1)
#define VPU_UP						(1 << 2)
#define DSP_DOWN					(1 << 1)
#define VPU_DOWN					(1 << 2)

struct mcom_pmctr {
	struct device *dev;
	void __iomem *reg_base;
	int dsp_vpu_pwr_state;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif	

};

static inline void pmctr_write(struct mcom_pmctr *pmctr, u32 reg, u32 val)
{
	iowrite32(val, pmctr->reg_base + reg);
}

static inline u32 pmctr_read(struct mcom_pmctr *pmctr, u32 reg)
{
	return ioread32(pmctr->reg_base + reg);
}

#ifdef CONFIG_DEBUG_FS
#define PMCTR_REGS_BUFSIZE	2048
static ssize_t pmctr_show_regs(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct mcom_pmctr *pmctr = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(PMCTR_REGS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;

	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"%s registers:\n", dev_name(pmctr->dev));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"=================================\n");
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_SYS_PWR_STATUS_REG: \t0x%02x 0x%08x\n", PMCTR_SYS_PWR_STATUS_REG, pmctr_read(pmctr, PMCTR_SYS_PWR_STATUS_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_SYS_PWR_IMASK_REG: \t0x%02x 0x%08x\n", PMCTR_SYS_PWR_IMASK_REG, pmctr_read(pmctr, PMCTR_SYS_PWR_IMASK_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_SYS_PWR_IRSTAT_REG: \t0x%02x 0x%08x\n", PMCTR_SYS_PWR_IRSTAT_REG, pmctr_read(pmctr, PMCTR_SYS_PWR_IRSTAT_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_SYS_PWR_ISTAT_REG: \t0x%02x 0x%08x\n", PMCTR_SYS_PWR_ISTAT_REG, pmctr_read(pmctr, PMCTR_SYS_PWR_ISTAT_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,		
			"PMCTR_SYS_PWR_DELAY_REG: \t0x%02x 0x%08x\n", PMCTR_SYS_PWR_DELAY_REG, pmctr_read(pmctr, PMCTR_SYS_PWR_DELAY_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_DDR_PIN_RET_REG: \t\t0x%02x 0x%08x\n", PMCTR_DDR_PIN_RET_REG, pmctr_read(pmctr, PMCTR_DDR_PIN_RET_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_DDR_INIT_END_REG: \t0x%02x 0x%08x\n", PMCTR_DDR_INIT_END_REG, pmctr_read(pmctr, PMCTR_DDR_INIT_END_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_WARM_RST_EN_REG: \t\t0x%02x 0x%08x\n", PMCTR_WARM_RST_EN_REG, pmctr_read(pmctr, PMCTR_WARM_RST_EN_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_WKP_IMASK_REG: \t\t0x%02x 0x%08x\n", PMCTR_WKP_IMASK_REG, pmctr_read(pmctr, PMCTR_WKP_IMASK_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_WKP_IRSTAT_REG: \t\t0x%02x 0x%08x\n", PMCTR_WKP_IRSTAT_REG, pmctr_read(pmctr, PMCTR_WKP_IRSTAT_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_WKP_ISTAT_REG: \t\t0x%02x 0x%08x\n", PMCTR_WKP_ISTAT_REG, pmctr_read(pmctr, PMCTR_WKP_ISTAT_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_WKP_ICLR_REG: \t\t0x%02x 0x%08x\n", PMCTR_WKP_ICLR_REG, pmctr_read(pmctr, PMCTR_WKP_ICLR_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_SW_RST_REG: \t\t0x%02x 0x%08x\n", PMCTR_SW_RST_REG, pmctr_read(pmctr, PMCTR_SW_RST_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_WARM_RST_STATUS_REG: \t0x%02x 0x%08x\n", PMCTR_WARM_RST_STATUS_REG, pmctr_read(pmctr, PMCTR_WARM_RST_STATUS_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,	
			"PMCTR_PDM_RST_STATUS_REG: \t0x%02x 0x%08x\n", PMCTR_PDM_RST_STATUS_REG, pmctr_read(pmctr, PMCTR_PDM_RST_STATUS_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_CORE_PWR_STATUS_REG: \t0x%02x 0x%08x\n", PMCTR_CORE_PWR_STATUS_REG, pmctr_read(pmctr, PMCTR_CORE_PWR_STATUS_REG));	
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,	
			"PMCTR_CORE_PWR_IMASK_REG: \t0x%02x 0x%08x\n", PMCTR_CORE_PWR_IMASK_REG, pmctr_read(pmctr, PMCTR_CORE_PWR_IMASK_REG));
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"PMCTR_CORE_PWR_IRSTAT_REG: \t0x%02x 0x%08x\n", PMCTR_CORE_PWR_IRSTAT_REG, pmctr_read(pmctr, PMCTR_CORE_PWR_IRSTAT_REG));				
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,	
			"PMCTR_CORE_PWR_ISTAT_REG: \t0x%02x 0x%08x\n", PMCTR_CORE_PWR_ISTAT_REG, pmctr_read(pmctr, PMCTR_CORE_PWR_ISTAT_REG));		
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,	
			"PMCTR_CORE_PWR_DELAY_REG: \t0x%02x 0x%08x\n", PMCTR_CORE_PWR_DELAY_REG, pmctr_read(pmctr, PMCTR_CORE_PWR_DELAY_REG));					
	len += snprintf(buf + len, PMCTR_REGS_BUFSIZE - len,
			"=================================\n");

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations pmctr_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= pmctr_show_regs,
	.llseek		= default_llseek,
};

static int pmctr_debugfs_init(struct mcom_pmctr *pmctr)
{
	pmctr->debugfs = debugfs_create_dir("mcom_pmctr", NULL);	
	if (!pmctr->debugfs)
		return -ENOMEM;

	debugfs_create_file("registers", S_IFREG | S_IRUGO,
		pmctr->debugfs, (void *)pmctr, &pmctr_regs_ops);
	return 0;
}

static void pmctr_debugfs_remove(struct mcom_pmctr *pmctr)
{
	debugfs_remove_recursive(pmctr->debugfs);
}

#else
static inline int pmctr_debugfs_init(struct mcom_pmctr *pmctr)
{
	return 0;
}

static inline void pmctr_debugfs_remove(struct mcom_pmctr *pmctr)
{
}
#endif /* CONFIG_DEBUG_FS */

static void dsp_vpu_pwr_up(struct mcom_pmctr *pmctr)
{
	pmctr_write(pmctr, PMCTR_CORE_PWR_UP_REG, (DSP_UP | VPU_UP));
}

static void dsp_vpu_pwr_down(struct mcom_pmctr *pmctr)
{
	pmctr_write(pmctr, PMCTR_CORE_PWR_DOWN_REG, (DSP_DOWN | VPU_DOWN));
}

static ssize_t mcom_pmctr_dsp_vpu_pwr_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct mcom_pmctr *pmctr = dev_get_drvdata(dev);
    int len;

    len = sprintf(buf, "%lu\n", pmctr->dsp_vpu_pwr_state);
    if (len <= 0)
        dev_err(dev, "Invalid sprintf len: %d\n", len);

    return len;
}

static ssize_t mcom_pmctr_dsp_vpu_pwr_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct mcom_pmctr *pmctr = dev_get_drvdata(dev);
    int ret;
    long val;

    ret = kstrtol(buf, 10, &val);
    if (ret)
		return ret;
	
	if (val == 1) {
		dsp_vpu_pwr_up(pmctr);
		pmctr->dsp_vpu_pwr_state = 1;
	}
	else if (val == 0) {
		dsp_vpu_pwr_down(pmctr);
		pmctr->dsp_vpu_pwr_state = 0;
	}
	else {
		dev_err(dev, "Invalid value: %lu\n", val);
		return -ENXIO;
	}
			
    return count;
}

static DEVICE_ATTR(dsp_vpu_pwr, S_IRUGO | S_IWUSR, mcom_pmctr_dsp_vpu_pwr_show,
                   mcom_pmctr_dsp_vpu_pwr_store);
                   
static struct attribute *mcom_pmctr_attrs[] = {
    &dev_attr_dsp_vpu_pwr.attr,
    NULL
};

static struct attribute_group mcom_pmctr_attr_group = {
    .name = "mcom_pmctr",
    .attrs = mcom_pmctr_attrs,
};

int mcom_pmctr_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mcom_pmctr *pmctr;
	int ret = -EINVAL;

	dev_info(&pdev->dev, "PMCTR controller probe...\n");	

	pmctr = devm_kzalloc(&pdev->dev, sizeof(struct mcom_pmctr),
				GFP_KERNEL);
	if (!pmctr) {
		dev_err(&pdev->dev, "Failed to allocate PMCTR controller!\n");
		return -ENOMEM;
	}
			
	pmctr->dev = &pdev->dev;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	pmctr->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pmctr->reg_base)) {
		ret = PTR_ERR(pmctr->reg_base);
		return ret;
	}
	
	platform_set_drvdata(pdev, pmctr);
	
	pmctr_debugfs_init(pmctr);
	
	ret = sysfs_create_group(&pdev->dev.kobj, &mcom_pmctr_attr_group);
    if (ret) {
        dev_err(&pdev->dev, "sysfs creation mcom_pmctr failed\n");
        return ret;
    }
	
	dev_info(&pdev->dev, "PMCTR demo driver loaded successfully!\n");

	return 0;
	
}

int mcom_pmctr_remove(struct platform_device *pdev)
{
	struct mcom_pmctr *pmctr = platform_get_drvdata(pdev);
	
	pmctr_debugfs_remove(pmctr);
	sysfs_remove_group(&pdev->dev.kobj, &mcom_pmctr_attr_group);
	return 0;
}

static const struct of_device_id mcom_pmctr_of_match[] = {
        { .compatible = "elvees,mcom-pmctr" },
        { /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, mcom_pmctr_of_match);

static struct platform_driver mcom_pmctr_driver = {
	.probe = mcom_pmctr_probe,
	.remove = mcom_pmctr_remove,
	.driver = {
		   .name = "pmctr",
		   .of_match_table = of_match_ptr(mcom_pmctr_of_match),
	},
};

module_platform_driver(mcom_pmctr_driver);

MODULE_AUTHOR("Michael Sadikov");
MODULE_DESCRIPTION("Elvees PMCTR controller demo driver");
MODULE_LICENSE("GPL");
