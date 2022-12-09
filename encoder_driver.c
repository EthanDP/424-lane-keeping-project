// References: https://github.com/Johannes4Linux/Linux_Driver_Tutorial/blob/main/11_gpio_irq/gpio_irq.c
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "carencoder"
#define CLASS_NAME "carclass"

static int major_number;
static struct class* encoder_class = NULL;
static struct device* encoder_device = NULL;

static int device_open(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);

static struct file_operations fops = {
    .open = device_open,
    .read = device_read,
};

unsigned int irq_number;
struct gpio_desc *led_gpio;
struct gpio_desc *encoder_gpio;

static int elapsed_ms = 0;
ktime_t start_time, old_time, elapsed;

module_param(elapsed_ms, int, S_IRUGO);

static int device_open(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "Encoder device opened.");
    return 0;
}

static ssize_t device_read(struct file *filep, char __user *buf, size_t length, loff_t *offset){
    return (ssize_t) elapsed_ms;
}

static irq_handler_t encoder_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs) {

    ktime_t new_time = ktime_get();
    elapsed = ktime_sub(new_time, old_time);
    old_time = new_time;
    elapsed_ms = ktime_to_ns(elapsed) / 100000;

    printk("Elapsed: %i\n", elapsed_ms);

    return (irq_handler_t) IRQ_HANDLED;
}


static int led_probe(struct platform_device *pdev) {
    
    printk("Setting up encoder IRQ.\n");

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    encoder_class = class_create(THIS_MODULE, CLASS_NAME);
    encoder_device = device_create(encoder_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);

    old_time = ktime_get();

    encoder_gpio = devm_gpiod_get(&pdev->dev, "userbutton", GPIOD_IN);
    gpiod_set_debounce(encoder_gpio, 1000000);
    irq_number = gpiod_to_irq(encoder_gpio);
    // Checks if the irq request was successful
    if (request_irq(irq_number, (irq_handler_t) encoder_irq_handler, IRQF_TRIGGER_RISING, "button_gpio_irq", &pdev->dev) != 0) {
        printk("Failed to request IRQ.\n");
        return -1;
    }

    printk("Successfully requested IRQ.\n");

    return 0;
}


static int led_remove(struct platform_device *pdev) {
    struct device *temp_dev;
    device_destroy(encoder_class, MKDEV(major_number,0));
    class_unregister(encoder_class);
    class_destroy(encoder_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    temp_dev = &pdev->dev;
    free_irq(irq_number, &temp_dev->id);
    printk("IRQ Freed!\n");
    return 0;
}


static struct of_device_id encoder_gpio_match[] = {
    {
    .compatible = "encoder-gpio",
    },
    {/* leave alone - keep this here (end node) */},
};


static struct platform_driver encoder_gpio_driver = {
    .probe = led_probe,
    .remove = led_remove,
    .driver = {
        .name = "encoder_gpio",
        .owner = THIS_MODULE,
        .of_match_table = encoder_gpio_match,
    },
};

module_platform_driver(encoder_gpio_driver);
MODULE_DESCRIPTION("Use of the accursed gpiod library to do some blinking LED stuff");
MODULE_AUTHOR("Ethan Peck");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:encoder_gpio_driver");
