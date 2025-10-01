import serial
import time
import threading
import queue
import csv
from datetime import datetime


# Parâmetros da porta serial - ajuste conforme necessário
PORT = 'COM3'  # Exemplo em Linux
BAUDRATE = 19200
BYTESIZE = 8
PARITY = 'E'
STOPBITS = 1
TIMEOUT = 0.05  # Curto para leitura não bloqueante

# Tempo máximo ocioso para considerar um novo frame (em segundos)
FRAME_GAP = 0.035
# # Dicionário global (ou mantido no escopo apropriado) para armazenar requisições pendentes
# # Chave: (address, function), Valor: timestamp da requisição ou dados adicionais se necessário
# pending_requests = {}


# def calc_crc(data):
#     crc = 0xFFFF
#     for byte in data:
#         crc ^= byte
#         for _ in range(8):
#             if crc & 0x0001:
#                 crc >>= 1
#                 crc ^= 0xA001
#             else:
#                 crc >>= 1
#     return crc

# def classify_frame(address, function):
#     # Verifica se é um erro do Slave (função com bit mais significativo)
#     if function & 0x80:
#         # Função >= 0x80 → resposta de erro do Slave
#         return "Slave", "Response (Error)"

#     # Caso contrário, tentamos identificar se é resposta a um request anterior
#     key = (address, function)
#     if key in pending_requests:
#         # Já existe um request pendente com esse endereço e função,
#         # então este frame é a resposta do Slave.
#         del pending_requests[key]  # Remove do pendente
#         return "Slave", "Response"
#     else:
#         # Não existe request pendente. Então consideramos este frame como um request do Master.
#         # Registramos o request, para que o próximo frame com a mesma função e endereço
#         # seja reconhecido como resposta.
#         pending_requests[key] = time.time()  # Armazena o momento do request
#         return "Master", "Request"
    


# def interpret_frame(frame):
#     # frame: [address, function, data..., crc_low, crc_high]
#     if len(frame) < 4:
#         return None  # Frame muito curto

#     address = frame[0]
#     function = frame[1]

#     payload = frame[:-2]
#     crc_sent = frame[-2] | (frame[-1] << 8)
#     crc_calc = calc_crc(payload)
#     crc_ok = (crc_calc == crc_sent)

#     sender, frame_type = classify_frame(address, function)

#     # Extrair dados úteis se possível
#     register = None
#     value = None

#     # Exemplo de parsing para função 06 (Write Single Register):
#     # Request: Addr(1), Func(1), RegHi(1), RegLo(1), ValHi(1), ValLo(1), CRC(2)
#     # Response: Mesmo formato da request (Ecoa).
    
#     # Função 03 (Read Holding Registers) request: 
#     # [Addr][Func][StartHi][StartLo][QtyHi][QtyLo][CRC] 
#     # Response:
#     # [Addr][Func][ByteCount][Data...] [CRC]
    
#     # Dependendo do frame_type (Request ou Response) e função, extrair infos:
#     if frame_type == "Request":
#         # Master pedindo algo
#         if function == 3 or function == 4:  # Read Registers
#             if len(payload) >= 5:
#                 start_addr = (frame[2] << 8) | frame[3]
#                 qty = (frame[4] << 8) | frame[5]
#                 register = start_addr
#                 value = qty
#         elif function == 6:  # Write Single Register
#             if len(payload) >= 5:
#                 register = (frame[2] << 8) | frame[3]
#                 val = (frame[4] << 8) | frame[5]
#                 value = val
#         elif function == 16: # Write Multiple Registers (0x10)
#             if len(payload) >= 5:
#                 start_addr = (frame[2] << 8) | frame[3]
#                 qty = (frame[4] << 8) | frame[5]
#                 register = start_addr
#                 value = qty
#     else:
#         # Resposta do Slave
#         if function == 3 or function == 4: # Response Read Registers
#             if len(payload) > 2:
#                 byte_count = frame[2]
#                 if byte_count >= 2 and len(frame) >= 5:
#                     val = (frame[3] << 8) | frame[4]
#                     value = val
#         elif function == 6: # Response Write Single Register
#             if len(payload) >= 5:
#                 register = (frame[2] << 8) | frame[3]
#                 val = (frame[4] << 8) | frame[5]
#                 value = val
#         elif function == 16:
#             if len(payload) >= 5:
#                 register = (frame[2] << 8) | frame[3]
#                 qty = (frame[4] << 8) | frame[5]
#                 value = qty

#     return {
#         'address': address,
#         'function': function,
#         'sender': sender,
#         'type': frame_type,
#         'register': register,
#         'value': value,
#         'crc_ok': crc_ok,
#         'crc_received': crc_sent,
#         'crc_calc': crc_calc,
#         'size': len(frame)
#     }

# def format_detailed_output(frame_data, frame_count, timestamp, delta):
#     # Exemplo de saída:
#     # 003874: MODBUS Request (packet size: 8, data size: 4) 2024-12-03 10:42:24,648262 +0,345827
#     # Mode: RTU
#     # Address: 000001 (Slave)
#     # Function: 000006 (Write Single Register)
#     #   · Register Address = 60035
#     #   · Register Data    = 4660
#     # CRC: 16717 (OK)
    
#     # Aqui ajustaremos conforme o frame_data real
#     packet_num = f"{frame_count:06d}"
#     mode = "RTU"
#     dt_str = timestamp.strftime("%Y-%m-%d %H:%M:%S,%f")
#     delta_str = f"+{delta:.6f}"
    
#     packet_size = frame_data['size']
#     # Data size = packet_size - 4 (address(1) + function(1) + CRC(2))
#     data_size = packet_size - 4
    
#     req_or_resp = frame_data['type']  # "Request" ou "Response"
    
#     address = frame_data['address']
#     function = frame_data['function']
#     crc_ok = frame_data['crc_ok']
#     crc_info = f"{frame_data['crc_received']} ({'OK' if crc_ok else 'NOK'})"
    
#     # Determinar se é Slave ou Master no endereço:
#     # se for request do Master, então Address é do Slave; se for response do Slave, Address é do Slave.
#     # Normalmente o "address" refere-se ao Slave. Podemos imprimir assim:
#     # Address: 000001 (Slave)
#     address_str = f"{address:06d} (Slave)"

#     # Decodificar função se conhecida
#     # Exemplo: 3 = Read Holding Registers, 6 = Write Single Register, etc.
#     func_name = {
#         1: "Read Coils",
#         2: "Read Discrete Inputs",
#         3: "Read Holding Registers",
#         4: "Read Input Registers",
#         5: "Write Single Coil",
#         6: "Write Single Register",
#         15: "Write Multiple Coils",
#         16: "Write Multiple Registers"
#     }.get(function, f"Function {function:02X}")

#     # Imprimir valores caso exista:
#     reg_str = ""
#     if frame_data['register'] is not None:
#         reg_str += f"\t· Register Address\t= {frame_data['register']}\n"
#     if frame_data['value'] is not None:
#         reg_str += f"\t· Register Data   \t= {frame_data['value']}\n"

#     output = []
#     output.append(f"{packet_num}: MODBUS {req_or_resp} (packet size: {packet_size}, data size: {data_size}) {dt_str} {delta_str}")
#     output.append(f"Mode: {mode}")
#     output.append(f"Address: {address_str}")
#     output.append(f"Function: {function:06d} ({func_name})")
#     if reg_str:
#         output.append(reg_str.rstrip("\n"))
#     output.append(f"CRC: {crc_info}")
#     return "\n".join(output)

# def main():
#     ser = serial.Serial(
#         port=PORT,
#         baudrate=BAUDRATE,
#         bytesize=BYTESIZE,
#         parity=PARITY,
#         stopbits=STOPBITS,
#         timeout=TIMEOUT
#     )

#     buffer = bytearray()
#     last_read_time = time.time()
#     last_frame_time = time.time()
#     frame_count = 0

#     # Abrir CSV para escrita
#     with open('modbus_sniff.csv', 'w', newline='') as csvfile, open('modbus_sniff.txt', 'w') as txtfile:
#         fieldnames = ['timestamp', 'sender', 'type', 'address', 'function', 'register', 'value_decimal', 'crc_ok']
#         writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
#         writer.writeheader()

#         while True:
#             data = ser.read(1)
#             if data:
#                 buffer.append(data[0])
#                 last_read_time = time.time()
#             else:
#                 now = time.time()
#                 if len(buffer) > 0 and (now - last_read_time) > FRAME_GAP:
#                     # Considera que o frame terminou
#                     frame_decoded = interpret_frame(buffer)
#                     if frame_decoded:
#                         frame_count += 1
#                         current_dt = datetime.now()
#                         delta = (current_dt.timestamp() - last_frame_time)
#                         last_frame_time = current_dt.timestamp()

#                         # Formatar saída detalhada
#                         detailed_str = format_detailed_output(
#                             frame_decoded, 
#                             frame_count, 
#                             current_dt, 
#                             delta
#                         )

#                         # Imprimir no terminal
#                         print(detailed_str)
#                         print()  # linha em branco

#                         # Salvar no txt
#                         txtfile.write(detailed_str + "\n\n")
#                         txtfile.flush()

#                         # Salvar no CSV
#                         timestamp = current_dt.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3] # ms precision
#                         writer.writerow({
#                             'timestamp': timestamp,
#                             'sender': frame_decoded['sender'],
#                             'type': frame_decoded['type'],
#                             'address': frame_decoded['address'],
#                             'function': frame_decoded['function'],
#                             'register': frame_decoded['register'] if frame_decoded['register'] is not None else '',
#                             'value_decimal': frame_decoded['value'] if frame_decoded['value'] is not None else '',
#                             'crc_ok': frame_decoded['crc_ok']
#                         })
#                         csvfile.flush()

#                     buffer.clear()

#             # Pressione Ctrl+C para parar manualmente

# Fila para transferência de bytes da thread de leitura para a principal
data_queue = queue.Queue()

# Dicionário global para armazenar requests pendentes
# Chave: (address, function), Valor: timestamp do request
pending_requests = {}

# =========================================
# Funções auxiliares
# =========================================
def calc_crc(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    return crc

def classify_frame(address, function):
    # Função >= 0x80 → resposta de erro do Slave
    if function & 0x80:
        return "Slave", "Response (Error)"

    key = (address, function)
    if key in pending_requests:
        # Existe request pendente com mesmo (addr, func), logo este é a resposta do Slave.
        del pending_requests[key]
        return "Slave", "Response"
    else:
        # Não existe request pendente, então este é um request do Master.
        pending_requests[key] = time.time()
        return "Master", "Request"

def interpret_frame(frame):
    # frame: [address, function, data..., crc_low, crc_high]
    if len(frame) < 4:
        return None  # Frame muito curto

    address = frame[0]
    function = frame[1]

    payload = frame[:-2]
    crc_sent = frame[-2] | (frame[-1] << 8)
    crc_calc = calc_crc(payload)
    crc_ok = (crc_calc == crc_sent)

    if not crc_ok:
        # CRC inválido, pode descartar ou logar como erro
        return None

    sender, frame_type = classify_frame(address, function)

    register = None
    value = None

    # Parsing simples baseado em função e tipo:
    if frame_type == "Request":
        # Master pedindo algo
        if function in [3,4]:  # Ler registradores
            if len(payload) >= 5:
                start_addr = (frame[2] << 8) | frame[3]
                qty = (frame[4] << 8) | frame[5]
                register = start_addr
                value = qty
        elif function == 6:  # Write Single Register
            if len(payload) >= 5:
                register = (frame[2] << 8) | frame[3]
                val = (frame[4] << 8) | frame[5]
                value = val
        elif function == 16: # Write Multiple Registers (0x10)
            if len(payload) >= 5:
                start_addr = (frame[2] << 8) | frame[3]
                qty = (frame[4] << 8) | frame[5]
                register = start_addr
                value = qty
    else:
        # Resposta do Slave
        if function in [3,4]: # Resposta de leitura
            if len(payload) > 2:
                byte_count = frame[2]
                # Considera apenas o primeiro registrador lido
                if byte_count >= 2 and len(frame) >= 5:
                    val = (frame[3] << 8) | frame[4]
                    value = val
        elif function == 6: # Resposta a Write Single Register
            if len(payload) >= 5:
                register = (frame[2] << 8) | frame[3]
                val = (frame[4] << 8) | frame[5]
                value = val
        elif function == 16:
            if len(payload) >= 5:
                register = (frame[2] << 8) | frame[3]
                qty = (frame[4] << 8) | frame[5]
                value = qty

    return {
        'address': address,
        'function': function,
        'sender': sender,
        'type': frame_type,
        'register': register,
        'value': value,
        'crc_ok': crc_ok,
        'crc_received': crc_sent,
        'crc_calc': crc_calc,
        'size': len(frame)
    }

def format_detailed_output(frame_data, frame_count, timestamp, delta):
    packet_num = f"{frame_count:06d}"
    mode = "RTU"
    dt_str = timestamp.strftime("%Y-%m-%d %H:%M:%S,%f")
    delta_str = f"+{delta:.6f}"

    packet_size = frame_data['size']
    data_size = packet_size - 4

    req_or_resp = frame_data['type']  # "Request" ou "Response"
    address = frame_data['address']
    function = frame_data['function']
    crc_ok = frame_data['crc_ok']
    crc_info = f"{frame_data['crc_received']} ({'OK' if crc_ok else 'NOK'})"

    address_str = f"{address:06d} (Slave)"

    func_name = {
        1: "Read Coils",
        2: "Read Discrete Inputs",
        3: "Read Holding Registers",
        4: "Read Input Registers",
        5: "Write Single Coil",
        6: "Write Single Register",
        15: "Write Multiple Coils",
        16: "Write Multiple Registers"
    }.get(function, f"Function {function:02X}")

    reg_str = ""
    if frame_data['register'] is not None:
        reg_str += f"\t· Register Address\t= {frame_data['register']}\n"
    if frame_data['value'] is not None:
        reg_str += f"\t· Register Data   \t= {frame_data['value']}\n"

    output = []
    output.append(f"{packet_num}: MODBUS {req_or_resp} (packet size: {packet_size}, data size: {data_size}) {dt_str} {delta_str}")
    output.append(f"Mode: {mode}")
    output.append(f"Address: {address_str}")
    output.append(f"Function: {function:06d} ({func_name})")
    if reg_str:
        output.append(reg_str.rstrip("\n"))
    output.append(f"CRC: {crc_info}")
    return "\n".join(output)


# =========================================
# Thread de leitura da UART
# =========================================
def uart_reader(ser, data_queue):
    """Thread para leitura contínua da UART."""
    while True:
        data = ser.read(1024)
        if data:
            for b in data:
                data_queue.put(b)


# =========================================
# Função principal
# =========================================
def main():
    ser = serial.Serial(
        port=PORT,
        baudrate=BAUDRATE,
        bytesize=BYTESIZE,
        parity=PARITY,
        stopbits=STOPBITS,
        timeout=TIMEOUT
    )

    # Iniciar a thread de leitura
    t = threading.Thread(target=uart_reader, args=(ser, data_queue), daemon=True)
    t.start()

    buffer = bytearray()
    last_read_time = time.time()
    last_frame_time = time.time()
    frame_count = 0

    # Abrir CSV e TXT
    with open('modbus_sniff.csv', 'w', newline='') as csvfile, open('modbus_sniff.txt', 'w') as txtfile:
        fieldnames = ['timestamp', 'sender', 'type', 'address', 'function', 'register', 'value_decimal', 'crc_ok']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()

        while True:
            # Ler um byte da fila se disponível
            try:
                b = data_queue.get(timeout=0.1)
                buffer.append(b)
                last_read_time = time.time()
            except queue.Empty:
                # Sem dados no momento
                pass

            now = time.time()
            # Se temos dados no buffer e se passou o tempo suficiente (FRAME_GAP),
            # consideramos o frame completo
            if len(buffer) > 0 and (now - last_read_time) > FRAME_GAP:
                frame_decoded = interpret_frame(buffer)
                if frame_decoded:
                    frame_count += 1
                    current_dt = datetime.now()
                    delta = (current_dt.timestamp() - last_frame_time)
                    last_frame_time = current_dt.timestamp()

                    # Formatar saída detalhada
                    detailed_str = format_detailed_output(
                        frame_decoded,
                        frame_count,
                        current_dt,
                        delta
                    )

                    # Imprimir no terminal
                    print(detailed_str)
                    print()

                    # Salvar no txt
                    txtfile.write(detailed_str + "\n\n")
                    txtfile.flush()

                    # Salvar no CSV
                    timestamp = current_dt.strftime('%Y-%m-%d %H:%M:%S.%f')[:-3] # precisão em ms
                    writer.writerow({
                        'timestamp': timestamp,
                        'sender': frame_decoded['sender'],
                        'type': frame_decoded['type'],
                        'address': frame_decoded['address'],
                        'function': frame_decoded['function'],
                        'register': frame_decoded['register'] if frame_decoded['register'] is not None else '',
                        'value_decimal': frame_decoded['value'] if frame_decoded['value'] is not None else '',
                        'crc_ok': frame_decoded['crc_ok']
                    })
                    csvfile.flush()

                buffer.clear()

                        
main()