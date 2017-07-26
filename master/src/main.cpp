/*
* MASTER
*
* Source file for the master teensy for LJ STAND 2017
*/

#include <Arduino.h>
#include <t3spi.h>
#include <DebugController.h>
#include <i2c_t3.h>
#include <Config.h>
#include <Bluetooth.h>
#include <MotorArray.h>
#include <IMU.h>
#include <MoveData.h>
#include <Pins.h>
#include <LightGate.h>
#include <PixyI2C.h>
#include <GoalData.h>
#include <Sonar.h>
#include <Slave.h>
#include <Timer.h>
#include <XBee.h>
#include <PlayMode.h>
#include <BallData.h>
#include <LineData.h>
#include <Common.h>
#include <MovingAverage.h>

XBee xbee;
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

LineData lineData(0, 0, true);
BallData ballData;
MoveData moveData;
GoalData goalData;

PlayMode playMode = PlayMode::undecided;
bool playModeSwitchComplete = true;
bool attackingBackwards = false;

MovingAverage switchingStrengthAverage(25);
MovingAverage otherSwitchingStrengthAverage(25);

Timer pixyTimer = Timer(PIXY_UPDATE_TIME);
Timer ledTimer = Timer(LED_BLINK_TIME_MASTER);
Timer lastSeenGoalTimer = Timer(LAST_SEEN_GOAL_TIME);
Timer xbeeTimer = Timer(XBEE_UPDATE_TIME);
Timer playModeSwitchTimer = Timer(PLAYMODE_SWITCH_TIME);

double compassPreviousAngle = 0;
long compassPreviousTime;
double compassDiff = 0;

double facingDirection = 0;
bool facingGoal = false;

bool ledOn;

void setup() {
    // Onboard LED
    pinMode(13, OUTPUT);
    digitalWrite(13, HIGH);

    // Debug
    debug.init();

    // I2C
    Wire.begin(I2C_MASTER, 0x00, I2C_PINS_18_19, I2C_PULLUP_EXT, 100000);
    Wire.setDefaultTimeout(200000);

    debug.toggleOrange(true);

    // Serial
    Serial.begin(57600);
    Bluetooth::init();

    // XBee
    xbee.init();

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
    compassPreviousTime = micros();

    debug.toggleBlue(true);

    // Light Gate
    lightGate.init();

    debug.toggleGreen(true);

    // Pixy
    pixy.init();

    debug.toggleAllLEDs(true);
    delay(100);
    debug.toggleAllLEDs(false);
}

PlayMode currentPlayMode() {
    return playMode == PlayMode::undecided ? PLAYMODE_DEFAULT : playMode;
}

double defaultDirection() {
    return currentPlayMode() == PlayMode::attack && !attackingBackwards ? 0 : 180;
}

void calculateRotationCorrection() {
    double multiplierD = facingGoal ? CORRECTION_ROTATION_MULTIPLIER_D_GOAL : CORRECTION_ROTATION_MULTIPLIER_D;
    double multiplierP = facingGoal ? CORRECTION_ROTATION_MULTIPLIER_P_GOAL : CORRECTION_ROTATION_MULTIPLIER_P;

    int correctionRotation;
    int rotation = ((mod(imu.heading - facingDirection, 360) > 180 ? 360 : 0) - mod(imu.heading - facingDirection, 360)) * multiplierP + compassDiff * multiplierD;

    if (abs(rotation) < CORRECTION_ROTATION_MINIMUM) {
        correctionRotation = 0;
    } else if (abs(rotation) < CORRECTION_ROTATION_MAXIMUM) {
        correctionRotation = rotation;
    } else {
        correctionRotation = (rotation > 0 ? CORRECTION_ROTATION_MAXIMUM : -CORRECTION_ROTATION_MAXIMUM);
    }

    moveData.rotation = correctionRotation;
}

void calculateLineAvoid() {
    if (!lineData.onField) {
        if (lineData.size > LINE_BIG_SIZE) {
            moveData.angle = mod(lineData.angle + 180 - imu.heading, 360);
            moveData.speed = lineData.size == 3 ? OVER_LINE_SPEED : min(lineData.size / 2.0 * LINE_SPEED * 5, LINE_SPEED);
        } else if (lineData.size > LINE_SMALL_SIZE) {
            if (mod(lineData.angle, 90) > LS_MOVEMENT_CORNER_ANGLE_THRESHOLD && mod(lineData.angle, 90) < 90 - LS_MOVEMENT_CORNER_ANGLE_THRESHOLD) {
                if (angleIsInside(doubleMod(lineData.angle - 135 - LS_MOVEMENT_ANGLE_BUFFER_CORNER, 360), doubleMod(lineData.angle + 135 + LS_MOVEMENT_ANGLE_BUFFER_CORNER, 360), mod(moveData.angle + imu.heading, 360))) {
                    moveData.angle = mod(lineData.angle + 180 - imu.heading, 360);
                    moveData.speed = lineData.size / 2.0 * LINE_SPEED;
                }
            } else {
                if (angleIsInside(doubleMod(lineData.angle - 90 - LS_MOVEMENT_ANGLE_BUFFER, 360), doubleMod(lineData.angle + 90 + LS_MOVEMENT_ANGLE_BUFFER, 360), mod(moveData.angle + imu.heading, 360))) {
                    moveData.angle = 0;
                    moveData.speed = 0;
                }
            }
        }
    }
}

void centre() {
    if (goalData.status != GoalStatus::invisible && smallestAngleBetween(imu.heading, 0) < 10) {
        double relativeDistance = abs(CENTRE_GOAL_DISTANCE - goalData.distance) > CENTRE_GOAL_DISTANCE_BUFFER ? CENTRE_GOAL_DISTANCE - goalData.distance : 0;
        double distanceMovement = relativeDistance > 0 ? min(relativeDistance * CENTRE_DISTANCE_MULTIPLIER, CENTRE_DISTANCE_MAX_SPEED) : max(relativeDistance * CENTRE_DISTANCE_MULTIPLIER, -CENTRE_DISTANCE_MAX_SPEED);

        double sidewaysMovement = 0;

        if (abs(goalData.angle) > CENTRE_GOAL_ANGLE_BUFFER) {
            if (goalData.angle > 0) {
                sidewaysMovement = min(goalData.angle / (PIXY_HORIZONTAL_FOV / 2.0) * CENTRE_SIDEWAYS_MULTIPLIER, CENTRE_SIDEWAYS_MAX_SPEED);
            } else {
                sidewaysMovement = max(goalData.angle / (PIXY_HORIZONTAL_FOV / 2.0) * CENTRE_SIDEWAYS_MULTIPLIER, -CENTRE_SIDEWAYS_MAX_SPEED);
            }
        }

        moveData.angle = mod(radiansToDegrees(atan2(sidewaysMovement, distanceMovement)), 360);
        moveData.speed = sqrt(distanceMovement * distanceMovement + sidewaysMovement * sidewaysMovement);
    } else {
        moveData.angle = 0;
        moveData.speed = 0;
    }
}

void calculateOrbit() {
    if (angleIsInside(360 - ORBIT_SMALL_ANGLE, ORBIT_SMALL_ANGLE, ballData.angle)) {
        moveData.angle = (int)round(ballData.angle < 180 ? (ballData.angle * ORBIT_BALL_FORWARD_ANGLE_TIGHTENER) : (360 - (360 - ballData.angle) * ORBIT_BALL_FORWARD_ANGLE_TIGHTENER));
    } else if (angleIsInside(360 - ORBIT_BIG_ANGLE, ORBIT_BIG_ANGLE, ballData.angle)) {
        if (ballData.angle < 180) {
            double nearFactor = (double)(ballData.angle - ORBIT_SMALL_ANGLE) / (double)(ORBIT_BIG_ANGLE - ORBIT_SMALL_ANGLE);
            moveData.angle = (int)round(90 * nearFactor + ballData.angle * ORBIT_BALL_FORWARD_ANGLE_TIGHTENER + ballData.angle * (1 - ORBIT_BALL_FORWARD_ANGLE_TIGHTENER) * nearFactor);
        } else {
            double nearFactor = (double)(360 - ballData.angle - ORBIT_SMALL_ANGLE) / (double)(ORBIT_BIG_ANGLE - ORBIT_SMALL_ANGLE);
            moveData.angle = (int)round(360 - (90 * nearFactor + (360 - ballData.angle) * ORBIT_BALL_FORWARD_ANGLE_TIGHTENER + (360 - ballData.angle) * (1 - ORBIT_BALL_FORWARD_ANGLE_TIGHTENER) * nearFactor));
        }
    } else {
        if (ballData.strength > ORBIT_SHORT_STRENGTH) {
            moveData.angle =  ballData.angle + (ballData.angle < 180 ? 90 : -90);
        } else if (ballData.strength > ORBIT_BIG_STRENGTH) {
            double strengthFactor = (double)(ballData.strength - ORBIT_BIG_STRENGTH) / (double)(ORBIT_SHORT_STRENGTH - ORBIT_BIG_STRENGTH);
            double angleFactor = strengthFactor * 90;
            moveData.angle = ballData.angle + (ballData.angle < 180 ? angleFactor : -angleFactor);
        } else {
            moveData.angle = ballData.angle;
        }
    }

    moveData.speed = ORBIT_SPEED;
}

void calculateDefense() {
    ballData.angle = mod(ballData.angle + 180, 360);

    if (goalData.status != GoalStatus::invisible) {
        double relativeDistance = abs(goalData.distance - DEFEND_GOAL_DISTANCE) > DEFEND_GOAL_DISTANCE_BUFFER ? goalData.distance - DEFEND_GOAL_DISTANCE : 0;
        double distanceMovement = relativeDistance > 0 ? min(relativeDistance * DEFEND_DISTANCE_MULTIPLIER, DEFEND_DISTANCE_MAX_SPEED) : max(relativeDistance * DEFEND_DISTANCE_MULTIPLIER, -DEFEND_DISTANCE_MAX_SPEED);

        double sidewaysMovement = 0;

        if (ballData.visible) {
            if (angleIsInside(270, 90, ballData.angle)) {
                if (angleIsInside(360 - DEFEND_SMALL_ANGLE, DEFEND_SMALL_ANGLE, ballData.angle) && ballData.strength > DEFEND_CHARGE_STRENGTH) {
                    sidewaysMovement = 0;
                } else if (angleIsInside(360 - DEFEND_SMALL_ANGLE, DEFEND_SMALL_ANGLE, ballData.angle)) {
                    sidewaysMovement = 0;
                } else if (ballData.angle < 180) {
                    sidewaysMovement = min(ballData.angle / 90.0 * DEFEND_SIDEWAYS_MULTIPLIER, DEFEND_SIDEWAYS_MAX_SPEED);
                } else {
                    sidewaysMovement = max(-(360 - ballData.angle) / 90.0 * DEFEND_SIDEWAYS_MULTIPLIER, -DEFEND_SIDEWAYS_MAX_SPEED);
                }
            } else {
                calculateOrbit();
                moveData.angle = mod(moveData.angle + 180, 360);
                return;
            }
        } else if (abs(goalData.angle) > CENTRE_GOAL_ANGLE_BUFFER) {
            if (goalData.angle > 0) {
                sidewaysMovement = max(-goalData.angle / (PIXY_HORIZONTAL_FOV / 2.0) * CENTRE_SIDEWAYS_MULTIPLIER, -CENTRE_SIDEWAYS_MAX_SPEED);
            } else {
                sidewaysMovement = min(-goalData.angle / (PIXY_HORIZONTAL_FOV / 2.0) * CENTRE_SIDEWAYS_MULTIPLIER, CENTRE_SIDEWAYS_MAX_SPEED);
            }
        }

        moveData.angle = mod(radiansToDegrees(atan2(sidewaysMovement, distanceMovement)), 360);
        moveData.speed = sqrt(distanceMovement * distanceMovement + sidewaysMovement * sidewaysMovement);
    } else if (smallestAngleBetween(imu.heading, 180) < 50 && ballData.visible) {
        calculateOrbit();
    } else {
        moveData.speed = 0;
        moveData.angle = 180;
    }

    moveData.angle = mod(moveData.angle + 180, 360);
}

void calculateGoalTracking() {
    if (goalData.status != GoalStatus::invisible && !attackingBackwards) {
        int goalAngle = mod(imu.heading + goalData.angle + 180, 360) - 180;

        if (currentPlayMode() == PlayMode::defend) {
            if (ballData.visible) {
                facingDirection = goalAngle;
                facingGoal = true;
            } else {
                facingDirection = 180;
                facingGoal = false;
            }
        } else {
            if (FACE_GOAL) {
                if (!ballData.visible) {
                    facingDirection = 0;
                    facingGoal = false;
                } else if (ballData.strength > FACE_GOAL_SHORT_STRENGTH || ALWAYS_FACE_GOAL) {
                    facingDirection = goalAngle;
                    facingGoal = true;
                } else if (ballData.strength > FACE_GOAL_BIG_STRENGTH) {
                    facingDirection = ((double)(ballData.strength - FACE_GOAL_BIG_STRENGTH) / (double)(FACE_GOAL_SHORT_STRENGTH - FACE_GOAL_BIG_STRENGTH)) * goalAngle;
                    facingGoal = true;
                } else {
                    facingDirection = 0;
                    facingGoal = false;
                }
            } else {
                facingDirection = 0;
                facingGoal = false;
            }
        }

        facingDirection = mod(facingDirection, 360);
    } else {
        facingDirection = defaultDirection();
        facingGoal = false;
    }
}

void calculateMovement() {
    if (currentPlayMode() == PlayMode::attack) {
        if (attackingBackwards) {
            ballData.angle = mod(ballData.angle + 180, 360);

            calculateOrbit();
            moveData.angle = mod(moveData.angle + 180, 360);

            if (!ballData.visible) {
                attackingBackwards = false;
            } else {
                if (goalData.status != GoalStatus::invisible) {
                    if (ballData.strength < ATTACK_BACKWARDS_MAX_STRENGTH && goalData.distance < DEFEND_LEFT_GOAL_DISTANCE) {
                        attackingBackwards = false;
                    }
                } else {
                    if (ballData.strength < ATTACK_BACKWARDS_MAX_STRENGTH) {
                        attackingBackwards = false;
                    }
                }
            }
        } else {
            if (ballData.visible) {
                calculateOrbit();
            } else {
                centre();
            }
        }
    } else {
        calculateDefense();
    }

    #if AVOID_LINE
        calculateLineAvoid();
    #endif

    calculateRotationCorrection();
}

void updatePixy() {
    if (pixyTimer.timeHasPassed()) {
        uint16_t blocks = pixy.getBlocks();

        Block goalBlock = Block();
        int biggestArea = 0;
        int foundBlocks = 0;

        if (blocks < 3 || currentPlayMode() == PlayMode::attack) {
            for (int i = 0; i < blocks; i++) {
                int blockArea = pixy.blocks[i].height * pixy.blocks[i].width;

                if (blockArea > GOAL_MIN_AREA && pixy.blocks[i].signature == (currentPlayMode() == PlayMode::attack && !attackingBackwards ? COLOUR_SIG_ATTACK : COLOUR_SIG_DEFEND) && smallestAngleBetween(imu.heading, defaultDirection()) < 90) {
                    foundBlocks += 1;

                    if (blockArea > biggestArea) {
                        biggestArea = blockArea;
                        goalBlock = pixy.blocks[i];
                    }
                }
            }
        }

        if (foundBlocks == 0) {
            if (lastSeenGoalTimer.timeHasPassedNoUpdate()) {
                goalData.status = GoalStatus::invisible;
                goalData.angle = 0;
                goalData.distance = 0;
            }
        } else {
            if (foundBlocks > 1) {
                goalData.status = GoalStatus::blocked;
            } else {
                goalData.status = GoalStatus::visible;
            }

            lastSeenGoalTimer.update();

            #if DEBUG_APP
                debug.appSendPixy(goalBlock.x, goalBlock.y, goalBlock.width, goalBlock.height);
            #endif

            goalData.distance = (double)goalBlock.height / (double)PIXY_FRAME_HEIGHT * GOAL_DISTANCE_MULTIPLIER;
            goalData.angle = (int)((double)(goalBlock.x - (PIXY_FRAME_WIDTH / 2.0)) / (PIXY_FRAME_WIDTH / 2.0) * 37.5);
        }

        debug.toggleRed(goalData.status != GoalStatus::invisible);
    }
}

void updateCompass() {
    imu.update();
    long currentTime = micros();
    compassDiff = (imu.heading - compassPreviousAngle) / (currentTime - compassPreviousTime);

    if (compassDiff > 180) {
        compassDiff -= 360;
    } else if (compassDiff < -180) {
        compassDiff += 360;
    }

    compassPreviousAngle = imu.heading;
    compassPreviousTime = currentTime;
}

void updateLine(double angle, double size) {
    bool noLine = angle == NO_LINE_ANGLE || size == 3;

    angle = noLine ? 0 : doubleMod(angle + imu.heading, 360);

    if (lineData.onField) {
        if (!noLine) {
            lineData.angle = angle;
            lineData.size = size;

            lineData.onField = false;
        }
    } else {
        if (lineData.size == 3) {
            if (!noLine) {
                lineData.angle = doubleMod(angle + 180, 360);
                lineData.size = 2 - size;
            }
        } else {
            if (noLine) {
                if (lineData.size <= 1) {
                    lineData.onField = true;
                    lineData.size = 0;
                    lineData.angle = 0;
                } else {
                    lineData.size = 3;
                }
            } else {
                if (smallestAngleBetween(lineData.angle, angle) <= 90) {
                    lineData.angle = angle;
                    lineData.size = size;
                } else {
                    lineData.angle = doubleMod(angle + 180, 360);
                    lineData.size = 2 - size;
                }
            }
        }
    }

    #if DEBUG_LINE
        Serial.print(lineData.onField ? "On" : "Off");
        Serial.println(", Robot: " + String(lineData.angle) + ", " + String(lineData.size) + ", Line: " + String(angle) + ", " + String(size));
    #endif
}

void updatePlayMode() {
    PlayMode previousPlayMode = playMode;

    if (playMode == PlayMode::undecided) {
        if (xbee.otherPlayMode == PlayMode::undecided) {
            if (xbee.otherBallAngle == TSOP_NO_BALL && ballData.angle != TSOP_NO_BALL) {
                playMode = PlayMode::attack;
            } else if (xbee.otherBallAngle != TSOP_NO_BALL && ballData.angle == TSOP_NO_BALL) {
                playMode = PlayMode::defend;
            } else if (xbee.otherBallAngle != TSOP_NO_BALL && ballData.angle != TSOP_NO_BALL) {
                if (angleIsInside(270, 90, ballData.angle) && angleIsInside(270, 90, xbee.otherBallAngle)) {
                    playMode = ballData.strength > xbee.otherBallStrength ? PlayMode::attack : PlayMode::defend;
                } else if (angleIsInside(270, 90, ballData.angle) && angleIsInside(90, 270, xbee.otherBallAngle)) {
                    playMode = PlayMode::defend;
                } else if (angleIsInside(90, 270, ballData.angle) && angleIsInside(270, 90, xbee.otherBallAngle)) {
                    playMode = PlayMode::attack;
                } else {
                    playMode = ballData.strength > xbee.otherBallStrength ? PlayMode::defend : PlayMode::attack;
                }
            }
        } else {
            playMode = xbee.otherPlayMode == PlayMode::attack ? PlayMode::defend : PlayMode::attack;
        }

        attackingBackwards = false;
    } else if (xbee.otherPlayMode != PlayMode::undecided) {
        if (playModeSwitchComplete && playModeSwitchTimer.timeHasPassedNoUpdate()) {
            if (xbee.otherPlayMode == playMode) {
                playMode = xbee.otherPlayMode == PlayMode::attack ? PlayMode::defend : PlayMode::attack;
                attackingBackwards = false;
                playModeSwitchTimer.update();
            } else if (playMode == PlayMode::defend) {
                if (xbee.otherBallAngle == TSOP_NO_BALL && ballData.angle != TSOP_NO_BALL) {
                    playMode = PlayMode::attack;
                    attackingBackwards = true;
                    playModeSwitchComplete = false;
                    playModeSwitchTimer.update();
                } else if (xbee.otherBallAngle != TSOP_NO_BALL && ballData.angle != TSOP_NO_BALL) {
                    if (angleIsInside(360 - PLAYMODE_SWITCH_DEFENDER_ANGLE, PLAYMODE_SWITCH_DEFENDER_ANGLE, mod(ballData.angle + 180, 360)) && switchingStrengthAverage.average() > PLAYMODE_SWITCH_DEFENDER_STRENGTH && otherSwitchingStrengthAverage.average() < PLAYMODE_SWITCH_ATTACKER_STRENGTH) {
                        playMode = PlayMode::attack;
                        attackingBackwards = true;
                        playModeSwitchComplete = false;
                        playModeSwitchTimer.update();
                    }
                }
            }
        } else if (xbee.otherPlayMode != playMode) {
            playModeSwitchComplete = true;
        }
    }

    if (playMode != previousPlayMode) {
        xbee.update(ballData.angle, ballData.strength, imu.heading, playMode, true);
    }
}

void updateXBee() {
    if (xbeeTimer.timeHasPassed()) {
        xbee.update(ballData.angle, ballData.strength, imu.heading, playMode);

        debug.toggleGreen(xbee.isConnected);

        if (xbee.isConnected) {
            switchingStrengthAverage.update(ballData.strength);
            otherSwitchingStrengthAverage.update(xbee.otherBallStrength);

            updatePlayMode();
        } else {
            playMode = PlayMode::undecided;
            attackingBackwards = false;
        }
    }
}

void appDebug() {
    #if DEBUG_APP
        //IMU
        debug.appSendIMU(imu.heading);

        //Light Sensors
        uint16_t first16Bit = slaveLightSensor.getFirst16Bit();
        uint16_t second16Bit = slaveLightSensor.getSecond16Bit();

        debug.appSendLightSensors(first16Bit, second16Bit);

        debug.appSendRobotPosition(lineData.angle, lineData.size);

        //TSOPS
        debug.appSendTSOPs(ballData.angle);
        debug.appSendOrbitAngle(moveData.angle);
    #endif
}

void loop() {
    ballData = slaveTSOP.getBallData();

    debug.toggleOrange(ballData.visible);

    updateLine(slaveLightSensor.getLineAngle(), slaveLightSensor.getLineSize());

    updateCompass();

    #if PIXY_ENABLED
        updatePixy();
        calculateGoalTracking();
    #endif

    #if XBEE_ENABLED
        updateXBee();
    #endif

    calculateMovement();

    motors.move(moveData);

    if (ledTimer.timeHasPassed()) {
        digitalWrite(LED_BUILTIN, ledOn);
        ledOn = !ledOn;
    }

    debug.toggleBlue(currentPlayMode() == PlayMode::attack);
    debug.toggleYellow(currentPlayMode() == PlayMode::defend);
    debug.toggleWhite(playMode == PlayMode::undecided);

    #if DEBUG_APP
        appDebug();
    #endif
}
