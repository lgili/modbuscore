#!/usr/bin/env python3
"""
Simple Modbus TCP server - NO pymodbus dependency!
Implements just FC03 (Read Holding Registers) for testing
"""

import socket
import struct
import threading

class SimpleModbusServer:
    def __init__(self, host='127.0.0.1', port=5502):
        self.host = host
        self.port = port
        self.holding_registers = list(range(100))  # Registers 0-99 with values = address
        self.running = False

    def start(self):
        self.running = True
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        self.server_socket.settimeout(1.0)  # Timeout para accept() poder checar self.running
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)

        print(f"=== Simple Modbus TCP Server ===")
        print(f"Listening on {self.host}:{self.port}")
        print(f"Holding Registers: 0-99, values = address")
        print("Press Ctrl+C to stop\n")

        while self.running:
            try:
                client_socket, addr = self.server_socket.accept()
                print(f"Client connected from {addr}")
                thread = threading.Thread(target=self.handle_client, args=(client_socket,))
                thread.daemon = True
                thread.start()
            except socket.timeout:
                # Normal timeout, continua loop para checar self.running
                continue
            except KeyboardInterrupt:
                break
            except Exception as e:
                if self.running:
                    print(f"Error accepting connection: {e}")

    def handle_client(self, client_socket):
        try:
            # Set socket to non-blocking mode with timeout
            client_socket.settimeout(5.0)

            while True:
                # Read MBAP header (7 bytes)
                try:
                    data = client_socket.recv(1024)
                except socket.timeout:
                    # Client não enviou mais dados, fecha conexão
                    break

                if not data:
                    break

                if len(data) < 7:
                    print(f"Invalid request: too short ({len(data)} bytes)")
                    continue

                # Parse MBAP header
                trans_id = struct.unpack('>H', data[0:2])[0]
                proto_id = struct.unpack('>H', data[2:4])[0]
                length = struct.unpack('>H', data[4:6])[0]
                unit_id = data[6]

                print(f"Request: trans_id={trans_id}, proto={proto_id}, len={length}, unit={unit_id}")

                # Parse PDU
                if len(data) < 7 + length - 1:
                    print("Incomplete PDU")
                    continue

                function_code = data[7]
                print(f"Function Code: {function_code}")

                if function_code == 0x03:  # Read Holding Registers
                    start_addr = struct.unpack('>H', data[8:10])[0]
                    quantity = struct.unpack('>H', data[10:12])[0]
                    print(f"FC03: addr={start_addr}, qty={quantity}", flush=True)

                    # Build response
                    response = self.build_fc03_response(trans_id, unit_id, start_addr, quantity)
                    client_socket.sendall(response)
                    print(f"Response sent: {len(response)} bytes", flush=True)
                    # Shutdown write side to signal we're done sending
                    # This flushes buffers and allows client to read
                    client_socket.shutdown(socket.SHUT_WR)
                    print("Write side shutdown", flush=True)
                    break  # Exit loop after responding
                else:
                    print(f"Unsupported function code: {function_code}")
                    # Send exception
                    response = self.build_exception(trans_id, unit_id, function_code, 0x01)
                    client_socket.sendall(response)
                    break  # Close connection after sending exception

        except Exception as e:
            print(f"Error handling client: {e}")
        finally:
            client_socket.close()
            print("Client disconnected")

    def build_fc03_response(self, trans_id, unit_id, start_addr, quantity):
        """Build FC03 response with MBAP header"""
        # Check bounds
        if start_addr + quantity > 100:
            return self.build_exception(trans_id, unit_id, 0x03, 0x02)  # Illegal data address

        # Get register values
        values = self.holding_registers[start_addr:start_addr + quantity]

        # Build PDU: [FC] [Byte Count] [Register Values...]
        byte_count = quantity * 2
        pdu = struct.pack('BB', 0x03, byte_count)

        for value in values:
            pdu += struct.pack('>H', value)

        # Build MBAP header
        pdu_length = len(pdu) + 1  # +1 for unit ID
        mbap = struct.pack('>HHHB', trans_id, 0, pdu_length, unit_id)

        return mbap + pdu

    def build_exception(self, trans_id, unit_id, function_code, exception_code):
        """Build exception response"""
        pdu = struct.pack('BB', function_code | 0x80, exception_code)
        pdu_length = len(pdu) + 1
        mbap = struct.pack('>HHHB', trans_id, 0, pdu_length, unit_id)
        return mbap + pdu

    def stop(self):
        self.running = False
        if hasattr(self, 'server_socket'):
            self.server_socket.close()

if __name__ == '__main__':
    server = SimpleModbusServer()
    try:
        server.start()
    except KeyboardInterrupt:
        print("\n✓ Server stopped")
    finally:
        server.stop()
