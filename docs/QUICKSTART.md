# ModbusCore v1.0 - Quick Start Guide

## Compilar e Testar (5 minutos)

### Passo 1: Compilar

```bash
cd "/Users/lgili/Documents/01 - Codes/02 - Empresa/02 - Modbus lib"

# Configure CMake
cmake -B build -S .

# Build
cmake --build build

# Verificar que tudo compilou
ls build/libmodbuscore.a   # Biblioteca
ls build/tests/            # Testes e exemplos
```

### Passo 2: Rodar Testes Unitários

```bash
cd build/tests

# Runtime tests
./runtime_smoke
✓ 5/5 passed

# Transport interface tests
./transport_iface_smoke
✓ 4/4 passed

# Mock transport tests
./mock_transport_smoke
✓ 6/6 passed

# RTU UART driver tests
./rtu_uart_smoke
✓ 4/4 passed

# POSIX RTU driver tests
./posix_rtu_smoke
✓ 3/3 passed

# Winsock TCP tests (retorna UNSUPPORTED fora do Windows)
./winsock_tcp_smoke
✓ Unsupported (OK em ambientes não-Windows)

# Engine resilience (parciais/falhas simuladas)
./engine_resilience_smoke
✓ 3/3 passed

# Protocol engine tests
./protocol_engine_smoke
✓ 8/8 passed

# PDU codec tests
./pdu_smoke
✓ 5/5 passed

# TCP driver tests (sem servidor, ok)
./posix_tcp_smoke
✓ 4/4 passed (skips integration if no server)
```

**Resultado esperado**: Todos os testes passam! 🎉

### Passo 3: Testar Integração End-to-End

**Terminal 1 - Iniciar servidor mock:**
```bash
cd tests

# Instalar dependência (uma vez)
pip3 install pymodbus

# Rodar servidor
python3 mock_modbus_server.py
```

Saída:
```
=== Mock Modbus TCP Server ===
Starting server on 127.0.0.1:5502...
✓ Server ready!
  Address: 127.0.0.1:5502
  Holding Registers: 0-99 (values = address)
  Press Ctrl+C to stop
```

**Terminal 2 - Rodar cliente:**
```bash
cd build/tests
./example_tcp_client_fc03
```

Saída esperada:
```
=== ModbusCore v1.0 - TCP Client Example (FC03) ===

Step 1: Creating TCP transport...
✓ Connected to 127.0.0.1:5502

Step 2: Building runtime with DI...
✓ Runtime initialized

Step 3: Initializing protocol engine (client mode)...
✓ Engine ready

Step 4: Building FC03 request (unit=1, addr=0, count=10)...
✓ Request encoded (6 bytes)
  Sending request...
✓ Request submitted

Step 5: Polling for response (max 100 iterations)...
✓ Response received!

Step 6: Parsing response...
  Registers read:
    [0] = 0x0000 (0)
    [1] = 0x0001 (1)
    ...
    [9] = 0x0009 (9)

=== SUCCESS ===
```

**Ou use o script automatizado:**
```bash
cd tests
chmod +x run_integration_test.sh
./run_integration_test.sh

# Output:
✓✓✓ INTEGRATION TEST PASSED ✓✓✓
```

### Passo 4 (Opcional): Exemplo RTU Bare-metal

Compile o esqueleto de cliente RTU e adapte os callbacks para a sua plataforma:

```bash
cd build/tests
./example_rtu_client
```

O código vive em `tests/example_rtu_client.c` e mostra como montar o driver `mbc_rtu_uart_create()` com guard-times automáticos e injetar o engine RTU.

---

## Usar a Biblioteca no Seu Projeto

### Opção 1: CMake (Recomendado)

```cmake
# CMakeLists.txt do seu projeto
add_subdirectory(path/to/modbuscore)

add_executable(my_app main.c)
target_link_libraries(my_app PRIVATE ModbusCore::modbuscore)
```

### Opção 2: Manualmente

```bash
# Compilar seu app
gcc -o my_app main.c \
    -I/path/to/modbuscore/include \
    -L/path/to/modbuscore/build \
    -lmodbuscore
```

---

## Exemplo Mínimo de Uso

```c
#include <modbuscore/transport/posix_tcp.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <stdio.h>

int main(void) {
    // 1. Criar transport TCP
    mbc_posix_tcp_config_t tcp_cfg = {
        .host = "192.168.1.100",
        .port = 502,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t transport;
    mbc_posix_tcp_ctx_t *tcp_ctx = NULL;

    if (mbc_posix_tcp_create(&tcp_cfg, &transport, &tcp_ctx) != MBC_STATUS_OK) {
        fprintf(stderr, "Connection failed\n");
        return 1;
    }

    // 2. Criar runtime com DI
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    mbc_runtime_builder_build(&builder, &runtime);

    // 3. Criar engine de protocolo
    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .response_timeout_ms = 3000,
    };
    mbc_engine_init(&engine, &engine_cfg);

    // 4. Construir e enviar request FC03
    mbc_pdu_t request;
    mbc_pdu_build_read_holding_request(&request, 1, 0, 10);  // unit=1, addr=0, qty=10

    uint8_t buffer[256];
    size_t len;
    mbc_pdu_encode(&request, buffer, sizeof(buffer), &len);
    mbc_engine_submit_request(&engine, buffer, len);

    // 5. Polling até receber resposta
    for (int i = 0; i < 100; i++) {
        mbc_engine_step(&engine, 10);

        mbc_pdu_t response;
        if (mbc_engine_take_pdu(&engine, &response)) {
            // Processar resposta
            const uint8_t *data;
            size_t count;
            mbc_pdu_parse_read_holding_response(&response, &data, &count);

            printf("Read %zu registers\n", count);
            for (size_t i = 0; i < count; i++) {
                uint16_t val = (data[i*2] << 8) | data[i*2+1];
                printf("  [%zu] = %u\n", i, val);
            }
            break;
        }
    }

    // 6. Cleanup
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(tcp_ctx);

    return 0;
}
```

**Compilar:**
```bash
gcc -o my_app my_app.c -I include -L build -lmodbuscore
./my_app
```

---

## Arquivos Importantes

| Arquivo | Descrição |
|---------|-----------|
| `PHASE4_COMPLETE.md` | Documentação completa da Fase 4A |
| `roadmap.md` | Roadmap geral do projeto |
| `tests/example_tcp_client_fc03.c` | Exemplo end-to-end comentado |
| `tests/mock_modbus_server.py` | Servidor mock para testes |
| `include/modbuscore/` | Headers públicos |
| `src/` | Implementação |

---

## Troubleshooting

### Erro: "Could not connect to 127.0.0.1:5502"
**Solução**: Rode o servidor mock primeiro (ver Passo 3)

### Erro: "pymodbus not installed"
**Solução**: `pip3 install pymodbus`

### Erro: "libmodbuscore.a not found"
**Solução**: Compile primeiro com `cmake --build build`

### Erro: "MBC_STATUS_TIMEOUT"
**Solução**: Aumente o timeout ou verifique se o servidor está respondendo

---

## Próximos Passos

1. ✅ **Você está aqui**: Fase 4A completa (POSIX TCP)
2. ⏳ **Opcional**: Adicionar mais function codes (FC01, FC02, FC04, FC05, etc.)
3. ⏳ **Opcional**: Driver RTU serial (bare-metal UART)
4. ⏳ **Fase 5**: DX & Tooling (mais exemplos, documentação)

---

**Dúvidas?** Leia `PHASE4_COMPLETE.md` para detalhes técnicos completos.
