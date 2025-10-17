#include <gtest/gtest.h>

#include <array>
#include <vector>

extern "C" {
#include <modbus/internal/client.h>
#include <modbus/internal/client_sync.h>
#include <modbus/internal/frame.h>
#include <modbus/mb_err.h>
#include <modbus/internal/core.h>
}

extern "C" {
void modbus_transport_init_mock(modbus_transport_t *transport);
int mock_inject_rx_data(const uint8_t *data, uint16_t length);
uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
void mock_clear_tx_buffer(void);
const mb_transport_if_t *mock_transport_get_iface(void);
}

namespace {

class MbClientSyncTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_init_mock(&legacy_transport_);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);
        ASSERT_EQ(MB_OK, mb_client_init(&client_, iface_, txn_pool_.data(), txn_pool_.size()));
        mock_clear_tx_buffer();
    }

    void TearDown() override
    {
        mock_clear_tx_buffer();
    }

    static void build_read_response(uint8_t unit_id,
                                    uint16_t quantity,
                                    mb_adu_view_t &adu,
                                    std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                    mb_size_t &frame_len)
    {
        std::array<mb_u8, MB_PDU_MAX> payload{};
        payload[0] = static_cast<mb_u8>(quantity * 2U);
        for (uint16_t i = 0U; i < quantity; ++i) {
            const uint16_t value = static_cast<uint16_t>(0x0100U + i);
            payload[1U + i * 2U] = static_cast<mb_u8>(value >> 8);
            payload[2U + i * 2U] = static_cast<mb_u8>(value & 0xFFU);
        }

        adu.unit_id = unit_id;
        adu.function = MODBUS_FUNC_READ_HOLDING_REGISTERS;
        adu.payload = payload.data();
        adu.payload_len = static_cast<mb_size_t>(quantity * 2U + 1U);
        ASSERT_EQ(MB_OK, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));
    }

#if MB_CONF_ENABLE_FC04
    static void build_read_input_response(uint8_t unit_id,
                                          uint16_t quantity,
                                          mb_adu_view_t &adu,
                                          std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                          mb_size_t &frame_len)
    {
        std::array<mb_u8, MB_PDU_MAX> payload{};
        payload[0] = static_cast<mb_u8>(quantity * 2U);
        for (uint16_t i = 0U; i < quantity; ++i) {
            const uint16_t value = static_cast<uint16_t>(0x0200U + i);
            payload[1U + i * 2U] = static_cast<mb_u8>(value >> 8);
            payload[2U + i * 2U] = static_cast<mb_u8>(value & 0xFFU);
        }

        adu.unit_id = unit_id;
        adu.function = MODBUS_FUNC_READ_INPUT_REGISTERS;
        adu.payload = payload.data();
        adu.payload_len = static_cast<mb_size_t>(quantity * 2U + 1U);
        ASSERT_EQ(MB_OK, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));
    }
#endif

    static void build_write_single_response(uint8_t unit_id,
                                            uint16_t address,
                                            uint16_t value,
                                            mb_adu_view_t &adu,
                                            std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                            mb_size_t &frame_len)
    {
        uint8_t payload[4];
        payload[0] = static_cast<mb_u8>(address >> 8);
        payload[1] = static_cast<mb_u8>(address & 0xFFU);
        payload[2] = static_cast<mb_u8>(value >> 8);
        payload[3] = static_cast<mb_u8>(value & 0xFFU);

        adu.unit_id = unit_id;
        adu.function = MODBUS_FUNC_WRITE_SINGLE_REGISTER;
        adu.payload = payload;
        adu.payload_len = sizeof(payload);
        ASSERT_EQ(MB_OK, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));
    }

#if MB_CONF_ENABLE_FC01
    static void build_coil_read_response(uint8_t unit_id,
                                         const bool *bits,
                                         uint16_t count,
                                         mb_adu_view_t &adu,
                                         std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                         mb_size_t &frame_len)
    {
        const mb_size_t byte_count = (mb_size_t)((count + 7U) / 8U);
        std::vector<mb_u8> payload(1U + byte_count, 0U);
        payload[0] = static_cast<mb_u8>(byte_count);
        for (uint16_t i = 0U; i < count; ++i) {
            if (bits[i]) {
                payload[1U + (i / 8U)] |= static_cast<mb_u8>(1U << (i % 8U));
            }
        }

        adu.unit_id = unit_id;
        adu.function = MODBUS_FUNC_READ_COILS;
        adu.payload = payload.data();
        adu.payload_len = payload.size();
        ASSERT_EQ(MB_OK, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));
    }
#endif

#if MB_CONF_ENABLE_FC02
    static void build_discrete_read_response(uint8_t unit_id,
                                             const bool *bits,
                                             uint16_t count,
                                             mb_adu_view_t &adu,
                                             std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                             mb_size_t &frame_len)
    {
        build_coil_read_response(unit_id, bits, count, adu, frame, frame_len);
        adu.function = MODBUS_FUNC_READ_DISCRETE_INPUTS;
    }
#endif

#if MB_CONF_ENABLE_FC05
    static void build_write_single_coil_response(uint8_t unit_id,
                                                 uint16_t address,
                                                 bool value,
                                                 mb_adu_view_t &adu,
                                                 std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                                 mb_size_t &frame_len)
    {
        const uint16_t coil_value = value ? MB_PDU_COIL_ON_VALUE : MB_PDU_COIL_OFF_VALUE;
        build_write_single_response(unit_id, address, coil_value, adu, frame, frame_len);
        adu.function = MODBUS_FUNC_WRITE_SINGLE_COIL;
    }
#endif

#if MB_CONF_ENABLE_FC10
    static void build_write_multiple_registers_response(uint8_t unit_id,
                                                        uint16_t address,
                                                        uint16_t count,
                                                        mb_adu_view_t &adu,
                                                        std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                                        mb_size_t &frame_len)
    {
        uint8_t payload[4];
        payload[0] = static_cast<mb_u8>(address >> 8);
        payload[1] = static_cast<mb_u8>(address & 0xFFU);
        payload[2] = static_cast<mb_u8>(count >> 8);
        payload[3] = static_cast<mb_u8>(count & 0xFFU);

        adu.unit_id = unit_id;
        adu.function = MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS;
        adu.payload = payload;
        adu.payload_len = sizeof(payload);
        ASSERT_EQ(MB_OK, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));
    }
#endif

    mb_client_t client_{};
    std::array<mb_client_txn_t, 4> txn_pool_{};
    modbus_transport_t legacy_transport_{};
    const mb_transport_if_t *iface_ = nullptr;
};

TEST_F(MbClientSyncTest, ReadHoldingRegistersReturnsData)
{
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_adu_view_t adu{};
    mb_size_t frame_len = 0U;
    build_read_response(0x11U, 3U, adu, response_frame, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(frame_len)));

    // Drain any previous TX data
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    (void)mock_get_tx_data(tx_frame.data(), tx_frame.size());

    uint16_t registers[3] = {0};
    mb_err_t err = mb_client_read_holding_sync(&client_, 0x11U, 0x0000U, 3U, registers, nullptr);
    ASSERT_EQ(MB_OK, err);
    EXPECT_EQ(0x0100U, registers[0]);
    EXPECT_EQ(0x0101U, registers[1]);
    EXPECT_EQ(0x0102U, registers[2]);
}

TEST_F(MbClientSyncTest, WriteSingleRegisterEchoesValue)
{
    const uint16_t address = 0x0020U;
    const uint16_t value = 0xABCDU;

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_adu_view_t adu{};
    mb_size_t frame_len = 0U;
    build_write_single_response(0x11U, address, value, adu, response_frame, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(frame_len)));
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    (void)mock_get_tx_data(tx_frame.data(), tx_frame.size());

    mb_err_t err = mb_client_write_register_sync(&client_, 0x11U, address, value, nullptr);
    EXPECT_EQ(MB_OK, err);
}

#if MB_CONF_ENABLE_FC04
TEST_F(MbClientSyncTest, ReadInputRegistersReturnsData)
{
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_adu_view_t adu{};
    mb_size_t frame_len = 0U;
    build_read_input_response(0x11U, 2U, adu, response_frame, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(frame_len)));
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    (void)mock_get_tx_data(tx_frame.data(), tx_frame.size());

    uint16_t registers[2] = {0};
    mb_err_t err = mb_client_read_input_sync(&client_, 0x11U, 0x0001U, 2U, registers, nullptr);
    ASSERT_EQ(MB_OK, err);
    EXPECT_EQ(0x0200U, registers[0]);
    EXPECT_EQ(0x0201U, registers[1]);
}
#endif

#if MB_CONF_ENABLE_FC01
TEST_F(MbClientSyncTest, ReadCoilsSyncDecodesBits)
{
    constexpr size_t kCoilCount = 9;
    std::array<bool, kCoilCount> coils = {true, false, true, true, false, true, false, false, true};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_adu_view_t adu{};
    mb_size_t frame_len = 0U;
    build_coil_read_response(0x11U, coils.data(), static_cast<uint16_t>(kCoilCount), adu, response_frame, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(frame_len)));
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    (void)mock_get_tx_data(tx_frame.data(), tx_frame.size());

    std::array<bool, kCoilCount> result{};
    mb_err_t err = mb_client_read_coils_sync(&client_, 0x11U, 0x0010U, static_cast<uint16_t>(kCoilCount), result.data(), nullptr);
    ASSERT_EQ(MB_OK, err);
    for (size_t i = 0; i < coils.size(); ++i) {
        EXPECT_EQ(coils[i], result[i]) << "Mismatch at index " << i;
    }
}
#endif

#if MB_CONF_ENABLE_FC02
TEST_F(MbClientSyncTest, ReadDiscreteInputsSyncDecodesBits)
{
    constexpr size_t kInputCount = 6;
    std::array<bool, kInputCount> inputs = {false, true, true, false, false, true};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_adu_view_t adu{};
    mb_size_t frame_len = 0U;
    build_discrete_read_response(0x11U, inputs.data(), static_cast<uint16_t>(kInputCount), adu, response_frame, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(frame_len)));
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    (void)mock_get_tx_data(tx_frame.data(), tx_frame.size());

    std::array<bool, kInputCount> result{};
    mb_err_t err = mb_client_read_discrete_inputs_sync(&client_, 0x11U, 0x0020U, static_cast<uint16_t>(kInputCount), result.data(), nullptr);
    ASSERT_EQ(MB_OK, err);
    for (size_t i = 0; i < inputs.size(); ++i) {
        EXPECT_EQ(inputs[i], result[i]) << "Mismatch at index " << i;
    }
}
#endif

#if MB_CONF_ENABLE_FC05
TEST_F(MbClientSyncTest, WriteSingleCoilSyncEchoesValue)
{
    const uint16_t address = 0x0030U;
    const bool value = true;

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_adu_view_t adu{};
    mb_size_t frame_len = 0U;
    build_write_single_coil_response(0x11U, address, value, adu, response_frame, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(frame_len)));
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    (void)mock_get_tx_data(tx_frame.data(), tx_frame.size());

    mb_err_t err = mb_client_write_coil_sync(&client_, 0x11U, address, value, nullptr);
    EXPECT_EQ(MB_OK, err);
}
#endif

#if MB_CONF_ENABLE_FC10
TEST_F(MbClientSyncTest, WriteMultipleRegistersSyncEchoesCount)
{
    const uint16_t address = 0x0040U;
    const uint16_t values[] = {0x1001U, 0x1002U, 0x1003U};
    constexpr uint16_t value_count = static_cast<uint16_t>(sizeof(values) / sizeof(values[0]));

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_adu_view_t adu{};
    mb_size_t frame_len = 0U;
    build_write_multiple_registers_response(0x11U, address, value_count, adu, response_frame, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(frame_len)));
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    (void)mock_get_tx_data(tx_frame.data(), tx_frame.size());

    mb_err_t err = mb_client_write_registers_sync(&client_, 0x11U, address, values, value_count, nullptr);
    EXPECT_EQ(MB_OK, err);
}
#endif

} // namespace
