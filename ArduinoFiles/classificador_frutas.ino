
#include <TensorFlowLite.h>

#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>
#include <Arduino_APDS9960.h>
#include "model.h"

// global variables used for TensorFlow Lite (Micro)
tflite::MicroErrorReporter tflErrorReporter;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;
// pull in all the TFLM ops
tflite::AllOpsResolver tflOpsResolver;
// Create a static memory buffer for TFLM, the size may need to
// be adjusted based on the model you are using
constexpr int tensorArenaSize = 8 * 1024;
byte tensorArena[tensorArenaSize];

// array to map gesture index to a name
const char* CLASSES[] = {
  u8"\U0001F34C", // Banana
  u8"\U0001F34B", // Limão
  u8"\U0001F34E", // Apple
};

#define NUM_CLASSES (sizeof(CLASSES) / sizeof(CLASSES[0]))

void setup() {
  Serial.begin(9600);
  while (!Serial) {};

  Serial.println("Object classification using RGB color sensor");
  Serial.println("--------------------------------------------");
  Serial.println("Arduino Nano 33 BLE Sense running TensorFlow Lite Micro");
  Serial.println("");

  if (!APDS.begin()) {
    Serial.println("Error initializing APDS9960 sensor.");
  }

  // get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }

  // Create an interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);

  // Allocate memory for the model's input and output tensors
  tflInterpreter->AllocateTensors();

  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);
}

void loop() {
  int r, g, b, p, c;
  float sum;

  // check if both color and proximity data is available to sample
  while (!APDS.colorAvailable() || !APDS.proximityAvailable()) {}

  // read the color and proximity sensor
  APDS.readColor(r, g, b, c);
  p = APDS.readProximity();
  sum = r + g + b;

  // check if there's an object close and well illuminated enough
  if (p == 0 && c > 10 && sum > 0) {

    float redRatio = r / sum;
    float greenRatio = g / sum;
    float blueRatio = b / sum;
    
    // Serial.print(redRatio);
    // Serial.print(",");
    // Serial.print(greenRatio);
    // Serial.print(",");
    // Serial.println(blueRatio);


    // input sensor data to model
    tflInputTensor->data.f[0] = redRatio;
    tflInputTensor->data.f[1] = greenRatio;
    tflInputTensor->data.f[2] = blueRatio;

    // Run inferencing
    TfLiteStatus invokeStatus = tflInterpreter->Invoke();
    if (invokeStatus != kTfLiteOk) {
      Serial.println("Invoke failed!");
      while (1);
      return;
    }

    // Output results
    for (int i = 0; i < NUM_CLASSES; i++) {
      Serial.print(CLASSES[i]);
      Serial.print(" ");
      Serial.print(int(tflOutputTensor->data.f[i] * 100));
      Serial.print("%\n");
    }
    Serial.println();
    Serial.println("--------------------------------------------");

    // Wait for the object to be moved away
    while (!APDS.proximityAvailable() || (APDS.readProximity() == 0)) {}
  }

}