# Quick RTU Test Guide

## 🚀 Teste Rápido em 3 Terminais

### Terminal 1: Criar Portas Virtuais
```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"
./scripts/create_virtual_serial.sh
```

**Deixe este terminal rodando!** Você verá:
```
Creating virtual serial port pair...
  Server side: /tmp/modbus_server
  Client side: /tmp/modbus_client

Press Ctrl+C to stop

2025/XX/XX socat[XXXXX] N PTY is /dev/ttysXXX
2025/XX/XX socat[XXXXX] N PTY is /dev/ttysXXX
```

### Terminal 2: Iniciar Servidor RTU
```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"
./build/examples/modbus_rtu_server --device /tmp/modbus_server --baud 9600 --unit 17
```

Deve aparecer:
```
Modbus RTU server ready (unit 0x11, baud 9600)
Waiting for master requests...
```

### Terminal 3: Testar com Cliente
```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"
./build/examples/modbus_rtu_client --device /tmp/modbus_client --baud 9600 --unit 17
```

**Comandos para testar:**
```
> read 0 10          # Lê 10 registradores começando em 0
> write 5 1234       # Escreve 1234 no registrador 5
> read 5 1           # Lê o registrador 5 (deve retornar 1234)
> quit
```

## ⚡ Teste Automatizado (Tudo em um)

```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"
./scripts/test_rtu_server.sh
```

Este script:
1. Cria as portas virtuais automaticamente
2. Inicia o servidor RTU
3. Aguarda conexões

Em outro terminal, rode o cliente normalmente.

## ❗ Troubleshooting

### Erro: "Permission denied"
```bash
# Limpe tudo e recomece
pkill -f socat
rm -f /tmp/modbus_server /tmp/modbus_client
```

### Erro: "status=-4" (IO_ERROR)
- Certifique-se que o Terminal 1 (socat) está rodando
- Verifique se os links existem: `ls -l /tmp/modbus_*`
- Aguarde 2-3 segundos após iniciar o socat

### Verificar se as portas foram criadas
```bash
ls -la /tmp/modbus_*
```

Deve mostrar:
```
/tmp/modbus_client -> /dev/ttysXXX
/tmp/modbus_server -> /dev/ttysXXX
```

## 📝 Exemplo Completo de Sessão

```
Terminal 1:
$ ./scripts/create_virtual_serial.sh
Creating virtual serial port pair...
[socat running...]

Terminal 2:
$ ./build/examples/modbus_rtu_server --device /tmp/modbus_server --baud 9600 --unit 17
Modbus RTU server ready (unit 0x11, baud 9600)
Waiting for master requests...
Received RTU function 0x03
Received RTU function 0x06
Received RTU function 0x03

Terminal 3:
$ ./build/examples/modbus_rtu_client --device /tmp/modbus_client --baud 9600 --unit 17
Modbus RTU client ready (unit 0x11, baud 9600)
Type 'help' for available commands

> read 0 5
Request sent (FC03: address=0, count=5)
Response received (5 registers):
  [0] = 0 (0x0000)
  [1] = 1 (0x0001)
  [2] = 2 (0x0002)
  [3] = 3 (0x0003)
  [4] = 4 (0x0004)

> write 10 999
Request sent (FC06: address=10, value=999)
Write confirmed

> read 10 1
Request sent (FC03: address=10, count=1)
Response received (1 registers):
  [0] = 999 (0x03E7)

> quit
Shutting down...
```

## ✅ Sucesso!

Se você conseguiu executar os comandos acima e ver as respostas, o servidor e cliente RTU estão funcionando perfeitamente!
