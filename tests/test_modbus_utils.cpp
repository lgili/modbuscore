/**
 * @file test_modbus_utils.cpp
 * @brief Testes unitários para modbus_utils usando Google Test.
 *
 * Este arquivo testa as funções definidas em modbus_utils.h:
 * - Leitura segura de uint8/uint16
 * - Ordenação e busca binária em arrays de holding registers
 *
 * Pré-requisitos:
 * - gtest instalado e integrado ao projeto.
 * - modbus_utils.h e modbus_utils.c inclusos no build.
 *
 * Autor:
 * Data: 2024-12-20
 */

#include <modbus/modbus.h>
#include "gtest/gtest.h"


// Teste de leitura segura de uint8
TEST(ModbusUtilsTest, ReadUint8Success) {
    uint8_t buffer[] = {0x12, 0x34};
    uint16_t index = 0;
    uint8_t value = 0;
    bool result = modbus_read_uint8(buffer, &index, sizeof(buffer), &value);
    EXPECT_TRUE(result);
    EXPECT_EQ(value, 0x12);
    EXPECT_EQ(index, 1); // index incrementado
}

// Teste de leitura segura de uint8 com overflow
TEST(ModbusUtilsTest, ReadUint8Overflow) {
    uint8_t buffer[] = {0x12};
    uint16_t index = 1; // já no fim
    uint8_t value = 0;
    bool result = modbus_read_uint8(buffer, &index, sizeof(buffer), &value);
    EXPECT_FALSE(result); // Falha pois não há byte para ler
}

// Teste de leitura segura de uint16
TEST(ModbusUtilsTest, ReadUint16Success) {
    uint8_t buffer[] = {0xAB, 0xCD, 0xEF};
    uint16_t index = 0;
    uint16_t value = 0;
    bool result = modbus_read_uint16(buffer, &index, sizeof(buffer), &value);
    EXPECT_TRUE(result);
    EXPECT_EQ(value, 0xABCD);
    EXPECT_EQ(index, 2);
}

// Teste de leitura segura de uint16 com overflow
TEST(ModbusUtilsTest, ReadUint16Overflow) {
    uint8_t buffer[] = {0xAB};
    uint16_t index = 0;
    uint16_t value = 0;
    bool result = modbus_read_uint16(buffer, &index, sizeof(buffer), &value);
    EXPECT_FALSE(result); // Falha, não há 2 bytes
}

// Teste de ordenação com modbus_selection_sort
TEST(ModbusUtilsTest, SelectionSort) {
    variable_modbus_t vars[5];
    // Preenche com endereços não ordenados
    vars[0].address = 30;
    vars[1].address = 10;
    vars[2].address = 50;
    vars[3].address = 20;
    vars[4].address = 40;

    modbus_selection_sort(vars, 5);

    // Espera ordem: 10,20,30,40,50
    EXPECT_EQ(vars[0].address, 10);
    EXPECT_EQ(vars[1].address, 20);
    EXPECT_EQ(vars[2].address, 30);
    EXPECT_EQ(vars[3].address, 40);
    EXPECT_EQ(vars[4].address, 50);
}

// Teste de busca binária
TEST(ModbusUtilsTest, BinarySearchFound) {
    variable_modbus_t vars[5];
    vars[0].address = 10;
    vars[1].address = 20;
    vars[2].address = 30;
    vars[3].address = 40;
    vars[4].address = 50;

    int idx = modbus_binary_search(vars, 0, 4, 30);
    EXPECT_EQ(idx, 2); // Encontrado no índice 2
}

TEST(ModbusUtilsTest, BinarySearchNotFound) {
    variable_modbus_t vars[5];
    vars[0].address = 10;
    vars[1].address = 20;
    vars[2].address = 30;
    vars[3].address = 40;
    vars[4].address = 50;

    int idx = modbus_binary_search(vars, 0, 4, 35);
    EXPECT_EQ(idx, -1); // Não encontrado
}

TEST(ModbusUtilsTest, CrcImplementationsMatchKnownVector) {
    const uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
    const uint16_t expected_crc = 0x0BC4;

    uint16_t bitwise_crc = modbus_calculate_crc(frame, sizeof(frame));
    uint16_t table_crc = modbus_crc_with_table(frame, sizeof(frame));

    EXPECT_EQ(bitwise_crc, expected_crc);
    EXPECT_EQ(table_crc, expected_crc);
}

TEST(ModbusUtilsTest, CrcValidateAcceptsCorrectTrailer) {
    uint8_t frame_with_crc[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
    EXPECT_TRUE(modbus_crc_validate(frame_with_crc, sizeof(frame_with_crc)));
}

TEST(ModbusUtilsTest, CrcValidateRejectsCorruptedTrailer) {
    uint8_t frame_with_crc[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00};
    EXPECT_FALSE(modbus_crc_validate(frame_with_crc, sizeof(frame_with_crc)));
}

TEST(ModbusUtilsTest, CrcValidateRejectsShortFrame) {
    const uint8_t frame[] = {0x01};
    EXPECT_FALSE(modbus_crc_validate(frame, sizeof(frame)));
}

