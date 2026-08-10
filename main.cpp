//Universidad del Valle de Guatemala
//Maria Isabel Rivera De Leon
//Electrónica Digital 2
//Proyecto 1

#include <Arduino.h>
#include <driver/gpio.h>
#include <stdint.h>

//Librerías de Adafruit IO
#include <WiFi.h> 
#include "AdafruitIO_WiFi.h"

//************************************************/
// LED RGB
//************************************************/
#define LEDR GPIO_NUM_27
#define LEDG GPIO_NUM_26
#define LEDB GPIO_NUM_25

//************************************************/
// Boton
//************************************************/
#define BT GPIO_NUM_39

//************************************************/
// Sensor de temperatura LM35
//************************************************/
#define ST GPIO_NUM_34

//************************************************/
// Servo
//************************************************/
#define SERVO GPIO_NUM_32

//************************************************/
// Displays 7 segmentos
//************************************************/
#define A GPIO_NUM_23
#define B GPIO_NUM_22
#define C GPIO_NUM_21
#define D GPIO_NUM_19
#define E GPIO_NUM_18
#define F GPIO_NUM_4
#define G GPIO_NUM_15
#define DP GPIO_NUM_14

//************************************************/
//Transistores de los displays
//************************************************/
#define T1 GPIO_NUM_13
#define T2 GPIO_NUM_5
#define T3 GPIO_NUM_33

//************************************************/
// Canales de PWM de LED RGB
//************************************************/
#define canalR 0
#define canalG 1
#define canalB 2

//************************************************/
// PWM Servo
//************************************************/
#define canalServo 3
#define freqServo 50
#define resServo 12

//************************************************/
// Wifi y Adafruit IO
//************************************************/
#include "infoper.h"  //Lama a la información de conexión a Adafruit IO y WiFi

#define IO_LOOP_DELAY 5000


// Conexión Adafruit IO 
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// Feeds 
AdafruitIO_Feed *feedTemp = io.feed("temperatura"); 
AdafruitIO_Feed *feedRejilla = io.feed("rejilla");

//************************************************/
// Variables
//************************************************/    
float temperatura = 0.0;


// Digitos del display
uint8_t d1 = 0;
uint8_t d2 = 0;
uint8_t d3 = 0;

// Multiplexeo
uint8_t actual = 0;


// Estado del sistema
bool iniciado = false;

// Bandera botón
volatile bool banderaBoton = false;
unsigned long tiempoBoton = 0;

// Adafruit
unsigned long lastUpdate = 0;
bool nuevaMedicion = false;
String estadoRejilla = "CERRADO";

// Puntero al timer
hw_timer_t *timerDisplay = NULL;

//************************************************/
// ISR Boton
//************************************************/
void IRAM_ATTR ISR_BOTON() {
  banderaBoton = true;
}

//************************************************/
// Apagar todo en el inicio
//************************************************/
void apagarTodo() {
  digitalWrite(A,HIGH);
  digitalWrite(B,HIGH);
  digitalWrite(C,HIGH);
  digitalWrite(D,HIGH);
  digitalWrite(E,HIGH);
  digitalWrite(F,HIGH);
  digitalWrite(G,HIGH);
  digitalWrite(DP,HIGH);

  digitalWrite(T1,LOW);
  digitalWrite(T2,LOW);
  digitalWrite(T3,LOW);
}

//************************************************/
// Mostrar Numero
//************************************************/
void IRAM_ATTR mostrarISR(uint8_t n) {
  switch(n){
    case 0:
      digitalWrite(A,LOW); 
      digitalWrite(B,LOW); 
      digitalWrite(C,LOW);
      digitalWrite(D,LOW); 
      digitalWrite(E,LOW); 
      digitalWrite(F,LOW);
      break;

    case 1:
      digitalWrite(B,LOW); 
      digitalWrite(C,LOW);
      break;

    case 2:
      digitalWrite(A,LOW); 
      digitalWrite(B,LOW); 
      digitalWrite(D,LOW);
      digitalWrite(E,LOW); 
      digitalWrite(G,LOW);
      break;

    case 3:
      digitalWrite(A,LOW); 
      digitalWrite(B,LOW); 
      digitalWrite(C,LOW);
      digitalWrite(D,LOW); 
      digitalWrite(G,LOW);
      break;

    case 4:
      digitalWrite(B,LOW); 
      digitalWrite(C,LOW);
      digitalWrite(F,LOW); 
      digitalWrite(G,LOW);
      break;

    case 5:
      digitalWrite(A,LOW); 
      digitalWrite(C,LOW); 
      digitalWrite(D,LOW);
      digitalWrite(F,LOW); 
      digitalWrite(G,LOW);
      break;

    case 6:
      digitalWrite(A,LOW); 
      digitalWrite(C,LOW); 
      digitalWrite(D,LOW);
      digitalWrite(E,LOW); 
      digitalWrite(F,LOW); 
      digitalWrite(G,LOW);
      break;

    case 7:
      digitalWrite(A,LOW); 
      digitalWrite(B,LOW); 
      digitalWrite(C,LOW);
      break;

    case 8:
      digitalWrite(A,LOW); 
      digitalWrite(B,LOW); 
      digitalWrite(C,LOW);
      digitalWrite(D,LOW); 
      digitalWrite(E,LOW); 
      digitalWrite(F,LOW);
      digitalWrite(G,LOW);
      break;

    case 9:
      digitalWrite(A,LOW); 
      digitalWrite(B,LOW); 
      digitalWrite(C,LOW);
      digitalWrite(D,LOW); 
      digitalWrite(F,LOW); 
      digitalWrite(G,LOW);
      break;
  }
}


//************************************************/
// Multiplexeo de displays
//************************************************/
void IRAM_ATTR ISR_DISPLAY() {
  if(!iniciado){
    apagarTodo();
    return;
  }
  apagarTodo();
  if(actual == 0){
    mostrarISR(d1);
    digitalWrite(T1,HIGH);
    actual = 1;
  }
  else if(actual == 1){
    mostrarISR(d2);
    digitalWrite(DP,LOW);
    digitalWrite(T2,HIGH);
    actual = 2;
  }
  else{
    mostrarISR(d3);
    digitalWrite(T3,HIGH);
    actual = 0;
  }
}

//************************************************/
// PWM LED RGB
//************************************************/
void initRGB() {
  //Rojo
  ledcSetup(canalR,5000,8);
  ledcAttachPin(LEDR,canalR);

  //Verde
  ledcSetup(canalG,5000,8);
  ledcAttachPin(LEDG,canalG);

  //Azul
  ledcSetup(canalB,5000,8);
  ledcAttachPin(LEDB,canalB);

  // Apagado inicial
  ledcWrite(canalR,0);
  ledcWrite(canalG,0);
  ledcWrite(canalB,0);
}

//************************************************/
// Inicializar PWM para el servo motor
//************************************************/
void initServo() {
  ledcSetup(canalServo,freqServo,resServo);
  ledcAttachPin(SERVO,canalServo);
  // Posición inicial
  ledcWrite(canalServo,205);
}

//************************************************/
// Mover servo a un ángulo específico
//************************************************/
void moverServo(int angulo){
  // 0° -> 51
  // 90° -> 410
  int duty = map(angulo, 0, 90, 205, 410);
  ledcWrite(canalServo, duty);
}

//************************************************/
// Medir temperatura LM35
//************************************************/
void sensorTemperatura() {
  int lectura = analogRead(ST);
  float voltaje = lectura * (3.3 / 4095.0);
  temperatura = voltaje / 0.01;
}

//************************************************/
// Digitos del display según temperatura
//************************************************/
void digitosdisplay() {
    int valor = (int)(temperatura * 10.0 + 0.5);
    d1= valor / 100;
    d2 = (valor / 10) % 10;
    d3 = valor % 10;
}

void medir() {
  sensorTemperatura();
  digitosdisplay();

  // RGB y servo
  if(temperatura < 23.0){
    // Azul
    ledcWrite(canalR,0);
    ledcWrite(canalG,0);
    ledcWrite(canalB,255);
    moverServo(0);
    estadoRejilla = "CERRADO";
  }

  else if(temperatura < 25.0){
    // Verde
    ledcWrite(canalR,0);
    ledcWrite(canalG,255);
    ledcWrite(canalB,0);
    moverServo(45);
    estadoRejilla = "MEDIO";
  }

  else if(temperatura < 27.0){
    // Amarillo
    ledcWrite(canalR,255);
    ledcWrite(canalG,255);
    ledcWrite(canalB,0);
    moverServo(45);
    estadoRejilla = "MEDIO";
  }

  else{
    // Rojo
    ledcWrite(canalR,255);
    ledcWrite(canalG,0);
    ledcWrite(canalB,0);
    moverServo(90);
    estadoRejilla = "ABIERTO";
  }

  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println(" C - Rejilla");
  Serial.println(estadoRejilla); 

  nuevaMedicion = true;
}




void setup() {
  Serial.begin(115200);
  // Displays 7 segmentos
  pinMode(A,OUTPUT);
  pinMode(B,OUTPUT);
  pinMode(C,OUTPUT);
  pinMode(D,OUTPUT);
  pinMode(E,OUTPUT);
  pinMode(F,OUTPUT);
  pinMode(G,OUTPUT);
  pinMode(DP,OUTPUT);

  // Transmisores de displays
  pinMode(T1,OUTPUT);
  pinMode(T2,OUTPUT);
  pinMode(T3,OUTPUT);

  // Entradas
  pinMode(BT,INPUT);
  pinMode(ST,INPUT);

  // Estado inicial
  apagarTodo();
  initRGB();
  initServo();

  iniciado = false;

  // Interrupción botón
  attachInterrupt(BT, ISR_BOTON, FALLING);
  timerDisplay = timerBegin(0, 80, true);

  // Asociar interrupción
  timerAttachInterrupt(timerDisplay, &ISR_DISPLAY, true);

  // Ejecutar cada 1000 microsegundos
  timerAlarmWrite(timerDisplay, 1000, true );

  // Activar timer
  timerAlarmEnable(timerDisplay);
  Serial.println("Sistema listo");


  //************************************************
  // ADAFRUIT IO
  //************************************************
  Serial.println("Conectando a Adafruit IO...");
  io.connect();
  // Esperar conexión
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Conectado a Adafruit IO");
}



void loop() {
  io.run();

  if(millis() > (lastUpdate + IO_LOOP_DELAY)) {
    // Solo enviar si existe una nueva medición
    if(nuevaMedicion) {
      //************************************************
      // Temperatura
      //************************************************
      Serial.print("sending temperatura -> ");
      Serial.println(temperatura);
      feedTemp->save(temperatura);

      //************************************************
      // Estado rejilla
      //************************************************
      Serial.print("sending rejilla -> ");
      Serial.println(estadoRejilla);
      feedRejilla->save(estadoRejilla);
      // Ya enviamos los datos
      nuevaMedicion = false;
    }

    // Igual que ejemplo del profesor
    lastUpdate = millis();
  }

  // Si se presionó el botón
  if (banderaBoton) {
    banderaBoton = false;
    // Antirrebote
    if (millis() - tiempoBoton > 200) {
      medir();
      iniciado = true;
      tiempoBoton = millis();
    }
  }
}