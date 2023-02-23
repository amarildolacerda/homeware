# HomewareIO
Arduino Homeware IO [ESP8266 /  ESP32]

Na primeira carga do dispositivo será inciado um roteador para se conectar, fazendo a conexão o servidor estará disponível em  [http://192.168.4.1:80] onde permitirá indicar o roteador WiFi a ser utilizado.

---


# Commands
Os comandos são enviados por TELNET na porta 23. Para ajudar nos principais itens de HELP é possivel pedir um "help" no terminal que irá mostrar os principais itens a serem utilizados;
Utilizar os comandos TELNET para fazer as configurações inciais do dispositivo:
* gpio 4 mode in  [diz que o pino 4 é um pino de aquisição]
* gpio 15 mode out [indica que o pino 15 é um acionamento (saída)]
* gpio 4 trigger 15 monostable [indica que ao acionar o 4 é para disparar um evento para o pino 15 com mesmo valor de acionamento]
* gpio 4 trigger 15 bistable [indica que dispara um evento somente quando o valor do pino 4 for HIGH, ignora os LOW]
* gpio 15 device onoff [fazer um registro de integração externa padrão "onoff" - usado na alexa]
* save [guarda as configurações]
   
<pre>
 *   show config 
 *   gpio [pin] mode [in,out,adc,lc,ldr]
 *   gpio [pin] default [n](usado no setup inicial)
 *   gpio [pin] mode gus (opcional groove ultrasonic)
 *   gpio [pin] trigger [pin] [monostable,bistable]
 *   gpio [pin] device [onoff,dimmable,motion] (usado na alexa)
 *   set app_key [x] (SINRIC)
 *   set app_secret [x] (SINRIC)
 *   gpio [pin] sensor [deviceId] (SINRIC)
 *   gpio [pin] get
 *   gpio [pin] set [n]
 *   set interval 500
 *   set adc_min 511
 *   set adc_max 512
</pre>

# 
