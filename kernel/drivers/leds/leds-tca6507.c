#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/leds-tca6507.h>

struct led_data {
	struct i2c_client *client;
	struct led_classdev ldev;
	uint8_t output_pin;
	uint8_t state;
};

struct tca6507_data {
	int enable;
	struct mutex mutex;
	struct i2c_client *client;
	struct led_data leds[1];
};

struct tca6507_select_control {
	char reg;
	char data[10];
};

static const char *led_blink_texts[] = {
	"Off",
	"On",
	"Fast-Blink",
	"Slow-Blink",
};

static int i2c_read(struct i2c_client *client, char *buf, int count)
{
	/* The first byte should be the register offset */
	if (1 != i2c_master_send(client, buf, 1)) {
		pr_err("tca6507_i2c_read: Send register info failed!\n");
		return -1;
	}

	/* The received buffer must be from the second byte of buffer */
	if (count != i2c_master_recv(client, buf + 1, count)) {
		pr_err("tca6507_i2c_read: Get response failed!\n");
		return -1;
	}

	return 0;
}

static int i2c_write(struct i2c_client *client, char *buf, int count)
{
	if (count != i2c_master_send(client, buf, count))
		return -1;

	return 0;
}

static void tca6507_enable_check(struct tca6507_data *data)
{
	int i;
	struct led_data *ldata = data->leds;
	struct tca6507_platform_data *pdata = data->client->dev.platform_data;
	int num_output = pdata->num_output_pins;

	for (i = 0; i < num_output; i++)
		if ((ldata + i)->state != TCA6507_OFF)
			return;

	/* disable the chip for power saving */
	data->enable = false;
	gpio_direction_output(pdata->gpio_enable, 0);
	pr_info("%s: tca6507 disable!\n", __func__);
}

static ssize_t tca6507_output_control(struct led_data *ldata, uint8_t state)
{
	int ret = 0;
	int port_setting;
	int length = sizeof(struct tca6507_select_control);
	int index = ldata->output_pin;
	struct tca6507_select_control i2c_buf = {0};
	struct tca6507_data *data = i2c_get_clientdata(ldata->client);
	struct tca6507_platform_data *pdata = data->client->dev.platform_data;

	switch (state) {
	default:
	case TCA6507_OFF:
		port_setting = TCA6507_PORT_LED_OFF;
		break;
	case TCA6507_ON:
		port_setting = TCA6507_PORT_LED_ON_FULLY;
		break;
	case TCA6507_FAST_BLINK:
		port_setting = TCA6507_PORT_LED_BLINK_PWM0;
		break;
	case TCA6507_SLOW_BLINK:
		port_setting = TCA6507_PORT_LED_BLINK_PWM1;
		break;
	}

	mutex_lock(&data->mutex);

	if (data->enable) {
		/* Only read the select registers */
		i2c_buf.reg =  TCA6507_REG_SELECT0 | TCA6507_AUTO_INCREMENT;
		if (i2c_read(ldata->client, (char *)&i2c_buf, 3)) {
			pr_err("%s: i2c read failed!\n", __func__);
			ret = -EIO;
			goto exit_i2c_failed;
		}

		i2c_buf.data[0] &= ~(0x01 << index);
		i2c_buf.data[1] &= ~(0x01 << index);
		i2c_buf.data[2] &= ~(0x01 << index);

		/* command + select0,1,2 */
		length = 4;
	} else {
		/* enable the chip before access */
		data->enable = true;
		gpio_direction_output(pdata->gpio_enable, 1);
		pr_info("%s: tca6507 enable!\n", __func__);

		/* Initialize the chip setting */
		i2c_buf.data[3] = (pdata->pwm1.fade_on << 4) | pdata->pwm0.fade_on;
		i2c_buf.data[4] = (pdata->pwm1.fully_on << 4) | pdata->pwm0.fully_on;
		i2c_buf.data[5] = (pdata->pwm1.fade_off << 4) | pdata->pwm0.fade_off;
		i2c_buf.data[6] = (pdata->pwm1.fir_fully_off << 4) | pdata->pwm0.fir_fully_off;
		i2c_buf.data[7] = (pdata->pwm1.sec_fully_off << 4) | pdata->pwm0.sec_fully_off;
		i2c_buf.data[8] = (pdata->pwm1.max_intensity << 4) | pdata->pwm0.max_intensity;
		i2c_buf.data[9] = TCA6507_MAX_ALD_VALUE;
	}

	/* Change the output */
	i2c_buf.reg = TCA6507_REG_SELECT0 | TCA6507_AUTO_INCREMENT;
	i2c_buf.data[0] |= ((port_setting & 0x01) << index);
	i2c_buf.data[1] |= (((port_setting & 0x02) >> 1) << index);
	i2c_buf.data[2] |= (((port_setting & 0x04) >> 2) << index);

	if (i2c_write(ldata->client, (char *)&i2c_buf, length)) {
		pr_err("%s: i2c write failed!\n", __func__);
		ret = -EIO;
		goto exit_i2c_failed;
	}

	ldata->state = state;

	tca6507_enable_check(data);

exit_i2c_failed:
	mutex_unlock(&data->mutex);

	return ret;
}

static ssize_t led_blink_solid_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev;
	struct led_data *ldata;
	struct tca6507_data *data;
	uint8_t state;
	ssize_t ret = 0;

	led_cdev = dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct led_data, ldev);
	data = i2c_get_clientdata(ldata->client);

	mutex_lock(&data->mutex);
	state = ldata->state;
	mutex_unlock(&data->mutex);

	ret += sprintf(&buf[ret], "%s\n", led_blink_texts[state]);

	return ret;
}

static ssize_t led_blink_solid_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct led_classdev *led_cdev;
	struct led_data *ldata;
	char *after;
	uint8_t state;
	size_t count;
	ssize_t ret = -EINVAL;

	led_cdev = dev_get_drvdata(dev);
	ldata = container_of(led_cdev, struct led_data, ldev);

	state = (uint8_t)simple_strtoul(buf, &after, 10);
	count = after - buf;
	if (*after && isspace(*after))
		count++;

	if (count == size) {
		ret = tca6507_output_control(ldata, state);

		if (!ret)
			ret = count;
	}

	return ret;
}

static DEVICE_ATTR(blink, 0644, led_blink_solid_show, led_blink_solid_store);

static void led_brightness_set(struct led_classdev *led_cdev,
			       enum led_brightness brightness)
{
	struct led_data *ldata;
	uint8_t state;

	ldata = container_of(led_cdev, struct led_data, ldev);

	if (brightness == LED_OFF)
		state = TCA6507_OFF;
	else
		state = TCA6507_ON;

	tca6507_output_control(ldata, state);
}

static int tca6507_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	int i, j, num_leds;
	struct tca6507_data *data;
	struct tca6507_platform_data *pdata;
	struct led_data *ldata;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c_check_functionality failed!\n", __func__);
		ret = -EIO;
		goto exit_i2c_check_failed;
	}

	pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("%s: platform data not set\n", __func__);
		ret = -EFAULT;
		goto exit_platform_data_failed;
	}

	num_leds = pdata->num_output_pins;
	if (num_leds > TCA6507_MAX_OUTPUT) {
		pr_err("%s: the configed output exceeds the max value %d\n",
						__func__, TCA6507_MAX_OUTPUT);
		ret = -EFAULT;
		goto exit_platform_data_failed;
	}

	data = kzalloc(sizeof(struct tca6507_data) +
				sizeof(struct led_data) * (num_leds - 1), GFP_KERNEL);
	if (data == NULL) {
		pr_err("%s: no memory for driver data\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc_failed;
	}
	i2c_set_clientdata(client, data);
	data->client = client;

	mutex_init(&data->mutex);

	ldata = data->leds;
	for (i = 0; i < num_leds; i++) {
		(ldata + i)->ldev.name = pdata->pin_config[i].name;
		(ldata + i)->ldev.brightness_set = led_brightness_set;
		(ldata + i)->output_pin = pdata->pin_config[i].output_pin;
		(ldata + i)->client = client;

		ret = led_classdev_register(&client->dev, &(ldata + i)->ldev);
		if (ret) {
			pr_err("%s: led_classdev_register failed\n", __func__);
			goto exit_led_classdev_register_failed;
		}
	}

	for (i = 0; i < num_leds; i++) {
		ret =
		    device_create_file((ldata + i)->ldev.dev, &dev_attr_blink);
		if (ret) {
			pr_err("%s: device_create_file failed\n", __func__);
			goto exit_attr_blink_create_failed;
		}
	}

	/* Set tca6507_en_pin as output high */
	ret = gpio_request(pdata->gpio_enable, "LED_DRIVER_EN");
	if (ret) {
		pr_err("%s: gpio_request failed on pin %d (rc=%d)\n",
								__func__, pdata->gpio_enable, ret);
		goto exit_gpio_request_failed;
	}

	strlcpy(client->name, TCA6507_NAME, I2C_NAME_SIZE);

	return 0;

exit_gpio_request_failed:
exit_attr_blink_create_failed:
	for (j = 0; j < i; j++)
		device_remove_file((ldata + j)->ldev.dev, &dev_attr_blink);
	i = num_leds;

exit_led_classdev_register_failed:
	for (j = 0; j < i; j++)
		led_classdev_unregister(&(ldata + j)->ldev);
	kfree(data);

exit_kzalloc_failed:
exit_platform_data_failed:
exit_i2c_check_failed:

	return ret;
}

static int tca6507_remove(struct i2c_client *client)
{
	int i;
	struct tca6507_data *data = i2c_get_clientdata(client);
	struct tca6507_platform_data *pdata = client->dev.platform_data;
	struct led_data *ldata = data->leds;

	for (i = 0; i < pdata->num_output_pins; i++) {
		device_remove_file((ldata + i)->ldev.dev, &dev_attr_blink);
		led_classdev_unregister(&(ldata + i)->ldev);
	}

	gpio_free(pdata->gpio_enable);

	kfree(data);
	i2c_set_clientdata(client, NULL);
	return 0;
}

static const struct i2c_device_id tca6507_id[] = {
	{ TCA6507_NAME, 0 },
	{ }
};

static struct i2c_driver tca6507_driver = {
	.probe    = tca6507_probe,
	.remove   = tca6507_remove,
	.id_table = tca6507_id,
	.driver   = {
	    .name = TCA6507_NAME,
	},
};

static int __init tca6507_init(void)
{
	int res = 0;

	res = i2c_add_driver(&tca6507_driver);
	if (res) {
		pr_err("%s: i2c_add_driver failed!\n", __func__);
		return res;
	}

	return 0;
}

static void __exit tca6507_exit(void)
{
	i2c_del_driver(&tca6507_driver);
}

module_init(tca6507_init);
module_exit(tca6507_exit);

MODULE_AUTHOR("Brad Chen <ChunHung_Chen@acer.com.tw");
MODULE_DESCRIPTION("tca6507 driver");
MODULE_LICENSE("GPL");
