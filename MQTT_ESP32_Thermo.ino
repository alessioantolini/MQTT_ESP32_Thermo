/*
 *******************************************************************************
 *
 * Purpose: Example of using the Arduino MqttClient with Esp8266WiFiClient.
 * Project URL: https://github.com/monstrenyatko/ArduinoMqtt
 *
 *******************************************************************************
 * Copyright Oleg Kovalenko 2017.
 *
 * Distributed under the MIT License.
 * (See accompanying file LICENSE or copy at http://opensource.org/licenses/MIT)
 *******************************************************************************
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <Arduino.h>
#include <oled.h>

OLED display=OLED(4,5,16);

// Enable MqttClient logs
#define MQTT_LOG_ENABLED 1
// Include library
#include <MqttClient.h>


#define LOG_PRINTFLN(fmt, ...)	logfln(fmt, ##__VA_ARGS__)
#define LOG_SIZE_MAX 128
void logfln(const char *fmt, ...) {
	char buf[LOG_SIZE_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, LOG_SIZE_MAX, fmt, ap);
	va_end(ap);
	Serial.println(buf);
}

#define HW_UART_SPEED									115200L
#define MQTT_ID											"MultiSensor01"
const char* MQTT_COMMAND_SUB = "antolini/" MQTT_ID "/cmd/get";
const char* MQTT_COMMAND_PUB = "antolini/" MQTT_ID "/cmd/status";
const char* MQTT_DATA_PUB = "antolini/" MQTT_ID "/data";

static MqttClient *mqtt = NULL;
static WiFiClient network;




// ============== Object to supply system functions ============================
class System: public MqttClient::System {
public:

	unsigned long millis() const {
		return ::millis();
	}

	void yield(void) {
		::yield();
	}
};


// ============== Subscription callback ========================================
void processCommand(MqttClient::MessageData& md) {
  const MqttClient::Message& msg = md.message;
  char payload[msg.payloadLen + 1];
  memcpy(payload, msg.payload, msg.payloadLen);
  payload[msg.payloadLen] = '\0';
  LOG_PRINTFLN(
    "Message arrived: qos %d, retained %d, dup %d, packetid %d, payload:[%s]",
    msg.qos, msg.retained, msg.dup, msg.id, payload
  );

  //Visualizzazione messaggio
  display.clear();
  display.draw_string(10,10,payload,OLED::DOUBLE_SIZE);
  display.display();
}


// ============== Setup all objects ============================================
void setup() {
	// Setup hardware serial for logging
	Serial.begin(HW_UART_SPEED);
	while (!Serial);

  // Setup OLED
  display.begin();
  display.draw_string(4,2,"Starting...");
  display.display();

	// Setup WiFi network
	WiFi.mode(WIFI_STA);
	WiFi.hostname("ESP_" MQTT_ID);
	WiFi.begin("antolini", "0622754525");
	LOG_PRINTFLN("\n");
	LOG_PRINTFLN("Connecting to WiFi");
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		LOG_PRINTFLN(".");
	}
	LOG_PRINTFLN("Connected to WiFi");
	LOG_PRINTFLN("IP: %s", WiFi.localIP().toString().c_str());

	// Setup MqttClient
	MqttClient::System *mqttSystem = new System;
	MqttClient::Logger *mqttLogger = new MqttClient::LoggerImpl<HardwareSerial>(Serial);
	MqttClient::Network * mqttNetwork = new MqttClient::NetworkClientImpl<WiFiClient>(network, *mqttSystem);
	//// Make 128 bytes send buffer
	MqttClient::Buffer *mqttSendBuffer = new MqttClient::ArrayBuffer<128>();
	//// Make 128 bytes receive buffer
	MqttClient::Buffer *mqttRecvBuffer = new MqttClient::ArrayBuffer<128>();
	//// Allow up to 2 subscriptions simultaneously
	MqttClient::MessageHandlers *mqttMessageHandlers = new MqttClient::MessageHandlersImpl<2>();
	//// Configure client options
	MqttClient::Options mqttOptions;
	////// Set command timeout to 10 seconds
	mqttOptions.commandTimeoutMs = 10000;
	//// Make client object
	mqtt = new MqttClient(
		mqttOptions, *mqttLogger, *mqttSystem, *mqttNetwork, *mqttSendBuffer,
		*mqttRecvBuffer, *mqttMessageHandlers
	);
}

// ============== Main loop ====================================================
void loop() {
	// Check connection status
	if (!mqtt->isConnected()) {
		// Close connection if exists
		network.stop();
		// Re-establish TCP connection with MQTT broker
		LOG_PRINTFLN("Connecting");
		network.connect("10.0.1.10", 2883);
		if (!network.connected()) {
			LOG_PRINTFLN("Can't establish the TCP connection");
			delay(5000);
			ESP.reset();
		}
		// Start new MQTT connection
		MqttClient::ConnectResult connectResult;
		// Connect
		{
			MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
			options.MQTTVersion = 4;
			options.clientID.cstring = (char*)MQTT_ID;
			options.cleansession = true;
			options.keepAliveInterval = 15; // 15 seconds
      MQTTString username = MQTTString_initializer;
      username.cstring = "mqttuser";
      options.username = username;
      MQTTString password = MQTTString_initializer;
      password.cstring = "alan2.";
      options.password = password;
			MqttClient::Error::type rc = mqtt->connect(options, connectResult);
			if (rc != MqttClient::Error::SUCCESS) {
				LOG_PRINTFLN("Connection error: %i", rc);
				return;
			}
		}
		{
			// Add subscribe here if required
     MqttClient::Error::type rc = mqtt->subscribe(
       MQTT_COMMAND_SUB, MqttClient::QOS0, processCommand
      );
      if (rc != MqttClient::Error::SUCCESS) {
        LOG_PRINTFLN("Subscribe error: %i", rc);
        LOG_PRINTFLN("Drop connection");
        mqtt->disconnect();
        return;
      }

     
		}
	} else {
		{
			// Add publish here if required
     const char* buf = "Cicciobbello";
      MqttClient::Message message;
      message.qos = MqttClient::QOS0;
      message.retained = false;
      message.dup = false;
      message.payload = (void*) buf;
      message.payloadLen = strlen(buf);
      mqtt->publish(MQTT_DATA_PUB, message);
    
		}
		// Idle for 30 seconds
		mqtt->yield(30000L);
	}
}
