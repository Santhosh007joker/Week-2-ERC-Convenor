//Extra Libraries to include

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

//Declare MPU

Adafruit_MPU6050 mpu;

//New Global Variables

uint32_t      stepCount    = 0;
unsigned long sessionStart = 0;
double        sumTemp      = 0.0;
double        sumHum       = 0.0;
uint32_t      envSamples   = 0;
float         accelX, accelY, accelZ;

//In setup() --->to initialize MPU

mpu.begin();
mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
mpu.setGyroRange(MPU6050_RANGE_500_DEG);
mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
sessionStart = millis();

//In loop() --->we only use acceleration for this project

sensors_event_t a, g, t;
mpu.getEvent(&a, &g, &t);
accelX = a.acceleration.x;
accelY = a.acceleration.y;
accelZ = a.acceleration.z;

{

  
  // step detection logic goes here


}




