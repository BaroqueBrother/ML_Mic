#include "Particle.h"
#include "Microphone_PDM.h"
#include "MicWavWriter.h"
#include "IoTClassroom_CNM.h"



SYSTEM_MODE(AUTOMATIC);

const unsigned long MAX_RECORDING_LENGTH_MS = 30000;

// This is the IP Address and port that the server.js node server is running on.
// byte serverAddr[] = {10,0,0,11}; // **UPDATE THIS**
IPAddress serverAddr = IPAddress(164,92,126,234);
int serverPort = 3000;

const int SAMPLE_RATE = 16000;
const int BITS_PER_SAMPLE = 16;
const int CHANNELS = 1;

const int RECORD_SECONDS = 3;
const int NUM_SAMPLES = SAMPLE_RATE * RECORD_SECONDS;

// 16-bit samples, mono
int16_t audioBuffer[NUM_SAMPLES];

volatile int samplesRecorded = 0;
bool recording = false;
bool readyToSend = false;


void sendAudioToServer(const char *label);
void startRecording();
void stopRecording();
TCPClient client;



Button audioButton(D4);

void setup() {

  Serial.begin(9600);
  waitFor(Serial.isConnected, 5000);

  while(!WiFi.ready()){
    Serial.printf(" . ");


  }

	// Blue D7 LED indicates recording is on
	pinMode(D7, OUTPUT);
	digitalWrite(D7, LOW);

	// Optional, just for testing so I can see the logs below
	// waitFor(Serial.isConnected, 10000);

	// output size is typically Microphone_PDM::OutputSize::UNSIGNED_8 or icrophone_PDM::OutputSize::SIGNED_16
	// and must match what the server is expecting (set on the command line)

	// Range must match the microphone. The Adafruit PDM microphone is 12-bit, so Microphone_PDM::Range::RANGE_2048.

	// The sample rate default to 16000. The other valid value is 8000.

	int err = Microphone_PDM::instance()
		.withOutputSize(Microphone_PDM::OutputSize::SIGNED_16)
		.withRange(Microphone_PDM::Range::RANGE_2048)
		.withSampleRate(16000)
		.init();

	if (err) {
		Log.error("PDM decoder init err=%d", err);
	}

	err = Microphone_PDM::instance().start();
	if (err) {
		Log.error("PDM decoder start err=%d", err);
	}

}

void loop() {

	if((audioButton.isClicked()) && !recording){
    startRecording();
  }

  if(recording){
     Microphone_PDM::instance().noCopySamples([](void *pSamples, size_t numSamples) {
            int16_t *samples = (int16_t *)pSamples;

            for (size_t i = 0; i < numSamples; i++) {
                if (samplesRecorded < NUM_SAMPLES) {
                    audioBuffer[samplesRecorded++] = samples[i];
                } else {
                    stopRecording();
                    break;
                }
            }
        });
    }

    if (readyToSend) {
        readyToSend = false;
        String audioTag = "audio_WAV_";
        String tagExtension = String(millis(), DEC);
        String fileName = String(audioTag + tagExtension);
        sendAudioToServer(fileName);
    }
  }






void sendAudioToServer(const char *label) {
    int audioBytes = samplesRecorded * sizeof(int16_t);

    if (!client.connect(serverAddr, serverPort)) {
        Serial.println("Connection failed");
        return;
    }

    Serial.println("Sending audio to Node.js...");

    client.println("POST /upload-audio HTTP/1.1");

    client.print("Host: ");
    client.println(serverAddr);

    client.println("Content-Type: application/octet-stream");

    client.print("X-Sample-Rate: ");
    client.println(SAMPLE_RATE);

    client.print("X-Bits-Per-Sample: ");
    client.println(BITS_PER_SAMPLE);

    client.print("X-Channels: ");
    client.println(CHANNELS);

    client.print("X-Label: ");
    client.println(label);

    client.print("Content-Length: ");
    client.println(audioBytes);

    client.println();

    client.write((uint8_t *)audioBuffer, audioBytes);

    while (client.connected()) {
        while (client.available()) {
            Serial.write(client.read());
        }
    }

    client.stop();

    Serial.println("Upload complete");
}

void startRecording() {
    digitalWrite(D7, HIGH);
    samplesRecorded = 0;
    recording = true;
    readyToSend = false;

    Serial.println("Recording 3 seconds...");

    Microphone_PDM::instance().start();
}

void stopRecording() {
    digitalWrite(D7, LOW);
    Microphone_PDM::instance().stop();

    recording = false;
    readyToSend = true;

    Serial.print("Recording stopped. Samples recorded: ");
    Serial.println(samplesRecorded);
}