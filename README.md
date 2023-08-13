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
 *   gpio [pin] mode [in,out,pwm,adc,lc,ldr,led,dht,rst,srn]
 *   gpio [pin] default [n](usado no setup inicial)
 *   gpio [pin] mode gus (opcional groove ultrasonic)
 *   gpio [pin] trigger [pinTrigger] [monostable,bistable] -> indica que [pin] ira acionar [pinTrigger] sempre que houver uma mudança de estado de [pin]
 *   gpio [pin] device [onoff,dimmable,motion, dht, ldr] (usado na alexa)
 *   set app_key [x] (SINRIC)
 *   set app_secret [x] (SINRIC)
 *   gpio [pin] sensor [deviceId] (SINRIC)
 *   gpio [pin] get
 *   gpio [pin] set [n]
 *   pwm [pin] set [value] timeout [x] // gera um pulso pwm por um tempo>0 (timeout=0, deixa ligado). Value=0: desliga e maior liga/timeout
 *   set interval 500
 *   set adc_min 511
 *   set adc_max 512
 *   set sleep 60 (estabele 60s para deepsleep - depende de conectar RST ao D0)
 *   gpio [pin] timer 60000 // duração para a ação antes de desligar de 60000s ( valor==0 - modo não temporizado)
 *   gpio [pin] interval 1000 // entre "timer" para uma ação (ex: led piscando);
 [scene] // depende do mqtt ativo
 *   scene [nome] set 1/0; scene [nome] none; 
 *   scene [nome] trigger [pin] //config: acao quando recebe um evento scene [nome] executa o [pin]
 *   gpio [pin] scene [noite] //config: gera um evento de scene - quando o pin for alterado enviar scene [nome]
 </pre>

Uma trigger "bistable" indica que é para funcionar como um switch (troca o estado quando seu acionador estiver ligado-HIGH ). Uma trigger monostable aciona com o mesmo valor do seu acionador, se ligado - liga, se desligado - deliga;


# Drivers
## Ultrosonic (gus)
O driver ultrasonic detect distância de um objeto. O Groover Ultrasonic detecta até 4 metros de distância como informa a documentação.
* gpio <pin> mode gus - ativa o Goover Ultrosonic para o pin
* set gus_interval <ms> - indicar o intervalo de tempo de varredura (1000)
* set gus <n> - indicar o valor de corte para gerar o evento  ( valor < n>)? HIGH : LOW;

## DHT11 - leitura de temperatura (Celsius)
O DHT11 é um sensor de leitura de temperatura e umidade. É possivel gera gatilhos com base na temperatura.
* gpio <pin> mode dht -> ativa o sensor
* set dht_interval <ms> -> indica o intervalo de leitura  (1000)
* set dht_min <graus>  - indica faixa interior para gerar evento HIGN
* set dht_max <graus>  - indica faixa superior para gerar evento HIGH
*    para fora da faixa o gatilho será LOW

## LDR - leitura de luminosidade
O LDR mede a intensidade de luz do ambiente.
* gpio <pin> mode ldr -> ativa o LDR para leitura em uma pino ADC;
* set ldr_interval <ms> -> indica o intervalo de varredura (60000)
* set ldr_min <n>  -> menor taxa de luminosidade para disparar o gatilho HIGH
* set ldr_max <n>  -> limite superior para disparar o gatilho HIGH
*   nas faixa intermediarias o gatilho será em LOW

## SRN - Sirene
O SRN é um driver de acionamento de sirene com liga e desliga intermitente
* gpio <pin> mode srn -> ativa modo sirene
* set srn_interval <ms> -> indica o intervalo entre liga e desliga (1000)
* set srn_duration <ms> -> indica o tempo que ira desligar automatico, "0" indica desligamento manual (60000)

Uma sirene pode ser combinado com outro evento que dispara ou desliga a sirene ex: detecta o nivel de agua e dispara a sirene   [gpio 2 trigger 4 bistable]
# Contribuindo
Se você gostaria de contribuir para o Homeware para Arduino, por favor, crie uma nova issue descrevendo o que você gostaria de adicionar ou consertar. Em seguida, crie um fork do repositório e faça suas alterações em um branch separado. Quando terminar, abra um pull request para revisão.

# Licença
Homeware é licenciado sob a Licença MIT. Veja o arquivo LICENSE para mais detalhes.
