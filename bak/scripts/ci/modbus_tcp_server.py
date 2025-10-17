#!/usr/bin/env python3
"""Lightweight Modbus TCP server for CI integration tests."""

from pymodbus.datastore import ModbusServerContext, ModbusSlaveContext
from pymodbus.datastore import ModbusSequentialDataBlock
from pymodbus.server.sync import StartTcpServer

HOST = "0.0.0.0"
PORT = 15020
UNIT_ID = 1


def main() -> None:
    holding_registers = [100, 200, 300, 400, 500]
    store = ModbusSlaveContext(
        hr=ModbusSequentialDataBlock(0, holding_registers),
        zero_mode=True,
    )
    context = ModbusServerContext(slaves={UNIT_ID: store}, single=False)

    print(f"[modbus-tcp-server] starting on {HOST}:{PORT} for unit {UNIT_ID}", flush=True)
    StartTcpServer(context, address=(HOST, PORT))


if __name__ == "__main__":
    main()
