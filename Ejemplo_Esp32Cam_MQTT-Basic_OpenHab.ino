
/*
 * Administración de Refrigeración operado en OpenHab
 * Por: Abril Ramírez Gómez
 * Fecha: 30 de mayo de 2022
 * 
 * Programa creado a partir de los programas MQTT_basic y ESP32Cam_Operadores
 * También me base en el programa de Refrigeración
 * 
 * Características
 *   Sensor de temperatura y humedad
 *   Boton1: Manual. Activa la refrigeración al ser presionado
 *   Boton2: Alta demanda
 *   Boton3: Sobre carga de funcionamiento
 *   
 *   Led1: Refrigeracion manual
 *   Led2: Refrigeración automática
 *   
 *   Que se encienda la refrigeracion manual cada que se presione el boton de manual
 *   
 *   La refrigeracion manual tiene prioridad a la refrigeracion automática.
 *   
 *   Si la termperatura es alta (mayor a 28, por ejemplo), se activa la refrigeracion automática
 *   
 *   Si tengo alta demanda o sobre carga, se activa la refrigeración automática
 *   
 *   Si tengo alta demanda o sobrecarga de funcionamiento y ademas temperatura alta, se activan ambas refrigeraciones
 *   
 *   Botones 14, 15, 13
 *   Leds 4, 2
 *   DHT11 12
 */
// Ejercicio de ejemplo de Esp32Cam, publicando con MQTTBasic y viendo en OpenHab
// Bibliotecas
#include <WiFi.h>  // Biblioteca para el control de WiFi
#include <PubSubClient.h> //Biblioteca para conexion MQTT
#include "DHT.h" // Incluir la biblioteca de DHT11

//Datos de WiFi
const char* ssid = "CASTILLO";  // Aquí debes poner el nombre de tu red
const char* password = "YolandaC23";  // Aquí debes poner la contraseña de tu red

//Datos del broker MQTT
//Puse un Broker IP público; para actualizar con: nslookup broker.hivemq.com en terminal ubuntu
const char* mqtt_server = "192.168.39.131"; //esta es la dirección IP local (la saque en terminal con ifconfig)
const char* topicTemp= "codigoIoT/SIC/G5/temp"; //Tema en el que se publica la temperatura de DHT11
const char* topicHum= "codigoIoT/SIC/G5/hum"; //Tema en el que se publica la humedad de DHT11
const char* topicLed= "codigoIoT/SIC/G5/led"; //Tema para leer el switc de activar manual
IPAddress server(192,168,39,131);



// Constantes
// Constantes para manejar el DHT11
#define DHTPIN 12       // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11
#define TemperaturaAlta 32 // Límite de temperatura, para acivar la refrigeración Automática (LEDA en pin 4) y que no ocupe memoria


const int BOTON1 = 13; // Manual
const int BOTON2 = 15; // Alta Demanda
const int BOTON3 = 14; // Sobrecarga de Funcionamiento

const int LEDM = 2; // Refrigeración MANUAL
const int LEDA = 4; // Refreigeración Automática


// Definición de objetos
WiFiClient espClient; // Este objeto maneja los datos de conexion WiFi
PubSubClient client(espClient); // Este objeto maneja los datos de conexion al broker
DHT dht(DHTPIN, DHTTYPE);   // Objeto para manejar el DHT11


// Variables
int dato1=0, dato2=0, dato3=0, l=1;
float t, h, tant=0,timeNow,timeLast=0;
int wait = 5000; //Tiempo de espera para madar mensajes por MQTT
char dataString[8]; // Define una arreglo de caracteres para enviarlos por MQTT, especifica la longitud del mensaje en 8 caracteres

// Condiciones iniciales - Se ejecuta sólo una vez al energizar

void setup() {// Inicio de void setup ()
  // Iniciar comunicación serial
  Serial.begin(115200);
  delay(10);
  //Se realiza conección a WiFi
  Serial.println();
  Serial.println(); 
  Serial.print("Conectar a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password); // Esta es la función que realiz la conexión a WiFi
  while (WiFi.status() != WL_CONNECTED) { // Este bucle espera a que se realice la conexión
    Serial.print(".");  // Indicador de progreso
    delay (500);
  }

 // Cuando se haya logrado la conexión, se informa lo siguiente
  Serial.println();
  Serial.println("WiFi conectado");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());
  
  pinMode (BOTON1, INPUT_PULLUP);//Configurar el pin del boton como entrada //INPUT, OUTPUT
  pinMode (BOTON2, INPUT_PULLUP);//Configurar el pin del boton como entrada //INPUT, OUTPUT
  pinMode (BOTON3, INPUT_PULLUP);//Configurar el pin del boton como entrada //INPUT, OUTPUT
  
  pinMode (LEDM, OUTPUT); // Configurar el pin del led como salida de voltaje
  pinMode (LEDA, OUTPUT); // Configurar el pin del led como salida de voltaje
  
  dht.begin(); //Inicialización de la comunicación con el sensor DHT11

  // Conexión con el broker MQTT
  client.setServer(server, 1883); // Conectarse a la IP del broker en el puerto indicado
  client.setCallback(callback); // Activar función de CallBack, permite recibir mensajes MQTT y ejecutar funciones a partir de ellos
  delay(1500);  // Esta espera es preventiva, espera a la conexión para no perder información
  
  timeLast=millis(); //Para iniciar el control del tiempo

}// Fin de void setup

// Cuerpo del programa - Se ejecuta constamente
void loop() {// Inicio de void loop

   // put your main code here, to run repeatedly:
  while (WiFi.status() == WL_CONNECTED) { 
   
   //Verificar siempre que haya conexión al broker
    if (!client.connected()) {
    Serial.println("No esta conectado al broker");
    reconnect();  // En caso de que no haya conexión, ejecutar la función de reconexión, definida despues del void setup ()
    }// fin del if (!client.connected())
    client.loop(); // Esta función es muy importante, ejecuta de manera no bloqueante las funciones necesarias para la comunicación con el broker
    
  leeSensor();  //En esta función se lee el DHT11 y s publica la temperatura en el tema
  leeBotones(); //En esta función únicamente se lee el estado de los botones
  activaRefrigeraciones(); //Según la lectura de los botones, activa la refrigeración según criterios
  }
  } // Fin de void loop



// Funciones del usuario

 void leeSensor ()
 {
    // Read temperature as Celsius (the default)
    
    timeNow= millis();
    if (timeNow - timeLast > wait) {
      timeLast = timeNow; // Actualización de seguimiento de tiempo no bloqueante
      Serial.println(F("-----"));
      //Primero lee y manda Temperatura
      t = dht.readTemperature();
      dtostrf(t, 1, 2, dataString);
      Serial.print(F("La temperatura publicada: "));      
      Serial.print(dataString);
      Serial.print(F(" °C , "));
      Serial.print(F("En el tópico: "));
      Serial.println(topicTemp);
      client.publish(topicTemp, dataString); // Esta es la función que envía los datos por MQTT
      
      delay(100);
      //Segundo lee y manda Humedad
      h = dht.readHumidity();
      dtostrf(h, 1, 2, dataString);
      Serial.print(F("La humedad publicada    : "));
      Serial.print(dataString);
      Serial.print(F(" hum, "));
      Serial.print(F("En el tópico: "));
      Serial.println(topicHum);
      client.publish(topicHum, dataString); // Esta es la función que envía los datos por MQTT

      

      if (isnan(t)) { // Check if any reads failed and exit early (to try again).
       Serial.println(F("Failed to read from DHT sensor!"));
       return;
     }
    }
 }

 void leeBotones ()
 {
  dato1 = digitalRead (BOTON1); //Leer el pin del boton 1 MANUAL
  dato2 = digitalRead (BOTON2); //Leer el pin del boton 2 ALTA DEMANDA
  dato3 = digitalRead (BOTON3); //Leer el pin del boton 3 SOBRE CARGA
 }

 void activaRefrigeraciones ()
 {
   //Para activar la refrigeración Automática
  if (t>=TemperaturaAlta) {
  digitalWrite (LEDA, HIGH);
  //Publicar alerta de temperatura Alta
  //dataString = 'TmAlert';
  //client.publish(topicTemp, dataString); // Esta es la función que envía los datos por MQTT
  
  }
  if (t<TemperaturaAlta) {
  digitalWrite (LEDA, LOW);}
  
  //Para activar la refrigeración Manual
  if (!dato1 || !l)
  {
  digitalWrite (LEDM, HIGH); //Prender el led Refrigeración manual
  delay (200);
  } else {
    digitalWrite (LEDM, LOW); //Prender el led Refrigeración manual
  delay (200);}
   
  // Alta demanda o sobre carga, se activa la refrigeración automática
  if (!dato2 || !dato3) {
      digitalWrite (LEDA, HIGH); //Prender el led refrigeración automática
      delay (200);
      }
  }

// Esta función permite tomar acciones en caso de que se reciba un mensaje correspondiente a un tema al cual se hará una suscripción
void callback(char* topic, byte* message, unsigned int length) {

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageTemp; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    messageTemp += (char)message[i];
  }
  String messageHum; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    messageHum += (char)message[i];
  }
  String messageLed; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    messageLed += (char)message[i];
   
  }
  //* Se comprueba que el mensaje se haya concatenado correctamente
  //Serial.println();
  //Serial.print ("Mensaje LED concatenado en una sola variable: ");
  //Serial.println (messageTemp);
  

  // En esta parte puedes agregar las funciones que requieras para actuar según lo necesites al recibir un mensaje MQTT

  if (String(topic) == topicLed) {  // En caso de recibirse mensaje en el tema esp32/output
    if(messageLed == "true"){
      l=0;
      Serial.println("PRENDE refrigeración manual desde Dashboard");
    }// fin del if topic 
    if(messageLed == "false"){
      l=1;
      Serial.println("APAGA refrigeración manual desde Dashboard");
    }// fin del if topic 
  }// fin del if topic
}// fin del void callback


// Función para reconectarse
void reconnect() {
  // Bucle hasta lograr conexión
  Serial.println("Entre a la función reconect");
  while (!client.connected()) { // Pregunta si hay conexión
    Serial.print("Tratando de contectarse...");
    // Intentar reconexión
    if (client.connect("espClient")) { //Pregunta por el resultado del intento de conexión
      Serial.println("Conectado");
      client.subscribe(topicTemp); // Esta función realiza la suscripción al tema temperatura
      client.subscribe(topicHum);  // Esta función realiza la suscripción al tema humedad
      client.subscribe(topicLed);  // Esta función realiza la suscripción al tema led
    }// fin del  if (client.connect("espClient"))
    else {  //en caso de que la conexión no se logre
      Serial.print("Conexion fallida, Error rc=");
      Serial.print(client.state()); // Muestra el codigo de error
      Serial.println(" Volviendo a intentar en 5 segundos");
      // Espera de 5 segundos bloqueante
      delay(5000);
      Serial.println (client.connected ()); // Muestra estatus de conexión
    }// fin del else
  }// fin del bucle while (!client.connected())
}// fin de void reconnect
  
