// References: https://github.com/Johannes4Linux/Linux_Driver_Tutorial/blob/main/11_gpio_irq/gpio_irq.c
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#define DEVICE_NAME "fred"

unsigned int irq_number;
struct gpio_desc *led_gpio;
struct gpio_desc *encoder_gpio;

int elapsed_ms;
ktime_t start_time, old_time, elapsed;


static irq_handler_t encoder_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs) {

    ktime_t new_time = ktime_get();
    elapsed = ktime_sub(new_time, old_time);
    old_time = new_time;
    elapsed_ms = ktime_to_ns(elapsed) / 100000;

    printk("Elapsed: %i\n", elapsed_ms);

    return (irq_handler_t) IRQ_HANDLED;
}


static int led_probe(struct platform_device *pdev) {
    
    old_time = ktime_get();

    encoder_gpio = devm_gpiod_get(&pdev->dev, "button", GPIOD_IN);
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
