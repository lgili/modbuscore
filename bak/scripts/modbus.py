import serial
import time
import minimalmodbus

class MaiaInverter( minimalmodbus.Instrument ):
    """
    Class for controlling and monitoring the Maia inverter using the Modbus RTU protocol.
    # ... (restante da docstring como estava)
    """

    # Endereços Modbus
    reg0 = 0
    reg1 = 1
    reg2 = 2
    reg3 = 3
    reg4 = 4
    # Adicione outros endereços que você usa, por exemplo:
    # bus_voltage_addr = 4
    # temperature_addr = 5
    # enable_motor_addr = 7


    def __init__(self, portname, slaveaddress, verbose = True,  debug = False):
        minimalmodbus.Instrument.__init__(self, portname, slaveaddress, debug=debug)
        self.serial.parity = minimalmodbus.serial.PARITY_EVEN
        self.serial.timeout = 1
        self.serial.baudrate = 19200 # Ajustado para 19200 como no seu exemplo anterior
        self.verbose = verbose
        self.close_port_after_each_call = True

    def close(self):
        """Closes the serial connection."""
        if self.serial.is_open: # Verifica se a porta está aberta antes de tentar fechar
            self.serial.close()
            print("Serial connection closed.")
        else:
            print("Serial connection was already closed or not opened.")


    def log(self, message):
        """Logs a message if verbose mode is enabled."""
        if self.verbose:
            print(f"[LOG] {message}")

    def read_single_variable(self, register_address, number_of_decimals=0):
        """
        Lê um único registrador.
        """
        try:
            value = self.read_register(register_address, number_of_decimals=number_of_decimals, functioncode=3)
            self.log(f"Registrador {register_address}: {value}")
            return value
        except (minimalmodbus.ModbusException, serial.SerialException, IOError) as e:
            self.log(f"Erro ao ler registrador {register_address}: {e}")
            return None

    def read_multiple_variables(self, start_address, num_registers):
        """
        Lê múltiplos registradores a partir de um endereço inicial.
        Retorna uma lista de valores ou None em caso de erro.
        """
        try:
            values = self.read_registers(start_address, num_registers, functioncode=3)
            self.log(f"Lidos {num_registers} registradores a partir de {start_address}: {values}")
            return values
        except (minimalmodbus.ModbusException, serial.SerialException, IOError) as e:
            self.log(f"Erro ao ler múltiplos registradores a partir de {start_address}: {e}")
            return None

    def write_single_variable(self, register_address, value_to_write):
        """
        Escreve em um único registrador.
        """
        try:
            self.write_register(register_address, value_to_write, functioncode=6) # FC6 para um registrador
            self.log(f"Escrito {value_to_write} no registrador {register_address}")
            return True
        except (minimalmodbus.ModbusException, serial.SerialException, IOError) as e:
            self.log(f"Erro ao escrever no registrador {register_address}: {e}")
            return False

    def write_multiple_variables(self, start_address, list_of_values):
        """
        Escreve em múltiplos registradores.
        """
        try:
            self.write_registers(start_address, list_of_values) # FC16 é o padrão para write_registers
            self.log(f"Escritos os valores {list_of_values} a partir do registrador {start_address}")
            return True
        except (minimalmodbus.ModbusException, serial.SerialException, IOError) as e:
            self.log(f"Erro ao escrever múltiplos registradores a partir de {start_address}: {e}")
            return False

    # ... (outros métodos como set_enable, monitor_device, etc., podem ser adaptados ou mantidos)

# Exemplo de uso:
if __name__ == '__main__':
    # Certifique-se de que a porta COM está correta e o dispositivo está conectado
    # e com o ID de escravo correto.
    # Exemplo: maia1 = MaiaInverter('COM3', 1, verbose=True)
    # Ajuste SERIAL_PORT e SLAVE_ID conforme sua configuração
    SERIAL_PORT_LOOP = 'COM19' # Defina sua porta serial aqui
    SLAVE_ID_LOOP = 1          # Defina o ID do escravo aqui

    maia1 = MaiaInverter(SERIAL_PORT_LOOP, SLAVE_ID_LOOP, verbose=True)
    
    successful_cycles = 0
    error_cycles = 0
    loop_iteration = 0

    try:
        while True:
            loop_iteration += 1
            print(f"\n--- Iniciando Ciclo {loop_iteration} ---")
            current_cycle_has_error = False

            # 1. Teste de Leitura de Registrador Único
            maia1.log(f"Ciclo {loop_iteration}: Lendo registrador único {maia1.reg0}...")
            valor_lido_single = maia1.read_single_variable(maia1.reg0)
            if valor_lido_single is None:
                current_cycle_has_error = True
                maia1.log(f"Ciclo {loop_iteration}: Falha ao ler registrador único {maia1.reg0}.")
            # else:
                # maia1.log(f"Ciclo {loop_iteration}: Registrador {maia1.reg0} lido: {valor_lido_single}")

            # 2. Teste de Leitura de Múltiplos Registradores
            num_regs_to_read_multiple = 5 # Ler todos os 5 registradores (0 a 4)
            maia1.log(f"Ciclo {loop_iteration}: Lendo {num_regs_to_read_multiple} registradores a partir de {maia1.reg0}...")
            valores_lidos_multiple = maia1.read_multiple_variables(maia1.reg0, num_regs_to_read_multiple)
            if valores_lidos_multiple is None:
                current_cycle_has_error = True
                maia1.log(f"Ciclo {loop_iteration}: Falha ao ler múltiplos registradores a partir de {maia1.reg0}.")
            # else:
                # maia1.log(f"Ciclo {loop_iteration}: Múltiplos registradores (0-4) lidos: {valores_lidos_multiple}")

            # 3. Teste de Escrita de Registrador Único
            valor_para_escrever_single = 100 + (loop_iteration % 10) # Valor dinâmico
            maia1.log(f"Ciclo {loop_iteration}: Escrevendo {valor_para_escrever_single} no registrador {maia1.reg2}...")
            if not maia1.write_single_variable(maia1.reg2, valor_para_escrever_single):
                current_cycle_has_error = True
                maia1.log(f"Ciclo {loop_iteration}: Falha ao escrever no registrador único {maia1.reg2}.")
            else:
                time.sleep(0.02) # Pequena pausa para o escravo processar
                confirmacao_single = maia1.read_single_variable(maia1.reg2)
                if confirmacao_single is None or confirmacao_single != valor_para_escrever_single:
                    maia1.log(f"Ciclo {loop_iteration}: Erro na confirmação da escrita única. Esperado: {valor_para_escrever_single}, Lido: {confirmacao_single}")
                    current_cycle_has_error = True

            # 4. Teste de Escrita de Múltiplos Registradores
            # Escrever em 2 registradores (endereços 3 e 4) para ficar dentro do limite 0-4
            valores_para_escrever_multiple = [loop_iteration % 256, (loop_iteration + 1) % 256]
            start_addr_write_multiple = maia1.reg3 # Começa no endereço 3
            maia1.log(f"Ciclo {loop_iteration}: Escrevendo {valores_para_escrever_multiple} a partir do registrador {start_addr_write_multiple}...")
            if not maia1.write_multiple_variables(start_addr_write_multiple, valores_para_escrever_multiple):
                current_cycle_has_error = True
                maia1.log(f"Ciclo {loop_iteration}: Falha ao escrever múltiplos registradores a partir de {start_addr_write_multiple}.")
            else:
                time.sleep(0.02) # Pequena pausa para o escravo processar
                confirmacao_multiple = maia1.read_multiple_variables(start_addr_write_multiple, len(valores_para_escrever_multiple))
                if confirmacao_multiple is None or confirmacao_multiple != valores_para_escrever_multiple:
                    maia1.log(f"Ciclo {loop_iteration}: Erro na confirmação da escrita múltipla. Esperado: {valores_para_escrever_multiple}, Lido: {confirmacao_multiple}")
                    current_cycle_has_error = True

            if current_cycle_has_error:
                error_cycles += 1
                print(f"--- Ciclo {loop_iteration} concluído com ERROS. ---")
            else:
                successful_cycles += 1
                print(f"--- Ciclo {loop_iteration} concluído com SUCESSO. ---")
            
            print(f"Status: Ciclos Bem-sucedidos = {successful_cycles}, Ciclos com Erro = {error_cycles}")
            time.sleep(0.1) # Intervalo de 100ms para o loop

    except KeyboardInterrupt:
        print("\n[INFO] Usuário interrompeu o loop. Saindo...")
    except Exception as e:
        print(f"\n[ERRO] Um erro inesperado ocorreu: {e}")
    finally:
        print(f"\n--- Estatísticas Finais ---")
        print(f"Total de Ciclos Tentados: {loop_iteration}")
        print(f"Ciclos Bem-sucedidos: {successful_cycles}")
        print(f"Ciclos com Erro: {error_cycles}")
        print("Fechando conexão com maia1...")
        maia1.close()
