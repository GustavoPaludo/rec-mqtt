Como configurar:

O Broket MQTT usado foi o MQTTX. Nele, será criado um novo tópico, usuário e senha.
No código fonte da aplicação, as constantes ssdi deve ser trocada para o nome da rede que deseja se conectar e o password a senha dessa rede.
O mqtt_broker e o mqtt_port são os valores padrão de quando se é criada uma nova conexão no MQTTX.
O tópico deverá ter o mesmo nome que o criado no MQTTX, bem como o usuário e senha.

O rssi_ref (RSSI referência para 1 metro) e a constante N (atenuador de sinal) podem variar dependendo do ambiente de testes.
