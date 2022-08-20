/*
 * Elvees mcom02 Real Time Clock interface for Linux
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/clk.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

/*
 * The mcom02 RTC is a year/month/day/hours/minutes/seconds/sixteenth_part_of_second 
 * BCD clock with century-range alarm matching, driven by the 32kHz clock.
 */

/* RTC registers */
#define MCOM02_RTC_ID_REG		0x00
#define MCOM02_RTC_CTRL_REG		0x04
#define MCOM02_RTC_TIME_REG		0x08
#define MCOM02_RTC_DATE_REG		0x0C
#define MCOM02_RTC_TALRM_REG	0x10
#define MCOM02_RTC_DALRM_REG	0x14
#define MCOM02_RTC_STAT_REG		0x18
#define MCOM02_RTC_TCNT_REG		0x1C
#define MCOM02_RTC_TCUR_REG		0x20

/* RTC CTRL_REG bit fields: */
#define CTRL_INT_WKUP_EN		BIT(3)
#define CTRL_ALRM_WKUP_EN		BIT(2)
#define CTRL_INT_IT_EN			BIT(1)
#define CTRL_INT_ALRM_EN		BIT(0)

/* RTC STATUS_REG bit fields: */
#define STATUS_APB_RESET		BIT(5)
#define STATUS_RTC_RESET		BIT(4)
#define STATUS_IT_WKUP			BIT(3)
#define STATUS_ALRM_WKUP		BIT(2)
#define STATUS_INT_IT			BIT(1)
#define STATUS_INT_ALRM			BIT(0)

#define DATE_TCEN				BIT(24)
#define DATE_MCEN				BIT(30)

#define TIME_SEC_S              4
#define TIME_SEC_MASK           (0x0F << TIME_SEC_S)
#define TIME_TENSEC_S           8
#define TIME_TENSEC_MASK        (0x07 << TIME_TENSEC_S)

#define TIME_MIN_S              11
#define TIME_MIN_MASK           (0x0F << TIME_MIN_S)
#define TIME_TENMIN_S           15
#define TIME_TENMIN_MASK        (0x07 << TIME_TENMIN_S)

#define TIME_HOUR_S             18
#define TIME_HOUR_MASK          (0x0F << TIME_HOUR_S)
#define TIME_TENHOUR_S          22
#define TIME_TENHOUR_MASK       (0x03 << TIME_TENHOUR_S) 

#define TIME_DOW_S             	24
#define TIME_DOW_MASK          	(0x07 << TIME_DOW_S)

#define TIME_DAY_S              0
#define TIME_DAY_MASK           (0x0F << TIME_DAY_S)
#define TIME_TENDAY_S           4
#define TIME_TENDAY_MASK        (0x03 << TIME_TENDAY_S)

#define TIME_MON_S              6
#define TIME_MON_MASK           (0x0F << TIME_MON_S)
#define TIME_TENMON_S           10
#define TIME_TENMON_MASK        (0x01 << TIME_TENMON_S)

#define TIME_YEAR_S             11
#define TIME_YEAR_MASK          (0x0F << TIME_YEAR_S)
#define TIME_TENYEAR_S          15
#define TIME_TENYEAR_MASK       (0x07 << TIME_TENYEAR_S)

#define TIME_CEN_S             	19
#define TIME_CEN_MASK          	(0x0F << TIME_CEN_S)
#define TIME_TENCEN_S          	23
#define TIME_TENCEN_MASK       	(0x0F << TIME_TENCEN_S)

#define TIME_RE_S          		27
#define TIME_RE_MASK       		(0x1F << TIME_RE_S)

#define DATE_RE_S          		27
#define DATE_RE_MASK       		(0x0F << DATE_RE_S)

#define ALRM_TIME_RE_S          27
#define ALRM_TIME_RE_MASK       (0x0F << ALRM_TIME_RE_S)

#define ALRM_DATE_RE_S          27
#define ALRM_DATE_RE_MASK       (0x0F << ALRM_DATE_RE_S)

struct mcom02_rtc {
	struct rtc_device *rtc;
	void __iomem *base;
	int irq_alarm;
	int irq_timer;
	struct clk *rtc_clk;
	spinlock_t lock;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs;
#endif	
};

static u32 rtc_read(struct mcom02_rtc *rtc, unsigned int reg)
{
	return readl(rtc->base + reg);
}

static void rtc_write(struct mcom02_rtc *rtc, unsigned int reg, u32 val)
{
	writel(val, rtc->base + reg);
}

#ifdef CONFIG_DEBUG_FS
#define RTC_REGS_BUFSIZE	1024
static ssize_t mcom02_rtc_show_regs(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct mcom02_rtc *rtc = file->private_data;
	char *buf;
	u32 len = 0;
	ssize_t ret;

	buf = kzalloc(RTC_REGS_BUFSIZE, GFP_KERNEL);
	if (!buf)
		return 0;
	
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"%s registers:\n", "rtc");
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"===========================================\n");
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"MCOM02_RTC_CTRL_REG: \t0x%02x 0x%08x\n", MCOM02_RTC_CTRL_REG, rtc_read(rtc, MCOM02_RTC_CTRL_REG));
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"MCOM02_RTC_TIME_REG: \t0x%02x 0x%08x\n", MCOM02_RTC_TIME_REG, rtc_read(rtc, MCOM02_RTC_TIME_REG));
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"MCOM02_RTC_DATE_REG: \t0x%02x 0x%08x\n", MCOM02_RTC_DATE_REG, rtc_read(rtc, MCOM02_RTC_DATE_REG));		
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"MCOM02_RTC_TALRM_REG: \t0x%02x 0x%08x\n", MCOM02_RTC_TALRM_REG, rtc_read(rtc, MCOM02_RTC_TALRM_REG));
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"MCOM02_RTC_DALRM_REG: \t0x%02x 0x%08x\n", MCOM02_RTC_DALRM_REG, rtc_read(rtc, MCOM02_RTC_DALRM_REG));
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"MCOM02_RTC_STAT_REG: \t0x%02x 0x%08x\n", MCOM02_RTC_STAT_REG, rtc_read(rtc, MCOM02_RTC_STAT_REG));						
	len += snprintf(buf + len, RTC_REGS_BUFSIZE - len,
			"===========================================\n");

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

static const struct file_operations mcom02_rtc_regs_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= mcom02_rtc_show_regs,
	.llseek		= default_llseek,
};

static int mcom02_rtc_debugfs_init(struct mcom02_rtc *rtc)
{
	rtc->debugfs = debugfs_create_dir("rtc", NULL);	
	if (!rtc->debugfs)
		return -ENOMEM;

	debugfs_create_file("registers", S_IFREG | S_IRUGO,
		rtc->debugfs, (void *)rtc, &mcom02_rtc_regs_ops);
	return 0;
}

static void mcom02_rtc_debugfs_remove(struct mcom02_rtc *rtc)
{
	debugfs_remove_recursive(rtc->debugfs);
}

#else
static inline int mcom02_rtc_debugfs_init(struct mcom02_rtc *rtc)
{
	return 0;
}

static inline void mcom02_rtc_debugfs_remove(struct mcom02_rtc *rtc)
{
}
#endif /* CONFIG_DEBUG_FS */

static irqreturn_t rtc_irq_handler(int irq, void *dev_id)
{
	struct mcom02_rtc	*rtc = dev_id;
	unsigned long events = 0;
	u32 irq_data;

	/*pr_info("rtc irq!\n");*/
	irq_data = rtc_read(rtc, MCOM02_RTC_STAT_REG);

	/* alarm irq? */
	if (irq_data & STATUS_INT_ALRM) {
		/*pr_info("alrm irq!\n");*/
		irq_data |= STATUS_INT_ALRM;
		rtc_write(rtc, MCOM02_RTC_STAT_REG, irq_data);
		events |= RTC_IRQF | RTC_AF;
	}

	/* periodic/update irq? */
	if (irq_data & STATUS_INT_IT) {
		irq_data |= STATUS_INT_IT;
		rtc_write(rtc, MCOM02_RTC_STAT_REG, irq_data);
		events |= RTC_IRQF | RTC_UF;
	}

	rtc_update_irq(rtc->rtc, 1, events);

	return IRQ_HANDLED;
}

static int mcom02_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct mcom02_rtc *rtc = dev_get_drvdata(dev);
	u32 reg;

	local_irq_disable();
	reg = rtc_read(rtc, MCOM02_RTC_CTRL_REG);
	if (enabled) {
		reg |= CTRL_INT_ALRM_EN;
		reg |= CTRL_ALRM_WKUP_EN;
	} else {
		reg &= ~CTRL_INT_ALRM_EN;
		reg &= ~CTRL_ALRM_WKUP_EN;
	}

	rtc_write(rtc, MCOM02_RTC_CTRL_REG, reg);	
	local_irq_enable();

	return 0;
}

static int mcom02_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct mcom02_rtc *rtc = dev_get_drvdata(dev);
	u32 date, time;

	local_irq_disable();
	date = rtc_read(rtc, MCOM02_RTC_DATE_REG);
	time = rtc_read(rtc, MCOM02_RTC_TIME_REG);
	local_irq_enable();

	/* The number of seconds after the minute, 
	 * normally in the range 0 to 59, but can be up to 60 
	 * to allow for leap seconds */
	tm->tm_sec = ((time & TIME_TENSEC_MASK) >> TIME_TENSEC_S)*10 + 
					((time & TIME_SEC_MASK) >> TIME_SEC_S);
	/* The number of minutes after the hour, in the range 0 to 59 */
	tm->tm_min = ((time & TIME_TENMIN_MASK) >> TIME_TENMIN_S)*10 + 
					((time & TIME_MIN_MASK) >> TIME_MIN_S);
	/* The number of hours past midnight, in the range 0 to 23 */
	tm->tm_hour = ((time & TIME_TENHOUR_MASK) >> TIME_TENHOUR_S)*10 + 
					((time & TIME_HOUR_MASK) >> TIME_HOUR_S);
	/* The day of the month, in the range 1 to 31 */
	tm->tm_mday = ((date & TIME_TENDAY_MASK) >> TIME_TENDAY_S)*10 + 
					((date & TIME_DAY_MASK) >> TIME_DAY_S);
	/* The number of months since January, in the range 0 to 11 */
	tm->tm_mon = ((date & TIME_TENMON_MASK) >> TIME_TENMON_S)*10 + 
					((date & TIME_MON_MASK) >> TIME_MON_S);
	/* The number of years since 1900 */
	tm->tm_year = ((date & TIME_TENCEN_MASK) >> TIME_TENCEN_S)*1000 + 
					((date & TIME_CEN_MASK) >> TIME_CEN_S)*100 + 
					((date & TIME_TENYEAR_MASK) >> TIME_TENYEAR_S)*10 + 
					((date & TIME_YEAR_MASK) >> TIME_YEAR_S)/*-1900*/;
 	// !!!!!! Если не вычитать здесь 1900, то нормально работает hwclock -r
	// Если вычитать, то hwclock -r блокируется, а затем выходит с ошибкой:
	// select() to /dev/rtc to wait for clock tick timed out...synchronization failed
	if (tm->tm_year >= 1900)
		tm->tm_year -= 1900;
	/* The number of days since Sunday, in the range 0 to 6 */
	//tm->tm_wday = ((date & TIME_DOW_MASK) >> TIME_DOW_S);
	
	/*dev_info(dev, "%s: %4d-%02d-%02d %02d:%02d:%02d\n", "readtime",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);*/

	return 0;
}

static int mcom02_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct mcom02_rtc *rtc = dev_get_drvdata(dev);
	u32 date, time;
	
	/*dev_info(dev, "%s: %4d-%02d-%02d %02d:%02d:%02d\n", "settime",
		tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);*/
		
	if (tm->tm_year <= 1900) 											
		tm->tm_year += 1900;
	
	time =  (((tm->tm_sec / 10) << TIME_TENSEC_S) |
			((tm->tm_sec % 10) << TIME_SEC_S) |
			((tm->tm_min / 10) << TIME_TENMIN_S) |
			((tm->tm_min % 10) << TIME_MIN_S) |
			((tm->tm_hour / 10) << TIME_TENHOUR_S) |
			((tm->tm_hour % 10) << TIME_HOUR_S) | TIME_RE_MASK);
			
	date =	(((tm->tm_year / 1000) << TIME_TENCEN_S) |
			(((tm->tm_year % 1000) / 100) << TIME_CEN_S) |
			((((tm->tm_year % 1000) % 100) / 10) << TIME_TENYEAR_S) |
			((((tm->tm_year % 1000) % 100) % 10) << TIME_YEAR_S) |
			((tm->tm_mon / 10) << TIME_TENMON_S) |
			((tm->tm_mon % 10) << TIME_MON_S) |
			((tm->tm_mday / 10) << TIME_TENDAY_S) |
			((tm->tm_mday % 10) << TIME_DAY_S) | DATE_RE_MASK);
			
	local_irq_disable();
			
	rtc_write(rtc, MCOM02_RTC_TIME_REG, time);
	rtc_write(rtc, MCOM02_RTC_DATE_REG, date);

	local_irq_enable();
	
	return 0;
}

static int mcom02_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct mcom02_rtc *rtc = dev_get_drvdata(dev);
	u32 alrm_date, alrm_time, reg;
	
	local_irq_disable();
	alrm_date = rtc_read(rtc, MCOM02_RTC_DALRM_REG);
	alrm_time = rtc_read(rtc, MCOM02_RTC_TALRM_REG);
	local_irq_enable();
	
	alm->time.tm_sec = ((alrm_time & TIME_TENSEC_MASK) >> TIME_TENSEC_S)*10 + 
					((alrm_time & TIME_SEC_MASK) >> TIME_SEC_S);
	alm->time.tm_min = ((alrm_time & TIME_TENMIN_MASK) >> TIME_TENMIN_S)*10 + 
					((alrm_time & TIME_MIN_MASK) >> TIME_MIN_S);
	alm->time.tm_hour = ((alrm_time & TIME_TENHOUR_MASK) >> TIME_TENHOUR_S)*10 + 
					((alrm_time & TIME_HOUR_MASK) >> TIME_HOUR_S);
	alm->time.tm_mday = ((alrm_date & TIME_TENDAY_MASK) >> TIME_TENDAY_S)*10 + 
					((alrm_date & TIME_DAY_MASK) >> TIME_DAY_S);
	alm->time.tm_mon = ((alrm_date & TIME_TENMON_MASK) >> TIME_TENMON_S)*10 + 
					((alrm_date & TIME_MON_MASK) >> TIME_MON_S);
	alm->time.tm_year = ((alrm_date & TIME_TENCEN_MASK) >> TIME_TENCEN_S)*1000 + 
					((alrm_date & TIME_CEN_MASK) >> TIME_CEN_S)*100 + 
					((alrm_date & TIME_TENYEAR_MASK) >> TIME_TENYEAR_S)*10 + 
					((alrm_date & TIME_YEAR_MASK) >> TIME_YEAR_S);
	if (alm->time.tm_year >= 1900) 											
		alm->time.tm_year -= 1900;
	
	reg = rtc_read(rtc, MCOM02_RTC_CTRL_REG);
	alm->enabled = (reg & CTRL_INT_ALRM_EN);
	
	/*dev_info(dev, "%s: %4d-%02d-%02d %02d:%02d:%02d\n", "readalarm",
		alm->time.tm_year, alm->time.tm_mon+1, alm->time.tm_mday,
		alm->time.tm_hour+1900, alm->time.tm_min, alm->time.tm_sec);*/
	
	return 0;
}

static int mcom02_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alm)
{
	struct mcom02_rtc *rtc = dev_get_drvdata(dev);
	u32 alrm_date, alrm_time;
	
	/*pr_info("mcom02_rtc_set_alarm\n");*/
	
	if (alm->time.tm_year <= 1900) 											
		alm->time.tm_year += 1900;
	
	alrm_time =  (((alm->time.tm_sec / 10) << TIME_TENSEC_S) |
			((alm->time.tm_sec % 10) << TIME_SEC_S) |
			((alm->time.tm_min / 10) << TIME_TENMIN_S) |
			((alm->time.tm_min % 10) << TIME_MIN_S) |
			((alm->time.tm_hour / 10) << TIME_TENHOUR_S) |
			((alm->time.tm_hour % 10) << TIME_HOUR_S) | ALRM_TIME_RE_MASK);
			
	alrm_date =	(((alm->time.tm_year / 1000) << TIME_TENCEN_S) |
			(((alm->time.tm_year % 1000) / 100) << TIME_CEN_S) |
			((((alm->time.tm_year % 1000) % 100) / 10) << TIME_TENYEAR_S) |
			((((alm->time.tm_year % 1000) % 100) % 10) << TIME_YEAR_S) |
			((alm->time.tm_mon / 10) << TIME_TENMON_S) |
			((alm->time.tm_mon % 10) << TIME_MON_S) |
			((alm->time.tm_mday / 10) << TIME_TENDAY_S) |
			((alm->time.tm_mday % 10) << TIME_DAY_S) | ALRM_DATE_RE_MASK);
			
	local_irq_disable();
	
	rtc_write(rtc, MCOM02_RTC_TALRM_REG, alrm_time);
	rtc_write(rtc, MCOM02_RTC_DALRM_REG, alrm_date);
	
	if (alm->enabled)
		mcom02_rtc_alarm_irq_enable(dev, 1);
	else
		mcom02_rtc_alarm_irq_enable(dev, 0);

	local_irq_enable();
	
	/*dev_info(dev, "%s: %4d-%02d-%02d %02d:%02d:%02d\n", "setalarm",
		alm->time.tm_year, alm->time.tm_mon+1, alm->time.tm_mday,
		alm->time.tm_hour, alm->time.tm_min, alm->time.tm_sec);*/
	
	return 0;
}

static struct rtc_class_ops mcom02_rtc_ops = {
	.read_time	= mcom02_rtc_read_time,
	.set_time	= mcom02_rtc_set_time,
	.read_alarm	= mcom02_rtc_read_alarm,
	.set_alarm	= mcom02_rtc_set_alarm,
	.alarm_irq_enable = mcom02_rtc_alarm_irq_enable,
};

static const struct platform_device_id mcom02_rtc_id_table[] = {
	{ .name	= "mcom02-rtc"}, 
	{},
};
MODULE_DEVICE_TABLE(platform, mcom02_rtc_id_table);

static const struct of_device_id mcom02_rtc_of_match[] = {
	{ .compatible = "elvees,mcom02-rtc" },
	{},
};
MODULE_DEVICE_TABLE(of, mcom02_rtc_of_match);

static int mcom02_rtc_probe(struct platform_device *pdev)
{
	struct mcom02_rtc *rtc;
	struct resource	*res;
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;
	
	rtc->irq_timer = platform_get_irq(pdev, 0);
	if (rtc->irq_timer <= 0)
		return -ENOENT;

	rtc->irq_alarm = platform_get_irq(pdev, 2);
	if (rtc->irq_alarm <= 0)
		return -ENOENT;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc->base))
		return PTR_ERR(rtc->base);

	platform_set_drvdata(pdev, rtc);
	
	mcom02_rtc_debugfs_init(rtc);
	
	/* disable interrupts */
	rtc_write(rtc, MCOM02_RTC_CTRL_REG, 0);
	
	/* clear interrupts */
	rtc_write(rtc, MCOM02_RTC_STAT_REG, 0x3F);

	rtc->rtc_clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(rtc->rtc_clk)) {
		dev_err(&pdev->dev, "Failed to get RTC clock\n");
		return -ENOMEM;
	}	
	ret = clk_prepare_enable(rtc->rtc_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not prepare or enable RTC clock\n");
		goto err;
	}
	
	device_init_wakeup(&pdev->dev, true);

	rtc->rtc = devm_rtc_device_register(&pdev->dev, pdev->name,
			&mcom02_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc)) {
		ret = PTR_ERR(rtc->rtc);
		goto err;
	}

	/* handle periodic and alarm irqs */
	ret = devm_request_irq(&pdev->dev, rtc->irq_timer, rtc_irq_handler, 0,
			dev_name(&rtc->rtc->dev), rtc);
	if (ret)
		goto err;

	ret = devm_request_irq(&pdev->dev, rtc->irq_alarm, rtc_irq_handler, 0,
			dev_name(&rtc->rtc->dev), rtc);
	if (ret)
		goto err;

	return 0;

err:
	return ret;
}

static int mcom02_rtc_remove(struct platform_device *pdev)
{
	struct mcom02_rtc *rtc= platform_get_drvdata(pdev);
	
	mcom02_rtc_debugfs_remove(rtc);
	
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mcom02_rtc_suspend(struct device *dev)
{
        struct mcom02_rtc *rtc = dev_get_drvdata(dev);
        
        if (device_may_wakeup(dev))
                enable_irq_wake(rtc->irq_alarm);
        
        return 0;
}
        
static int mcom02_rtc_resume(struct device *dev)
{
        struct mcom02_rtc *rtc = dev_get_drvdata(dev);
        
        if (device_may_wakeup(dev))
                disable_irq_wake(rtc->irq_alarm);

        return 0;
}

static SIMPLE_DEV_PM_OPS(mcom02_rtc_pm_ops, mcom02_rtc_suspend, mcom02_rtc_resume);

#endif

static struct platform_driver mcom02_rtc_driver = {
	.probe		= mcom02_rtc_probe,
	.remove		= mcom02_rtc_remove,
	.driver		= {
		.name	= "mcom02-rtc",
#ifdef CONFIG_PM_SLEEP		
		.pm     = &mcom02_rtc_pm_ops,
#endif		
		.of_match_table = mcom02_rtc_of_match,
	},
	.id_table	= mcom02_rtc_id_table,
};

module_platform_driver(mcom02_rtc_driver);

MODULE_ALIAS("platform:mcom02-rtc");
MODULE_AUTHOR("Michael Sadikov");
MODULE_DESCRIPTION("Elvees mcom02 Real Time Clock");
MODULE_LICENSE("GPL");
