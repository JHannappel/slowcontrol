#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "measurement.h"
#include "parasitic_temperature.h"
#include "slowcontrolDaemon.h"
#include <Options.h>
#include <dirent.h>
#include <cmath>
#include <iostream>
#include <string.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <bmp280.h>
class bmp280pressure: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>,
	        public slowcontrol::defaultReaderInterface,
	        public slowcontrol::unitInterface {
  protected:
	static int fd; // ugly hack needed by BOSCH APIs lack of private data ptr
	slowcontrol::parasitic_temperature *lTemperature;
	struct bmp280_dev bmp;

	static int8_t readI2C(uint8_t /*dev_id*/, uint8_t reg_addr,
	                      uint8_t *data, uint16_t /*len*/) {
		auto bytesRead = i2c_smbus_read_block_data(fd, reg_addr, data);
		if (bytesRead < 0) {
			return -1;
		}
		return BMP280_OK;
	}
	static int8_t writeI2C(uint8_t /*dev_id*/, uint8_t reg_addr,
	                       uint8_t *data, uint16_t len) {
		return i2c_smbus_write_block_data(fd, reg_addr, len, data);
	}
	static void delay(uint32_t period) {
		std::this_thread::sleep_for(std::chrono::milliseconds(period));
	}
  public:
	bmp280pressure(const char *aPath, const char *aNameBase):
		boundCheckerInterface(40, 60, 0.5),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		unitInterface(lConfigValues, " %") {
		lClassName.fSetFromString(__func__);
		fd = open(aPath, O_RDWR);
		ioctl(fd, I2C_SLAVE, 0x76);
		/* Sensor interface over I2C with primary slave address  */
		bmp.dev_id = BMP280_I2C_ADDR_PRIM;
		bmp.intf = BMP280_I2C_INTF;
		bmp.read = readI2C;
		bmp.write = writeI2C;
		bmp.delay_ms = delay;
		if (bmp280_init(&bmp) != BMP280_OK) {
			throw slowcontrol::exception("can't init bmp280", slowcontrol::exception::level::kStop);
		}
		struct bmp280_config cfg;
		bmp280_get_config(&cfg, &bmp);
		bmp280_set_config(&cfg, &bmp);
		bmp280_set_power_mode(BMP280_NORMAL_MODE, &bmp);
		{
			std::string temperatureName(aNameBase);
			temperatureName += "_temperature";
			lTemperature = new slowcontrol::parasitic_temperature(temperatureName.c_str());
		}
		{
			std::string pressureName(aNameBase);
			pressureName += "_pressure";
			fInitializeUid(pressureName);
		}
		fConfigure();
	};
	bool fReadCurrentValue() override {

		struct bmp280_uncomp_data ucomp_data;
		bmp280_get_uncomp_data(&ucomp_data, &bmp);
		auto valueHasChanged = lTemperature->fStore(bmp280_comp_temp_double(ucomp_data.uncomp_temp, &bmp));
		valueHasChanged |= fStore(bmp280_comp_pres_double(ucomp_data.uncomp_press, &bmp));

		return valueHasChanged;
	};
};

int bmp280pressure::fd = 0;

int main(int argc, const char *argv[]) {
	options::single<const char*> deviceName('d', "device", "name of the i2c device", "/dev/i2c-1");
	options::single<const char*> measurementName('n', "name", "name base of the measurement");
	options::parser parser("slowcontrol program for reading bmp280 pressure sensors");
	parser.fParse(argc, argv);



	auto daemon = new slowcontrol::daemon("bmp280d");

	new bmp280pressure(deviceName, measurementName);

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
