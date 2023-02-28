# Arduino Homeware IO [ESP8266 /  ESP32]

Homeware é uma biblioteca para a criação de aplicativos de automação em Arduino. Ele permite controlar vários dispositivos, como lâmpadas, termostatos, através de uma API RESTful [http://ip:porta/cmd?q=commando].

Na primeira carga do dispositivo um ponto de acesso (AP) para se conectar [nnnnnn.local], ao fazer uma conexão estará disponível em  [http://192.168.4.1:80] o serviço que permitirá indicar o roteador WiFi a ser utilizado na rede local como estação.

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
 *   set sleep 60 (estabele 60s para deepsleep - depende de conectar RST ao D0)
</pre>



# Contribuindo
Se você gostaria de contribuir para o Homeware para Arduino, por favor, crie uma nova issue descrevendo o que você gostaria de adicionar ou consertar. Em seguida, crie um fork do repositório e faça suas alterações em um branch separado. Quando terminar, abra um pull request para revisão.

# Licença
Homeware é licenciado sob a Licença MIT. Veja o arquivo LICENSE para mais detalhes.
