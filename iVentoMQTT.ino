#include "WiFiEsp.h"          //Load a Wifi Library
#include "SoftwareSerial.h"   //Load a serial simulator for arduino digital pins.
#include <LiquidCrystal.h>    //Load a LCD library
#include <DHT.h>              //Load a DHT biblioteca 
#include <PubSubClient.h>     //Load a Biblioteca PubSubClient

//mapeamento de pinos do arduino uno
#define pinRelay    3
#define pinLCD01    4
#define pinLCD02    5
#define pinLCD03    8
#define pinLCD04    9
#define pinLCD05    10
#define pinLCD06    11
#define pinSerialRX 6
#define pinSerialTX 7

#define pinDHT 2 // what pin we're connected to
#define DHTTYPE DHT22 // DHT 22  (AM2302)
DHT dht(pinDHT, DHTTYPE); // Initialize DHT sensor for normal 16mhz Arduino

//Define os pinos que serão ligados ao LCD
LiquidCrystal lcd(pinLCD01, pinLCD02, pinLCD03, pinLCD04, pinLCD05, pinLCD06);

//Array simbolo grau
byte grau[8] = { B00001100,
                 B00010010,
                 B00010010,
                 B00001100,
                 B00000000,
                 B00000000,
                 B00000000,
                 B00000000,
               };

SoftwareSerial Serial1(pinSerialRX, pinSerialTX); //Define os pinos que emulam a serial

//TODO: alterar os parametros abaixo para conexão na sua rede.
char ssid[] = "informe o nome da rede aqui";  //Aqui você deve informar o nome da rede onde o arduino irá se conectar.
char pass[] = "informe a senha aqui";         //Aqui, você deve informar a senha da rede onde o arduino irá se conectar.

int status = WL_IDLE_STATUS;  //STATUS TEMPORÁRIO ATRIBUÍDO QUANDO O WIFI É INICIALIZADO E PERMANECE ATIVO
                              //ATÉ QUE O NÚMERO DE TENTATIVAS EXPIRE (RESULTANDO EM WL_NO_SHIELD) OU QUE UMA CONEXÃO SEJA ESTABELECIDA
                              //(RESULTANDO EM WL_CONNECTED)

RingBuffer buf(8);            //BUFFER PARA AUMENTAR A VELOCIDADE E REDUZIR A ALOCAÇÃO DE MEMÓRIA

//Global Variables
float   hum;                  //Stores humidity value
float   temp;                 //Stores temperature value

String  txtLigaDesliga = "ON";
float   floatTempDesliga = 1;
float   floatTempLiga = 98;


// MQTT
//TODO: cada aparelho deve possuir uma identificação unica, alterar o nome do seu aparelho na linha abaixo.
#define ID_MQTT  "iVento001"     //id mqtt (para identificação de sessão)
#define TOPICO_SUBSCRIBE "iVentoRecebe"   //tópico MQTT de escuta
#define TOPICO_PUBLISH   "iVentoEnvia"    //tópico MQTT de envio de informações para Broker
const char* BROKER_MQTT = "test.mosquitto.org"; //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883; // Porta do Broker MQTT

WiFiEspClient espClient;      // Cria o objeto espClient
PubSubClient MQTT(espClient); // Instancia o Cliente MQTT passando o objeto espClient


//Prototypes
void initSerial();
void initWiFi();
void initMQTT();
void reconectWiFi(); 
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void VerificaConexoesWiFIEMQTT(void);


void readHumidityTemp(){
  
  //Intervalo recomendado para leitura do sensor de 2 segundos
  delay(2000);

  //Read data and store it to variables hum and temp
  hum = dht.readHumidity();
  temp = dht.readTemperature();

  //Print temp and humidity values to serial monitor
  //Serial.print("Humidity: ");
  //Serial.print(hum);
  //Serial.print(" %, Temp: ");
  //Serial.print(temp);
  //Serial.println(" Celsius");

  //Exibe os dados no display
  lcd.setCursor(0, 0);
  lcd.print("Temp : ");
  lcd.print(" ");
  lcd.setCursor(7, 0);
  lcd.print(temp, 1);
  lcd.setCursor(12, 0);

  //Mostra o simbolo do grau formado pelo array
  lcd.write((byte)0);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Umid : ");
  lcd.print(" ");
  lcd.setCursor(7, 1);
  lcd.print(hum, 1);
  lcd.setCursor(12, 1);
  lcd.print("%");

}

//Função: inicializa parâmetros de conexão MQTT(endereço do 
//        broker, porta e seta função de callback)
//Parâmetros: nenhum
//Retorno: nenhum
void initMQTT() 
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);   //informa qual broker e porta deve ser conectado
    MQTT.setCallback(mqtt_callback);            //atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
}

//Função: função de callback 
//        esta função é chamada toda vez que uma informação de 
//        um dos tópicos subescritos chega)
//Parâmetros: nenhum
//Retorno: nenhum
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    //Serial.println();
    String msg;
    String txtTempDesliga;
    String txtTempLiga;
    int    intCountPontoEVirgula;

    txtLigaDesliga = "";
    intCountPontoEVirgula = 0;
    //obtem a string do payload recebido
    for(int i = 0; i < length; i++) {
       char c = (char)payload[i];
       msg += c;
       if (c == ';') {
          intCountPontoEVirgula++;
       }else{
          if (intCountPontoEVirgula == 0) {
            txtLigaDesliga += c;
          }
          if (intCountPontoEVirgula == 1) {
            txtTempDesliga += c;
          }
          if (intCountPontoEVirgula == 2) {
            txtTempLiga += c;
          }
       }
    }
    Serial.println();
    Serial.println("Novos dados recebidos via MQTT.");
    Serial.print("Opção ON/OFF:");
    Serial.print(txtLigaDesliga);
    Serial.println(".");
    Serial.print("Temperatura para desligar:");
    Serial.print(txtTempDesliga);
    Serial.println(".");
    Serial.print("Temperatura para re-ligar:");
    Serial.print(txtTempLiga);
    Serial.println(".");

    floatTempDesliga = txtTempDesliga.toFloat();
    floatTempLiga = txtTempLiga.toFloat();
  
}

//Função: reconecta-se ao broker MQTT (caso ainda não esteja conectado ou em caso de a conexão cair)
//        em caso de sucesso na conexão ou reconexão, o subscribe dos tópicos é refeito.
//Parâmetros: nenhum
//Retorno: nenhum
void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("* Tentando se conectar ao Broker MQTT: ");
    Serial.println(BROKER_MQTT);
    if (MQTT.connect(ID_MQTT)){
      //Serial.println("Conectado com sucesso ao broker MQTT!");
      MQTT.subscribe(TOPICO_SUBSCRIBE); 
    } 
    else{
      Serial.println("Falha ao reconectar no broker.");
      Serial.println("Havera nova tentatica de conexao em 2s");
      delay(2000);
    }
  }
}

void EnviaEstadoOutputMQTT(void)
{
  char txtTemp[8];
  dtostrf(temp, 1, 2, txtTemp);
  MQTT.publish(TOPICO_PUBLISH, txtTemp);
 
  delay(1000);
}

//Função: verifica o estado das conexões WiFI e ao broker MQTT. 
//        Em caso de desconexão (qualquer uma das duas), a conexão
//        é refeita.
//Parâmetros: nenhum
//Retorno: nenhum
void VerificaConexoesWiFIEMQTT(void)
{
  if (!MQTT.connected()) 
    reconnectMQTT(); //se não há conexão com o Broker, a conexão é refeita
}

void setup(){
  dht.begin();              //Inicia o sensor DHT22
  lcd.begin(16, 2);         //Inicializa LCD
  lcd.clear();              //Limpa o LCD
                            //Cria o caractere customizado com o simbolo do grau
  lcd.createChar(0, grau);
  pinMode(pinRelay, OUTPUT);    //DEFINE O PINO COMO SAÍDA (PINO 3 --> Relê)
  digitalWrite(pinRelay, LOW);  //PINO 3 INICIA DESLIGADO (o pino desligado deixa corrente passar)
  Serial.begin(9600);           //INICIALIZA A SERIAL
  Serial1.begin(9600);          //INICIALIZA A SERIAL PARA O ESP8266
  WiFi.init(&Serial1);          //INICIALIZA A COMUNICAÇÃO SERIAL COM O ESP8266

  //INÍCIO - VERIFICA SE O ESP8266 ESTÁ CONECTADO AO ARDUINO e CONECTA A REDE SEM FIO
  if(WiFi.status() == WL_NO_SHIELD){
    while (true);
  }
  while(status != WL_CONNECTED){
    status = WiFi.begin(ssid, pass);
  }
  initMQTT();
}

void loop(){
  
  //garante funcionamento das conexões WiFi e ao broker MQTT
  VerificaConexoesWiFIEMQTT();

  readHumidityTemp();

  //Se o aparelho já está ligado
  if (digitalRead(pinRelay) == LOW){
    if (  txtLigaDesliga == "OFF" 
       or (   txtLigaDesliga == "AUT" 
          and temp <= floatTempDesliga))
          digitalWrite(pinRelay, HIGH);
  //Se o aparelho está desligado
  }else{
    if (  txtLigaDesliga == "ON"
       or (   txtLigaDesliga == "AUT" 
          and temp >= floatTempLiga))
          digitalWrite(pinRelay, LOW);
  }

  //keep-alive da comunicação com broker MQTT
  MQTT.loop();

  //envia o status de todos os outputs para o Broker no protocolo esperado
  EnviaEstadoOutputMQTT();  
 
}
