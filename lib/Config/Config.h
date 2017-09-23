/*
 * Header file to define configurable values within the whole project
 *(e.g. ORBIT_SPEED)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <PlayMode.h>

// SPI

#define SPI_DELAY 10

// Light Sensors

#define LS_NUM 24

#define LS_CALIBRATION_COUNT 10
#define LS_CALIBRATION_BUFFER 35

#define NO_LINE_ANGLE 400
#define NO_LINE_SIZE 3

#define LINE_CORNER_ANGLE_THRESHOLD 30
#define LINE_ANGLE_BUFFER -10
#define LINE_ANGLE_BUFFER_CORNER -10
#define LS_MOVEMENT_SMALL_MULTIPLIER 0.5

#define OVER_LINE_SPEED 255
#define LINE_SPEED 255

#define LINE_SMALL_SIZE 0.4
#define LINE_BIG_SIZE 0.7

#define AVOID_LINE true

// Light Gate

#define LIGHT_GATE_CALIBRATION_COUNT 10
#define LIGHT_GATE_CALIBRATION_BUFFER 10

// TSOPS

#define TSOP_NUM 24

#define TSOP_LOOP_COUNT 255

#define TSOP_UNLOCK_DELAY 2

#define TSOP_BEST_TSOP_NO_ANGLE 5
#define TSOP_BEST_TSOP_NO_STRENGTH 2

#define TSOP_K1 10
#define TSOP_K2 2
#define TSOP_K3 1

#define TSOP_MIN_IGNORE 50
#define TSOP_MAX_IGNORE 220

#define TSOP_FIRST_TSOP_WEIGHT 3
#define TSOP_SECOND_TSOP_WEIGHT 2

#define TSOP_FILTER_NOISE true
#define TSOP_FILTER_SURROUNDING true

#define TSOP_NO_BALL 400

#define TSOP_HAS_BALL_STRENGTH 130

// IMU

#define HEADING_KP 4.0
#define HEADING_KI 0.0
#define HEADING_KD 0.3
#define HEADING_MAX_CORRECTION 180

#define IMU_CALIBRATION_COUNT 20
#define IMU_CALIBRATION_TIME 50
#define IMU_THRESHOLD 1000

#define MPU9250_ADDRESS 0x68
#define MAG_ADDRESS 0x0C

#define GYRO_FULL_SCALE_250_DPS 0x00
#define GYRO_FULL_SCALE_500_DPS 0x08
#define GYRO_FULL_SCALE_1000_DPS 0x10
#define GYRO_FULL_SCALE_2000_DPS 0x18

#define ACC_FULL_SCALE_2_G 0x00
#define ACC_FULL_SCALE_4_G 0x08
#define ACC_FULL_SCALE_8_G 0x10
#define ACC_FULL_SCALE_16_G 0x18

// Orbit

#define ORBIT_SHORT_STRENGTH 135
#define ORBIT_BIG_STRENGTH 105

#define ORBIT_SMALL_ANGLE 5
#define ORBIT_BIG_ANGLE 60

#define ORBIT_BALL_FORWARD_ANGLE_TIGHTENER 0.5

#define ORBIT_SPEED 200

#define ATTACK_BACKWARDS_MAX_STRENGTH 100

#define BALL_FRONT_BUFFER 10

// Defence

#define DEFEND_SHORT_STRENGTH 135
#define DEFEND_SMALL_ANGLE 10

#define DEFEND_GOAL_DISTANCE 0.3
#define DEFEND_LEFT_GOAL_DISTANCE 0.7

#define DEFEND_SIDEWAYS_MAX_SPEED 170
#define DEFEND_DISTANCE_MAX_SPEED 160
#define DEFEND_SIDEWAYS_MULTIPLIER 400
#define DEFEND_DISTANCE_MULTIPLIER 10

#define DEFEND_SIDEWAYS_KP -4.4
#define DEFEND_SIDEWAYS_KI 0.0
#define DEFEND_SIDEWAYS_KD 0.0
#define DEFEND_SIDEWAYS_MAX_SPEED 170

#define DEFEND_CHARGE_STRENGTH 150

// Play Mode

#define PLAYMODE_DEFAULT PlayMode::attack

#define ROBOT_ID_EEPROM 0

#define PLAYMODE_SWITCH_DEFENDER_ANGLE 30
#define PLAYMODE_SWITCH_ATTACKER_ANGLE 90
#define PLAYMODE_SWITCH_DEFENDER_STRENGTH 125
#define PLAYMODE_SWITCH_ATTACKER_STRENGTH 120
#define PLAYMODE_SWITCH_ATTACKER_STRENGTH_FAR 95

#define PLAYMODE_SWITCH_TIME 1000000

// Pixy

#define GOAL_MIN_AREA 100
#define GOAL_HEIGHT 0.14

#define PIXY_FRAME_WIDTH 320
#define PIXY_FRAME_HEIGHT 200
#define PIXY_HORIZONTAL_FOV 75
#define PIXY_VERTICAL_FOV 47

#define PIXY_UPDATE_TIME 20000
#define LAST_SEEN_GOAL_TIME 100000

#define PIXY_ENABLED true
#define FACE_GOAL true
#define ALWAYS_FACE_GOAL false

#define FACE_GOAL_SHORT_STRENGTH 120
#define FACE_GOAL_BIG_STRENGTH 110

#define CENTRE_GOAL_DISTANCE 1.0
#define CENTRE_GOAL_DISTANCE_CLOSE 1.6

#define CENTRE_DISTANCE_KP -400.0
#define CENTRE_DISTANCE_KI 0.0
#define CENTRE_DISTANCE_KD -5.0

#define CENTRE_SIDEWAYS_KP -800.0
#define CENTRE_SIDEWAYS_KI 0.0
#define CENTRE_SIDEWAYS_KD -5.0

// Yellow = 1, Blue = 2
#define COLOUR_SIG_ATTACK 1
#define COLOUR_SIG_DEFEND 2

// Sonars

#define SONAR_FRONT_ADDRESS 0x70 // 0xE0
#define SONAR_RIGHT_ADDRESS 0x71 // 0xE2
#define SONAR_BACK_ADDRESS 0x72 // 0xE4
#define SONAR_LEFT_ADDRESS 0x73 // 0xE6

#define SONAR_RANGE_TIME 65000

// Debug

#define DEBUG_APP false

#define DEBUG_LINE false
#define DEBUG_TSOP false

#define LED_BLINK_TIME_MASTER 100000
#define LED_BLINK_TIME_SLAVE_TSOP 300000
#define LED_BLINK_TIME_SLAVE_LIGHT 700000

// XBee

#define XBEE_ENABLED true
#define XBEE_LOST_COMMUNICATION_TIME 1000000
#define XBEE_UPDATE_TIME 200000
#define XBEE_BAUD 57600
#define XBEE_START 255
#define XBEE_PACKET_SIZE 10

#define XBEESERIAL Serial4

// Motors

#define MOTOR_ANGLE 40

#endif // CONFIG_H
