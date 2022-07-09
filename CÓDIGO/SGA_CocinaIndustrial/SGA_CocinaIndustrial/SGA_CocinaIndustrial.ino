/*
 * Sistema de Gestión Ambiental para Centros de producción, empaquetado y tratamiento de alimentos 
 * en las Industrias Alimentarias
 * Operado en NodeRed
 * Por: Armida González Lorence / Alexander Arroyo / Juan Carlos Estrada
 * Fecha: 29 de junio de 2022
 * 
 * Características
 *   Los Sensores de: temperatura y humedad, Gas LP, Air Quality, CO
 *   Activan los actuadores: Aire Acondicionado y ventanas según lecturas y especificaciones establecidas por el usuario
 *   
 *   Los actuadores operan según las lecturas de sensores(ver siguiente tabla:)
 *   -----------------------------------------------------------------------------------
 *   |                  Sensores                              |       Actuators         |
 *   -----------------------------------------------------------------------------------
 *   | Temperature |Humidity | GAS LP | Air Quality |   Co    |Air Conditioner | Windows|
 *   |  topicTemp  |topicHum |topicMQ6|  topicMQ135 |topicMQ7 | topic 
 *   ------------------------------------------------------------------------------------  
 *   |      *      |    *    |    0   |      *      |    *    |        0       |    1   |
 *   |      *      |    *    |    *   |      0      |    *    |        0       |    1   |          
 *   |      *      |    *    |    *   |      *      |    0    |        0       |    1   |
 *   |      *      | =>HumMax|    *   |      *      |    *    |        0       |    1   |
 *   |  =>TempMax  | <HumMax |    1   |      1      |    1    |        1       |    0   |
 *   |   <TempMax  | <HumMax |    1   |      1      |    1    |        0       |    0   |
 *   -------------------------------------------------------------------------------------
 *   Sensores
 *   DHT11  12   Temp and Humidity
 *   MQ-6   14   Gas LP
 *   MQ-7   13   CO Monóxido de carbono 
 *   MQ-135 15   Air Quality Sensor (CO)   
 *   
 *   Actuadores
 *   LedWin IO2     Windows  
 *   LedAC  IO4 AIR CONDITIONER
 *   ------------------------------------------------
 *   Componente     PinESP32CAM     Estados lógicos
 *   ------------------------------------------------
 *   ledWin---------GPIO 2---------On=>HIGH, Off=>LOW
 *   ledAC----------GPIO 4---------On=>HIGH, Off=>LOW
 *
 *   DHT11
 *   PIN 1 ---------5v
 *   PIN 2 ---------12
 *   PIN 3 ---------GND
 *   
 *   MQ7
 *   AC ----------- NC
 *   DC ----------- GPIO 13
 *   GND----------- GND
 *   VCC----------- 5v
 *   
 *   MQ6
 *   AC ----------- NC
 *   DC ----------- GPIO 14
 *   GND----------- GND
 *   VCC----------- 5v
 *      
 *   MQ135
 *   AC ----------- NC
 *   DC ----------- GPIO 15
 *   GND----------- GND
 *   VCC----------- 5v   
 *   
 */ 

// Bibliotecas
#include <WiFi.h>          // Biblioteca para el control de WiFi
#include <PubSubClient.h>  // Biblioteca para conexion MQTT
#include <DHT.h>           // Incluir la biblioteca de DHT11
#include <MQ135.h>         // Incluir biblioteca para manejar MQ6, MQ7 y MQ-135
   

//Datos de WiFi
const char* ssid     = "CASTILLO";  // Aquí debes poner el nombre de tu red
const char* password = "YolandaC23";  // Aquí debes poner la contraseña de tu red

//Datos del broker MQTT
const char* mqtt_server = "192.168.39.131"; //localhost (Para actualizar: Desde la terminal con ifconfig)
IPAddress server(192,168,39,131);

// Tópicos en los que se publica la información adquirida con los sensores
const char* topicTemp = "CodigoIoT/SGCIndustrial/Temp"; //Tema donde se publica la Temperatura
const char* topicHum  = "CodigoIoT/SGCIndustrial/Hum";  //Tema donde se publica la Humedad
const char* topicAirC = "CodigoIoT/SGCIndustrial/AirC"; //Tema donde se publica el estado de Aire Acondicionado
const char* topicWin  = "CodigoIoT/SGCIndustrial/Win";  //Tema donde se publica el estado de las ventanas
const char* topicMQ6  = "CodigoIoT/SGCIndustrial/MQ6";  //Tema donde se publica el estado del Gas LP
const char* topicMQ7  = "CodigoIoT/SGCIndustrial/MQ7";  //Tema donde se publica el estado del Co2
const char* topicMQ135= "CodigoIoT/SGCIndustrial/MQ135";//Tema donde se publica el estado del Alcohol
// Tópicos en los que se subscribe para extraer información del usuario TemperaturaAlta y HumedadAlta
const char* topicMaxTemp = "CodigoIoT/SGCIndustrial/ValoresCliente/LimitTemp"; // Temperatura máxima dada por el usuario
const char* topicMaxHum  = "CodigoIoT/SGCIndustrial/ValoresCliente/LimitHum";  // Humedad máxima dada por el usuario


// Constantes para manejar sensores
#define DHTPIN 12     // Digital pin 12 donde se conecta el DHT sensor
#define DHTTYPE DHT11 // DHT 11
#define PIN_MQ6       // MQ6
#define PIN_MQ7       // MQ7
#define PIN_MQ135     // MQ135

const int LEDW     =  2; // Ventanas
const int LEDA     =  4; // Refreigeración Automática
const int MQ7Pin   = 13; // CO Monoxido de Carbono
const int MQ6Pin   = 14; // GAS LP
const int MQ135Pin = 15; // Alcohol

// ---------Definición de objetos--------------
WiFiClient espClient; // Este objeto maneja los datos de conexion WiFi
PubSubClient client(espClient); // Este objeto maneja los datos de conexion al broker
DHT dht(DHTPIN, DHTTYPE);   // Objeto para manejar el DHT11

// ---------- Variables ------------------------
float t, h, mq135, mq6, mq7, tant=0,timeNow,timeLast=0;
int   wait = 5000;   //Tiempo de espera para madar mensajes por MQTT
char  dataString[8]; //Define una arreglo de caracteres para enviarlos por MQTT
float TemperaturaAlta=30; //Parámetro de inicio que puede alterar el usuario
int   HumedadAlta=70;     //Parámetro de inicio que puede alterar el usuario

// -----------Inicio de void setup ()-------------
void setup() {
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
 
     pinMode (LEDA,    OUTPUT); // Configura el pin del led como salida de voltaje (Simula Aire acondicionado)
     pinMode (LEDW,    OUTPUT); // Configura el pin del led como salida de voltaje (Simula Ventanas)
     pinMode (MQ135Pin,INPUT);  // Configura el Pin del MQ135 como entrada de voltaje
     pinMode (MQ6Pin,  INPUT);  // Configura el Pin del MQ6 como entrada de voltaje
     pinMode (MQ7Pin,  INPUT);  // Configura el Pin del MQ7 como entrada de voltaje
     dht.begin();               // Inicialización de la comunicación con el sensor DHT11
  
  // Conexión con el broker MQTT
     client.setServer(server, 1883); // Conectarse a la IP del broker en el puerto indicado
     client.setCallback(callback);   // Activar función de CallBack, permite recibir mensajes MQTT y ejecutar funciones a partir de ellos
     delay(1500);                    // Esta espera es preventiva, espera a la conexión para no perder información
     timeLast=millis();              //Para iniciar el control del tiempo
     client.publish(topicMaxTemp,"30"); // Temperatura máxima de inicio y que cambiará el usuario
     client.publish(topicMaxHum, "70"); // Humedad máxima de inicio y que cambiará el usuario
} // -------------Final de void setup ()-------------

// -------------Cuerpo del programa - Se ejecuta constamente------
void loop() {// Inicio de void loop
   // put your main code here, to run repeatedly:
      while (WiFi.status() == WL_CONNECTED) { //Verificar siempre que haya conexión al broker
         if (!client.connected()) {
            Serial.println("No esta conectado al broker");
            reconnect();  // En caso de que no haya conexión, ejecutar la función de reconexión, definida despues del void setup ()
            } // fin del if (!client.connected())
         client.loop();   // Esta función es muy importante, ejecuta de manera no bloqueante las funciones necesarias para la comunicación con el broker
         leeSensor();     //En esta función se lee sensores y publica en sus temas
      }
  } 
// -------------Fin de void loop -----------------------------


// -------------Funciones del usuario ------------------------

 void leeSensor ()
 {
    timeNow= millis();
    if (timeNow - timeLast > wait) {
      timeLast = timeNow;            // Actualización de seguimiento de tiempo no bloqueante
      Serial.println(F("-----"));
    
     /*   T E M P E R A T U R A   */ 
      //Primero lee y manda Temperatura
      t = dht.readTemperature();    // Read temperature as Celsius (the default)
      dtostrf(t, 1, 2, dataString);
      Serial.print(F("La temperatura publicada: "));      
      Serial.print(dataString);
      Serial.print(F(" °C , "));
      Serial.print(F(" En el tópico: "));
      Serial.println(topicTemp);
      client.publish(topicTemp, dataString); // Envía los datos por MQTT
      delay(100);

      /*   H U M E D A D   */ 
      //Segundo lee y manda Humedad
      h = dht.readHumidity();      // Read Humidity
      dtostrf(h, 1, 2, dataString);
      Serial.print(F("La humedad publicada    : "));
      Serial.print(dataString);
      Serial.print(F(" hum, En el tópico: "));
      Serial.println(topicHum);
      client.publish(topicHum, dataString); // Envía los datos por MQTT
      
      /*   A L C O H O L  MQ135 : 1) No hay   0) Si hay */
      //Tercero lee y manda Ausencia/Presencia de alcohol
      boolean m135 = digitalRead(MQ135Pin);    // Read Alcohol 0, hay.  1 no hay
      m135 = (!m135);                          //Se niega solo por lógica de representación en Dashboard
      dtostrf(m135, 1, 2, dataString);
      Serial.print(F("El Alcohol publicado(mq135): "));      
      Serial.print(m135);
       if(m135)  //si la salida del sensor es 1 
         Serial.print(",  Sin presencia de alcohol");
        else  //si la salida del sensor es 0 
         Serial.print(",  Alcohol detectado : ");    
      Serial.print(dataString);
      Serial.print(F(",  En el tópico: "));
      Serial.println(topicMQ135);
      client.publish(topicMQ135, dataString); // Envía los datos por MQTT
      delay(100);

      /*   G A S   L P    MQ6  : 1) No hay   0) Si hay */
      //Tercero lee y manda Ausencia/Presencia de alcohol
      boolean m6 = digitalRead(MQ6Pin);    // Read Gas LP 0, hay.  1 no hay
      m6=(!m6);                            //Se niega solo por lógica de representación en Dashboard
      dtostrf(m6, 1, 2, dataString);
      Serial.print(F("El GAS publicado    (mq6)  : "));      
      Serial.print(m6);
       if(m6) //si la salida del sensor es 1 
         Serial.print(",  Sin presencia de Gas LP");
        else //si la salida del sensor es 0 
         Serial.print(",  GAS LP detectado : ");     
      Serial.print(dataString);
      Serial.print(F(", En el tópico: "));
      Serial.println(topicMQ6);
      client.publish(topicMQ6, dataString); // Envía los datos por MQTT
      delay(100);

     /*   Monóxido   CO    MQ7  : 1) A salvo   0) Peligro */
      //Tercero lee y manda Ausencia/Presencia de alcohol
      boolean m7 = digitalRead(MQ7Pin);     // Read CO, si es 0 hay Peligro. Si es 1 se está a salvo
      m7=(!m7);                             //Se niega solo por lógica de representación en Dashboard
      dtostrf(m7, 1, 2, dataString);
      Serial.print(F("El CO publicado     (mq7)  : "));      
      Serial.print(m7);
       if(m7) //si la salida del sensor es 1 
          Serial.print(",  Sin presencia dañina de CO");
        else //si la salida del sensor es 0 
          Serial.print(",  nivel de CO dañino detectado :");     
      Serial.print(dataString);
      Serial.print(F(", En el tópico: "));
      Serial.println(topicMQ7);
      client.publish(topicMQ7, dataString); // Esta es la función que envía los datos por MQTT
      delay(100);

      if (isnan(t)) {
        Serial.println(F("Failed to read TEMPERATURE from DHT sensor!")); return;}
      if (isnan(h)) {
        Serial.println(F("Failed to read HUMIDITY from DHT sensor!")); return;}
      if (isnan(m135)) {
        Serial.println(F("Failed to read from MQ135 sensor!")); return;}
      if (isnan(m6)) {
        Serial.println(F("Failed to read from MQ6 sensor!")); return;}
      if (isnan(m7)) {
        Serial.println(F("Failed to read from MQ7 sensor!")); return;}
     Serial.print("Temperatura Máxima= ");
     Serial.print(TemperaturaAlta);
     Serial.print(",  Humedad Máxima= ");
     Serial.println(HumedadAlta);
     
     // LOGICA DE ATUACIONES SEGÚN TABLA DE ENCABEZADO
     if ( m135 || m6 || m7 ) {
      Serial.println("Se cumple: (m135||m6||m7)");
      apagaAirConditioner();
      abreWindows();
      } else
      if ((h>=HumedadAlta)){
        apagaAirConditioner();
        abreWindows();
      } else
      if ((t>=TemperaturaAlta) && (h<HumedadAlta)){
      Serial.println("Se cumple: (t>=TemperaturaAlta) && (h<HumedadAlta)");  
      cierraWindows();
      activaAirConditioner();  
      } else
      if ((t<TemperaturaAlta) && (h<HumedadAlta)){
        Serial.println("Se cumple: (t<TemperaturaAlta) && (h<HumedadAlta)");
        cierraWindows();
        apagaAirConditioner();
      }
      Serial.println();
      }// Fin del IF Time Wait
  }// -----------Fin de void leeSensor() ------


 void activaAirConditioner ()  //Para activar el Aire Acondicionado Automático
 {     
  digitalWrite (LEDA, HIGH);
  client.publish(topicAirC, "1"); // Envía los datos por MQTT
  }

  void apagaAirConditioner ()  //Para apagar el Aire Acondicionado Automático
 {
  digitalWrite (LEDA, LOW);
  client.publish(topicAirC, "0"); // Envía los datos por MQTT
  }
 
  void abreWindows ()         //Para abrir Ventanas Automáticas
 {
      digitalWrite (LEDW, HIGH);
      client.publish(topicWin, "1"); // Envía los datos por MQTT
 }
 // -----------Fin de void activaVentanas() ------
 
 void cierraWindows ()         //Para cerrar Ventanas Automáticas
 {
      digitalWrite (LEDW, LOW);
      client.publish(topicWin, "0"); // Envía los datos por MQTT
 }


// Esta función permite tomar acciones en caso de que se reciba un mensaje correspondiente a un tema al cual se hará una suscripción
  void callback(char* topic, byte* message, unsigned int length) {

  // Concatenar los mensajes recibidos para conformarlos como una varialbe String
  String messageTemp; // Se declara la variable en la cual se generará el mensaje completo  
  for (int i = 0; i < length; i++) {  // Se imprime y concatena el mensaje
    //Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

//Se comprueba que el mensaje se haya concatenado correctamente
  /*  Serial.print("topic :");
    Serial.println(topic);
    Serial.print ("Mensaje concatenado en una sola variable: ");
    Serial.println (messageTemp);
    Serial.println();
  */

  // En esta parte puedes agregar las funciones que requieras para actuar segun lo necesites al recibir un mensaje MQTT
  if (String(topic) == topicMaxTemp) {  // En caso de recibirse mensaje en el tema CodigoIoT/SGCIndustrial/ValoresCliente/LimitTemp
      Serial.println("Usuario cambió Temperatura máxima: ");
      Serial.println (messageTemp);
      TemperaturaAlta= messageTemp.toFloat();
  }// fin del if topic
  if (String(topic) == topicMaxHum) {  // En caso de recibirse mensaje en el tema CodigoIoT/SGCIndustrial/ValoresCliente/LimitHum
      Serial.println("Usuario cambió Humedad máxima: ");
      Serial.println (messageTemp);
      HumedadAlta= messageTemp.toInt();
  }// fin del if topic
  
}// ---------------fin del void callback---------------


// --------------- Función para reconectarse------------------
void reconnect() {     // Bucle hasta lograr conexión
  Serial.println("Entre a la función reconect");
  while (!client.connected()) { // Pregunta si hay conexión
    Serial.print("Tratando de contectarse...");
    // Intentar reconexión
    if (client.connect("espClient")) { //Pregunta por el resultado del intento de conexión
      Serial.println("Conectado");
      client.subscribe(topicTemp);   // Suscripción al tema temperatura
      client.subscribe(topicHum);    // Suscripción al tema humedad
      client.subscribe(topicAirC);   // Suscripción al tema Aire Acondicionado
      client.subscribe(topicMQ6);    // Suscripción al tema GAS LP
      client.subscribe(topicMQ7);    // Suscripción al tema CO
      client.subscribe(topicMQ135);  // Suscripción al tema Alcohol
      client.subscribe(topicMaxTemp);// Temperatura máxima dada por el usuario
      client.subscribe(topicMaxHum); // Humedad máxima dada por el usuario
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
}// --------------fin de void reconnect-----------------------
