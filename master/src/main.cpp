/*
* MASTER
*
* Source file for the master teensy for LJ STAND 2017
*/

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <t3spi.h>
#include <DebugController.h>
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
#include <LightGate.h>
#include <PixyI2C.h>
#include <GoalData.h>
#include <Sonar.h>
#include <Slave.h>
#include <Timer.h>

T3SPI spi;
DebugController debug;
MotorArray motors;
IMU imu;
LightGate lightGate;
PixyI2C pixy;
Sonar sonarFront;
Sonar sonarRight;
Sonar sonarBack;
Sonar sonarLeft;

SlaveLightSensor slaveLightSensor;
SlaveTSOP slaveTSOP;
SlaveData slaveData;

RobotPosition position;
RobotPosition previousPosition = RobotPosition::field;
GoalData goalData;

Timer pixyTimer = Timer(PIXY_UPDATE_TIME);
Timer ledTimer = Timer(1000000);

bool ledOn;

int facingDirection;

void setup() {
    // Onboard LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // Debug
    debug.init();

    // I2C
    Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, 400000);
    Wire.setDefaultTimeout(200000);

    debug.toggleOrange(true);

    // Serial
    Serial.begin(9600);
    Bluetooth::init();

    debug.toggleWhite(true);

    // SPI
    spi.begin_MASTER(MASTER_SCK, MASTER_MOSI, MASTER_MISO, MASTER_CS_TSOP, CS_ActiveLOW);
    spi.setCTAR(CTAR_0, 16, SPI_MODE0, LSB_FIRST, SPI_CLOCK_DIV16);

    slaveLightSensor.init();
    slaveTSOP.init();

    debug.toggleYellow(true);

    // IMU
    imu.init();
    imu.calibrate();

    debug.toggleBlue(true);

    // Light Gate
    lightGate.init();

    debug.toggleGreen(true);

    // Pixy
    pixy.init();

    // Sonars
    sonarFront.init(SONAR_FRONT_ADDRESS);
    sonarRight.init(SONAR_RIGHT_ADDRESS);
    sonarBack.init(SONAR_BACK_ADDRESS);
    sonarLeft.init(SONAR_LEFT_ADDRESS);

    debug.toggleAllLEDs(true);
    delay(500);
    debug.flashAllLEDs(3, 200);
}

int calculateRotationCorrection() {
    int correctionRotation;
    int rotation = (mod(imu.heading - facingDirection, 360) > 180 ? 360 : 0) - mod(imu.heading - facingDirection, 360);

    if (abs(rotation) < IMU_THRESHOLD) {
        correctionRotation = 0;
    } else if (abs(rotation) < CORRECTION_ROTATION_MINIMUM) {
        correctionRotation = (rotation > 0 ? CORRECTION_ROTATION_MINIMUM : -CORRECTION_ROTATION_MINIMUM);
    } else if (abs(rotation) < CORRECTION_ROTATION_MAXIMUM) {
        correctionRotation = rotation;
    } else {
        correctionRotation = (rotation > 0 ? CORRECTION_ROTATION_MAXIMUM : -CORRECTION_ROTATION_MAXIMUM);
    }

    return correctionRotation * CORRECTION_ROTATION_MULTIPLIER;
}

MoveData calculateLineAvoid(MoveData movement) {
    if (position != RobotPosition::field && slaveData.orbitAngle != TSOP_NO_BALL && AVOID_LINE) {
        int orbitAngle = slaveData.orbitAngle;

        // Front
        if (position == RobotPosition::smallOnFrontLine) {
            if (270 - LS_MOVEMENT_ANGLE_BUFFER < orbitAngle || orbitAngle < 90 + LS_MOVEMENT_ANGLE_BUFFER) {
                movement.speed = 0;
            }
        }

        if (position == RobotPosition::bigOnFrontLine) {
            if (270 - LS_MOVEMENT_ANGLE_BUFFER < orbitAngle || orbitAngle < 90 + LS_MOVEMENT_ANGLE_BUFFER) {
                if (orbitAngle < 180) {
                    movement.angle = 120;
                } else {
                    movement.angle = 240;
                }
            }
        }

        if (position == RobotPosition::overFrontLine) {
            movement.angle = 180;
        }

        // Right
        if (position == RobotPosition::smallOnRightLine) {
            if (mod(0 - LS_MOVEMENT_ANGLE_BUFFER, 360) < orbitAngle && orbitAngle < 180 + LS_MOVEMENT_ANGLE_BUFFER) {
                movement.speed = 0;
            }
        }

        if (position == RobotPosition::bigOnRightLine) {
            if (mod(0 - LS_MOVEMENT_ANGLE_BUFFER, 360) < orbitAngle && orbitAngle < 180 + LS_MOVEMENT_ANGLE_BUFFER) {
                if (orbitAngle > 90) {
                    movement.angle = 210;
                } else {
                    movement.angle = 330;
                }
            }
        }

        if (position == RobotPosition::overRightLine) {
            movement.angle = 270;
        }

        // Back
        if (position == RobotPosition::smallOnBackLine) {
            if (90 - LS_MOVEMENT_ANGLE_BUFFER < orbitAngle && orbitAngle < 270 + LS_MOVEMENT_ANGLE_BUFFER) {
                movement.speed = 0;
            }
        }

        if (position == RobotPosition::bigOnBackLine) {
            if (90 - LS_MOVEMENT_ANGLE_BUFFER < orbitAngle && orbitAngle < 270 + LS_MOVEMENT_ANGLE_BUFFER) {
                if (orbitAngle > 180) {
                    movement.angle = 300;
                } else {
                    movement.angle = 60;
                }
            }
        }

        if (position == RobotPosition::overBackLine) {
            movement.angle = 0;
        }

        // Left
        if (position == RobotPosition::smallOnLeftLine) {
            if (180 - LS_MOVEMENT_ANGLE_BUFFER < orbitAngle || orbitAngle < 0 + LS_MOVEMENT_ANGLE_BUFFER) {
                movement.speed = 0;
            }
        }

        if (position == RobotPosition::bigOnLeftLine) {
            if (180 - LS_MOVEMENT_ANGLE_BUFFER < orbitAngle || orbitAngle < 0 + LS_MOVEMENT_ANGLE_BUFFER) {
                if (orbitAngle > 270) {
                    movement.angle = 30;
                } else {
                    movement.angle = 150;
                }
            }
        }

        if (position == RobotPosition::overLeftLine) {
            movement.angle = 90;
        }
    }

    return movement;
}

MoveData calculateMovement() {
    MoveData movement;
    movement.angle = slaveData.orbitAngle != TSOP_NO_BALL ? slaveData.orbitAngle : 0;
    movement.speed = slaveData.orbitAngle != TSOP_NO_BALL ? slaveData.orbitSpeed : 0;

    if (goalData.status != GoalStatus::invisible && slaveData.hasBallTSOP) {
        // We have the ball and we can see the goal
        facingDirection = mod(imu.heading + (int)((double)goalData.angle * 1.2), 360);

        movement.angle = mod(-((int)min(((double)goalData.angle / 75.0) * 150, 90)), 360);
        // angle = 0;
        //
        // // angle = goalData.angle > 0 ? 270 : 90;
        // rotation += (int)(((double)goalData.angle / 75.0) * GOAL_ROTATION_MULTIPLIER);
        // speed = 255;
        //
        // // Serial.println(String(goalData.angle) + ", " + String(angle) + ", " + String(rotation));
    } else {
        facingDirection = 0;
    }

    // NOTE IF THE LINE AVOIDANCE DOESNT WORK ITS BECAUSE ITS NOT ENABLED!!!
    // movement = calculateLineAvoid(movement);

    movement.rotation = calculateRotationCorrection();

    return movement;
}

void updatePixy() {
    if (pixyTimer.timeHasPassed()) {
        uint16_t blocks = pixy.getBlocks();

        if (blocks > 1) {
            goalData.status = GoalStatus::blocked;
        } else if (blocks > 0) {
            goalData.status = GoalStatus::visible;
        } else {
            goalData.status = GoalStatus::invisible;
        }

        if (goalData.status != GoalStatus::invisible) {
            debug.toggleRed(true);
            double height = pixy.blocks[0].height;
            goalData.distance = (int)((height / (double)(GOAL_HEIGHT_SHORT - GOAL_HEIGHT_LONG)) * GOAL_DISTANCE_MULTIPLIER);

            double middleGoalPoint = (double)pixy.blocks[0].x;
            double goalDiffMiddleFOV = middleGoalPoint - 160;

            goalData.angle = (int)(((double)goalDiffMiddleFOV / 160.0) * 75);
        } else {
            debug.toggleRed(false);
            goalData.distance = 0;
            goalData.angle = 0;
        }
    }
}

void loop() {
    // Slaves
    LinePosition linePosition = slaveLightSensor.getLinePosition();
    int orbitAngle = slaveTSOP.getOrbitAngle();
    int orbitSpeed = slaveTSOP.getOrbitSpeed();
    bool hasBallTSOP = slaveTSOP.getHasBallTSOP();
    slaveData = SlaveData(linePosition, orbitAngle, orbitSpeed, hasBallTSOP);

    // Sensors
    imu.update();
    updatePixy();

    // Debug
    #if DEBUG_APP_IMU
    debug.appSendIMU(imu.heading);
    #endif

    #if DEBUG_APP_LIGHTSENSORS
    debug.appSendLightSensors(slaveLightSensor.getFirst16Bit(), slaveLightSensor.getSecond16Bit());
    #endif

    // Movement
    position = calculateRobotPosition(slaveData.linePosition, previousPosition);
    MoveData movement = calculateMovement();

    if (previousPosition != position) {
        previousPosition = position;
    }

    debug.toggleGreen(lightGate.hasBall());

    if (ledTimer.timeHasPassed()) {
        debug.toggleBlue(ledOn);
        ledOn = !ledOn;
    }

    motors.move(movement);
}
