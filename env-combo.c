/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ENV-COMBO I2C Sensor Driver with IIO
 *
 * This driver supports a fictional environmental sensor device that provides
 * temperature and relative humidity readings via an I2C interface.
 * It implements the Industrial I/O (IIO) subsystem interface in Linux,
 * enabling access through standard IIO sysfs entries.
 *
 * Developer: Sami Natshe
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/driver.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/mutex.h>

#define ENV_COMBO_DRV_NAME "env-combo"

// Sensor register map definitions
#define REG_WHO_AM_I               0x00
#define REG_TEMP_MSB               0x01
#define REG_TEMP_LSB               0x02
#define REG_HUM_OUT                0x03
#define REG_CFG                    0x06
#define REG_STATUS                 0x0C
#define REG_CALIB_TEMP_MSB         0x0D
#define REG_CALIB_TEMP_LSB         0x0E
#define REG_CALIB_HUM              0x0F

#define CFG_TEMP_EN                0x40
#define CFG_DEFAULT                (CFG_TEMP_EN)
#define WHO_AM_I_EXPECTED          0xEB

#define STATUS_TEMP_RDY_MASK       0x02
#define STATUS_HUM_RDY_MASK        0x04

#define TEMP_SHIFT_BITS            8
#define TEMP_CHANNEL_INDEX         0
#define HUM_CHANNEL_INDEX          1
#define BYTE_MASK                  0xFF
#define SENSOR_DATA_BITS           16

#define CHECK_NULL_PTR(ptr) \
	do { \
		if (!(ptr)) \
			return -EINVAL; \
	} while (0)

#define CHECK_I2C_READ(val, label) \
	do { \
		if ((val) < 0) { \
			ret = -EIO; \
			goto label; \
		} \
	} while (0)

// Enum for sensor channel indices
enum sensor_channels {
	TEMP_CHANNEL = TEMP_CHANNEL_INDEX,
	HUM_CHANNEL = HUM_CHANNEL_INDEX
};

// Driver private data structure
struct env_combo_data {
	struct i2c_client *client;
	struct mutex lock;
	int temp_calib;
	int hum_calib;
};

// Defines IIO channel specifications for temperature and humidity
static const struct iio_chan_spec env_combo_channels[] = {
	{
		.type = IIO_TEMP,
		.indexed = 1,
		.channel = TEMP_CHANNEL,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = TEMP_CHANNEL,
		.scan_type = {
			.sign = 's',
			.realbits = SENSOR_DATA_BITS,
			.storagebits = SENSOR_DATA_BITS,
			.endianness = IIO_CPU,
		},
	},
	{
		.type = IIO_HUMIDITYRELATIVE,
		.indexed = 1,
		.channel = HUM_CHANNEL,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = HUM_CHANNEL,
		.scan_type = {
			.sign = 's',
			.realbits = SENSOR_DATA_BITS,
			.storagebits = SENSOR_DATA_BITS,
			.endianness = IIO_CPU,
		},
	},
};

/*
 * Reads raw data from the sensor for a specified IIO channel.
 */
static int env_combo_read_raw(
	struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan,
	int *val,
	int *val2,
	long mask)
{
	struct env_combo_data *data = iio_priv(indio_dev);
	struct i2c_client *client = data->client;
	int status = 0, raw_data = 0, ret = 0;
	int temp_msb = 0, temp_lsb = 0, hum_lsb = 0;

	(void)val2;
	CHECK_NULL_PTR(chan);
	CHECK_NULL_PTR(val);

	if (mask != IIO_CHAN_INFO_RAW)
		return -EINVAL;

	mutex_lock(&data->lock);

	// Read sensor status register to check readiness
	status = i2c_smbus_read_byte_data(client, REG_STATUS);
	CHECK_I2C_READ(status, out);

	switch (chan->type) {
	case IIO_TEMP:
		// Check if temperature data is ready
		if (!(status & STATUS_TEMP_RDY_MASK)) {
			ret = -EAGAIN;
			goto out;
		}

		// Read temperature MSB and LSB values
		temp_msb = i2c_smbus_read_byte_data(client, REG_TEMP_MSB);
		CHECK_I2C_READ(temp_msb, out);
		temp_lsb = i2c_smbus_read_byte_data(client, REG_TEMP_LSB);
		CHECK_I2C_READ(temp_lsb, out);

		// Combine MSB and masked LSB, apply calibration
		raw_data = ((temp_msb << TEMP_SHIFT_BITS) | (temp_lsb & BYTE_MASK)) + data->temp_calib;
		*val = raw_data;
		ret = IIO_VAL_INT;
		break;
	case IIO_HUMIDITYRELATIVE:
		// Check if humidity data is ready
		if (!(status & STATUS_HUM_RDY_MASK)) {
			ret = -EAGAIN;
			goto out;
		}

		// Read humidity output register
		hum_lsb = i2c_smbus_read_byte_data(client, REG_HUM_OUT);
		CHECK_I2C_READ(hum_lsb, out);

		// Add calibration offset
		raw_data = hum_lsb + data->hum_calib;
		*val = raw_data;
		ret = IIO_VAL_INT;
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&data->lock);
	return ret;
}

// Provides information functions for the IIO core to interact with the device
static const struct iio_info env_combo_info = {
	.read_raw = env_combo_read_raw,
};

/*
 * Probes and initializes the sensor device.
 */
static int env_combo_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct iio_dev *indio_dev;
	struct env_combo_data *data;
	int ret = 0, id_reg = 0, calib_msb = 0, calib_lsb = 0;

	CHECK_NULL_PTR(client);
	CHECK_NULL_PTR(id);

	dev_info(&client->dev, "env_combo: probe() called\n");

	// Verify the device identity
	id_reg = i2c_smbus_read_byte_data(client, REG_WHO_AM_I);
	if (id_reg < 0 || id_reg != WHO_AM_I_EXPECTED) {
		dev_err(&client->dev, "Device ID mismatch or read error\n");
		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;
	mutex_init(&data->lock);

	// Read and combine temperature calibration values
	calib_msb = i2c_smbus_read_byte_data(client, REG_CALIB_TEMP_MSB);
	CHECK_I2C_READ(calib_msb, fail);
	calib_lsb = i2c_smbus_read_byte_data(client, REG_CALIB_TEMP_LSB);
	CHECK_I2C_READ(calib_lsb, fail);
	data->temp_calib = (calib_msb << TEMP_SHIFT_BITS) | calib_lsb;

	// Read humidity calibration value
	calib_msb = i2c_smbus_read_byte_data(client, REG_CALIB_HUM);
	CHECK_I2C_READ(calib_msb, fail);
	data->hum_calib = calib_msb;

	// Configure the sensor with default settings
	ret = i2c_smbus_write_byte_data(client, REG_CFG, CFG_DEFAULT);
	if (ret < 0)
		return dev_err_probe(&client->dev, ret, "Failed to write config\n");

	// Initialize the IIO device structure
	indio_dev->dev.parent = &client->dev;
	indio_dev->name = ENV_COMBO_DRV_NAME;
	indio_dev->info = &env_combo_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = env_combo_channels;
	indio_dev->num_channels = ARRAY_SIZE(env_combo_channels);

	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret)
		dev_err(&client->dev, "Failed to register IIO device\n");

	return ret;

fail:
	return -EIO;
}

// I2C device ID table
static const struct i2c_device_id env_combo_id[] = {
	{ "env-combo", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, env_combo_id);

// OF device match table for device tree support
static const struct of_device_id env_combo_of_match[] = {
	{ .compatible = "env,combo" },
	{ }
};
MODULE_DEVICE_TABLE(of, env_combo_of_match);

// Defines the I2C driver structure and links probe function
static struct i2c_driver env_combo_driver = {
	.driver = {
		.name = ENV_COMBO_DRV_NAME,
		.of_match_table = of_match_ptr(env_combo_of_match),
	},
	.probe = env_combo_probe,
	.id_table = env_combo_id,
};

// Macro to register the driver module
module_i2c_driver(env_combo_driver);

MODULE_AUTHOR("Sami Natshe");
MODULE_DESCRIPTION("ENV-COMBO I2C Sensor Driver with IIO");
MODULE_LICENSE("GPL");
