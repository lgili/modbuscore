#include "uart_windows.h"
#include <stdio.h>
#include <modbus/utils.h>

DWORD WINAPI uart_listener_thread(LPVOID lpParam);

int uart_init(uart_handle_t *uart, const char *port_name, int baud_rate) {
    char commName[50] = {0};
    sprintf_s(commName, sizeof(commName), "\\\\.\\%s", port_name);

    uart->hSerial = CreateFileA(
        commName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (uart->hSerial == INVALID_HANDLE_VALUE) {
        printf("[Error] Unable to open %s.\n", port_name);
        return -1;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(uart->hSerial, &dcbSerialParams)) {
        printf("[Error] Failed to get COM state.\n");
        CloseHandle(uart->hSerial);
        return -1;
    }

    dcbSerialParams.BaudRate = baud_rate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;

    if (!SetCommState(uart->hSerial, &dcbSerialParams)) {
        printf("[Error] Failed to set COM parameters.\n");
        CloseHandle(uart->hSerial);
        return -1;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = 50;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(uart->hSerial, &timeouts)) {
        printf("[Error] Failed to set COM timeouts.\n");
        CloseHandle(uart->hSerial);
        return -1;
    }

    // Configura máscara de eventos para monitorar dados recebidos
    if (!SetCommMask(uart->hSerial, EV_RXCHAR)) {
        printf("[Error] Failed to set communication mask.\n");
        CloseHandle(uart->hSerial);
        return -1;
    }

    uart->hThread = NULL;
    uart->onDataReceived = NULL;
    uart->stopThread = FALSE;

    printf("[Info] COM port %s opened and configured successfully.\n", port_name);
    return 0;
}


// int uart_write(uart_handle_t *uart, const uint8_t *data, uint16_t length) {
//     printf("[Info] Iniciando uart_write: %d bytes", length);
//     DWORD bytesWritten;
//     BOOL status = WriteFile(uart->hSerial, data, length, &bytesWritten, NULL);
//     printf("[Info] WriteFile retornou: %d, bytesWritten: %lu", status, bytesWritten);
//     if (!status) {
//         printf("[Error] Failed to write to COM port.\n");
//         return -1;
//     }

//     printf("[Info] Sent %lu bytes to COM port.\n", bytesWritten);
//     return (int)bytesWritten;
// }
int uart_write(uart_handle_t *uart, const uint8_t *data, uint16_t length) {
    LOG(LOG_LEVEL_DEBUG, "Iniciando uart_write: %d bytes", length);
    DWORD bytesWritten;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        LOG(LOG_LEVEL_ERROR, "Falha ao criar evento Overlapped.");
        return -1;
    }

    BOOL status = WriteFile(uart->hSerial, data, length, &bytesWritten, &overlapped);
    if (!status) {
        if (GetLastError() != ERROR_IO_PENDING) {
            LOG(LOG_LEVEL_ERROR, "Falha ao escrever na COM port.");
            CloseHandle(overlapped.hEvent);
            return -1;
        }

        // Espera a operação de escrita completar
        status = GetOverlappedResult(uart->hSerial, &overlapped, &bytesWritten, TRUE);
        if (!status) {
            LOG(LOG_LEVEL_ERROR, "Falha na operação de escrita Overlapped.");
            CloseHandle(overlapped.hEvent);
            return -1;
        }
    }

    LOG(LOG_LEVEL_INFO, "Enviou %lu bytes para a COM port.", bytesWritten);
    CloseHandle(overlapped.hEvent);
    return (int)bytesWritten;
}

int uart_read(uart_handle_t *uart, uint8_t *buffer, uint16_t length) {
    // LOG(LOG_LEVEL_DEBUG, "Iniciando uart_read: %d bytes", length);
    DWORD bytesRead;
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        LOG(LOG_LEVEL_ERROR, "Falha ao criar evento Overlapped.");
        return -1;
    }

    BOOL status = ReadFile(uart->hSerial, buffer, length, &bytesRead, &overlapped);
    if (!status) {
        if (GetLastError() != ERROR_IO_PENDING) {
            LOG(LOG_LEVEL_ERROR, "Falha ao ler da COM port.");
            CloseHandle(overlapped.hEvent);
            return -1;
        }

        // Espera a operação de leitura completar
        status = GetOverlappedResult(uart->hSerial, &overlapped, &bytesRead, TRUE);
        if (!status) {
            LOG(LOG_LEVEL_ERROR, "Falha na operação de leitura Overlapped.");
            CloseHandle(overlapped.hEvent);
            return -1;
        }
    }

    if(bytesRead > 0)
        LOG(LOG_LEVEL_DEBUG, "Lido %lu bytes da COM port.", bytesRead);
    CloseHandle(overlapped.hEvent);
    return (int)bytesRead;
}

// int uart_read(uart_handle_t *uart, uint8_t *buffer, uint16_t length) {
//     DWORD bytesRead;
//     BOOL status = ReadFile(uart->hSerial, buffer, length, &bytesRead, NULL);
//     if (!status) {
//         printf("[Error] Failed to read from COM port.\n");
//         return -1;
//     }

//     if (bytesRead > 0) {
//         printf("[Info] Received %lu bytes from COM port.\n", bytesRead);
//     }

//     return (int)bytesRead;
// }

void uart_close(uart_handle_t *uart) {
    if (uart->hThread) {
        // Sinaliza a thread para encerrar
        uart->stopThread = TRUE;

        // Envia um evento para desbloquear a thread se estiver esperando
        EscapeCommFunction(uart->hSerial, CLRDTR);

        // Espera pela thread encerrar
        WaitForSingleObject(uart->hThread, INFINITE);

        CloseHandle(uart->hThread);
        uart->hThread = NULL;
    }
    if (uart->hSerial != INVALID_HANDLE_VALUE) {
        CloseHandle(uart->hSerial);
        uart->hSerial = INVALID_HANDLE_VALUE;
        printf("[Info] COM port closed.\n");
    }
}

int uart_set_callback(uart_handle_t *uart, int (*callback)(uint8_t data)) {
    uart->onDataReceived = callback;

    // Cria uma thread para monitorar a UART
    if (!uart->hThread) {
        uart->hThread = CreateThread(NULL, 0, uart_listener_thread, uart, 0, NULL);
        if (!uart->hThread) {
            printf("[Error] Failed to create UART listener thread.\n");
            return -1;
        }
    }

    return 0;
}

DWORD WINAPI uart_listener_thread(LPVOID lpParam) {
    uart_handle_t *uart = (uart_handle_t *)lpParam;
    DWORD eventMask;
    uint8_t buffer[256]; // Buffer para armazenar múltiplos bytes
    DWORD bytesRead;

    while (!uart->stopThread) {
        if (WaitCommEvent(uart->hSerial, &eventMask, NULL)) {
            if (eventMask & EV_RXCHAR) {
                // Ler todos os bytes disponíveis na UART
                while (ReadFile(uart->hSerial, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                    // Processar cada byte no buffer
                    for (DWORD i = 0; i < bytesRead; i++) {
                        if (uart->onDataReceived) {
                            uart->onDataReceived(buffer[i]); // Chama o callback para cada byte
                        }
                    }
                }
            }
        } else {
            // Opcional: implementar tratamento de erros ou sair se necessário
            // Exemplo: verificar erros de comunicação e encerrar a thread
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING) {
                printf("[Error] WaitCommEvent failed with error: %lu\n", error);
                break;
            }
        }
    }

    return 0;
}

// DWORD WINAPI uart_listener_thread(LPVOID lpParam) {
//     uart_handle_t *uart = (uart_handle_t *)lpParam;
//     DWORD eventMask;
//     uint8_t buffer[256]; // Buffer para armazenar múltiplos bytes
//     DWORD bytesRead;

//     while (1) {
//         if (WaitCommEvent(uart->hSerial, &eventMask, NULL)) {
//             if (eventMask & EV_RXCHAR) {
//                 // Ler todos os bytes disponíveis na UART
//                 while (ReadFile(uart->hSerial, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
//                     // Processar cada byte no buffer
//                     for (DWORD i = 0; i < bytesRead; i++) {
//                         if (uart->onDataReceived) {
//                             uart->onDataReceived(buffer[i]); // Chama o callback para cada byte
//                         }
//                     }
//                 }
//             }
//         }
//     }
//     return 0;
// }
