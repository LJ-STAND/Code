/*
 * MASTER

 * Source file for the master teensy for LJ STAND
 */

#include <Arduino.h>
#include <t3spi.h>
#include <i2c_t3.h>
#include <Config.h>
#include <SlaveData.h>
#include <Bluetooth.h>
#include <MotorArray.h>
#include <IMU.h>
#include <LinePosition.h>
#include <RobotPosition.h>
#include <CalculateRobotPosition.h>
#include <MoveData.h>
#include <Pins.h>

T3SPI spi;

volatile uint16_t dataInTsop[DATA_LENGTH_TSOP];
volatile uint16_t dataOutTsop[DATA_LENGTH_TSOP];
volatile uint16_t dataInLight[DATA_LENGTH_LIGHT];
volatile uint16_t dataOutLight[DATA_LENGTH_LIGHT];

SlaveData slaveData;
RobotPosition position;

MotorArray motors;
IMU imu;

void setup() {
    // I2C
    Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, 400000);
    Wire.setDefaultTimeout(200000);

    // Serial
    Serial.begin(9600);
    Bluetooth::init();

    // SPI
    spi.begin_MASTER(MASTER_SCK, MASTER_MOSI, MASTER_MISO, CS0, CS_ActiveLOW);
    spi.setCTAR(CTAR_0, 16, SPI_MODE0, LSB_FIRST, SPI_CLOCK_DIV16);

    // Onboard LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // IMU
    imu.init();
    imu.calibrate();
}

int calculateRotationCorrection() {
    double relativeHeading = 180.0 - (doubleMod((imu.heading + imu.facingDirection) + 180, 360));
    int correctionRotation;

	if (abs(relativeHeading > IMU_THRESHOLD) {
		correctionRotation = (int) relativeHeading;

		if (correctionRotation < 0 && correctionRotation > -CORRECTION_ROTATION_MINIMUM) {
			correctionRotation = -CORRECTION_ROTATION_MINIMUM;
		} else if (correctionRotation > 0 && correctionRotation < CORRECTION_ROTATION_MINIMUM) {
			correctionRotation = CORRECTION_ROTATION_MINIMUM;
		} else if (correctionRotation > 0 && correctionRotation > CORRECTION_ROTATION_MAXIMUM) {
			correctionRotation = CORRECTION_ROTATION_MAXIMUM;
		} else if (correctionRotation < 0 && correctionRotation < -CORRECTION_ROTATION_MAXIMUM) {
			correctionRotation = -CORRECTION_ROTATION_MAXIMUM;
		}
	} else {
		correctionRotation = 0;
	}

	return correctionRotation;
}

MoveData calculateMovement() {
    // TODO
}

void getSlaveData() {
    spi.txrx16(dataOutTsop, dataInTsop, DATA_LENGTH_TSOP, false, MASTER_CS_TSOP);
    spi.txrx16(dataOutLight, dataInLight, DATA_LENGTH_LIGHT, false, MASTER_CS_LIGHT);

    slaveData = SlaveData(static_cast<LinePosition>(dataInLight[0]), (int) dataInTsop[0], (int) dataInTsop[1]);
}

void loop() {
    getSlaveData();

    position = calculateRobotPosition(slaveData.linePosition, position);

    motors.move(calculateMovement());
}
