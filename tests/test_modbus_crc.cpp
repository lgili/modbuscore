/**
 * @file test_modbus_crc.cpp
 * @brief Testes unitários para modbus_crc usando Google Test.
 *
 * Este arquivo testa as funções de cálculo de CRC Modbus:
 * - modbus_calculate_crc (bit-a-bit)
 * - modbus_crc_with_table (lookup table)
 *
 * Verificamos se o CRC resultante coincide com valores conhecidos e se as duas
 * implementações retornam o mesmo resultado para os mesmos dados.
 *
 * Autor:
 * Data: 2024-12-20
 */

#include <modbus/modbus.h>
#include "gtest/gtest.h"

// Teste com um array simples
TEST(ModbusCrcTest, BasicCrcCheck) {
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A}; 
    // Este é um frame típico de leitura de 10 registros a partir do endereço 0:
    // Address=0x01, Function=0x03, StartAddr=0x0000, Qty=0x000A
    // CRC esperado (calculado previamente com ferramenta externa) é 0xCDC5:
    // Em Modbus RTU o CRC low byte vem primeiro, então o frame seria: ...C5 CD 

    uint16_t crc_bit_by_bit = modbus_calculate_crc(data, (uint16_t)sizeof(data));
    uint16_t crc_table = modbus_crc_with_table(data, (uint16_t)sizeof(data));


    EXPECT_EQ(crc_bit_by_bit, 0xCDC5);
    EXPECT_EQ(crc_table, 0xCDC5);
    EXPECT_EQ(crc_bit_by_bit, crc_table);
}

// Teste com dados vazios
TEST(ModbusCrcTest, EmptyData) {
    uint8_t data[1]; // conteúdo irrelevante
    // Se length=0, CRC deve iniciar em 0xFFFF e não alterar, então não há input.
    uint16_t crc_bit_by_bit = modbus_calculate_crc(data, 0);
    uint16_t crc_table = modbus_crc_with_table(data, 0);

    // Com length=0, o CRC final não é definido especificamente, mas espera-se
    // que retorne o valor inicial 0xFFFF após processamento sem dados?
    // Neste caso, verifique apenas se ambos batem.
    EXPECT_EQ(crc_bit_by_bit, crc_table);
}

// Teste com um único byte
TEST(ModbusCrcTest, SingleByte) {
    uint8_t data[] = {0xFF};
    // Calcular CRC de um único byte 0xFF
    uint16_t crc_bit_by_bit = modbus_calculate_crc(data, 1);
    uint16_t crc_table = modbus_crc_with_table(data, 1);

    // Apenas verificar se ambos dão o mesmo resultado, e não falha.
    EXPECT_EQ(crc_bit_by_bit, crc_table);
}

// Teste com dados maiores
TEST(ModbusCrcTest, LargerData) {
    uint8_t data[10] = {0x10, 0x20, 0x30, 0xA5, 0x5A, 0xFF, 0x00, 0x11, 0x33, 0x77};

    uint16_t crc_bit_by_bit = modbus_calculate_crc(data, (uint16_t)sizeof(data));
    uint16_t crc_table = modbus_crc_with_table(data, (uint16_t)sizeof(data));

    EXPECT_EQ(crc_bit_by_bit, 0x7002);
    EXPECT_EQ(crc_table, 0x7002);
    EXPECT_EQ(crc_bit_by_bit, crc_table);
}

