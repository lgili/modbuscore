#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

extern "C" {
#include <modbus/mb_simple.h>
#include <modbus/internal/mb_simple_backend.h>
}

namespace {

struct FakeClient {
    std::string endpoint;
    uint32_t baudrate = 0;
    uint32_t timeout = 0;
    bool logging_enabled = false;
    mb_err_t next_err = MB_OK;
    std::vector<uint16_t> read_registers;
    std::vector<uint8_t> coil_bytes;
    uint16_t last_address = 0;
    uint16_t last_count = 0;
    std::vector<uint16_t> last_written_registers;
    std::vector<uint8_t> last_written_coils;
    bool last_bool_value = false;
    uint8_t last_exception = 0;
};

struct FakeBackendState {
    int connect_tcp_calls = 0;
    int connect_rtu_calls = 0;
    int disconnect_calls = 0;
    int set_timeout_calls = 0;
    int enable_logging_calls = 0;
    int read_holding_calls = 0;
    int write_registers_calls = 0;
    int write_coils_calls = 0;
    mb_err_t last_error_string_arg = MB_OK;
    std::string error_message = "fake-error";
    FakeClient *last_client = nullptr;
    std::vector<FakeClient *> live_clients;
    uint32_t last_rtu_baudrate = 0;
};

FakeBackendState g_backend_state;

FakeClient *as_fake(mb_simple_backend_client_t *handle)
{
    return reinterpret_cast<FakeClient *>(handle);
}

mb_simple_backend_client_t *fake_connect_tcp(const char *endpoint)
{
    g_backend_state.connect_tcp_calls += 1;
    auto *client = new FakeClient();
    client->endpoint = endpoint ? endpoint : "";
    g_backend_state.last_client = client;
    g_backend_state.live_clients.push_back(client);
    return reinterpret_cast<mb_simple_backend_client_t *>(client);
}

mb_simple_backend_client_t *fake_connect_rtu(const char *device, uint32_t baudrate)
{
    g_backend_state.connect_rtu_calls += 1;
    auto *client = new FakeClient();
    client->endpoint = device ? device : "";
    client->baudrate = baudrate;
    g_backend_state.last_rtu_baudrate = baudrate;
    g_backend_state.last_client = client;
    g_backend_state.live_clients.push_back(client);
    return reinterpret_cast<mb_simple_backend_client_t *>(client);
}

void fake_disconnect(mb_simple_backend_client_t *handle)
{
    g_backend_state.disconnect_calls += 1;
    auto *client = as_fake(handle);
    auto &live = g_backend_state.live_clients;
    live.erase(std::remove(live.begin(), live.end(), client), live.end());
    delete client;
}

void fake_set_timeout(mb_simple_backend_client_t *handle, uint32_t timeout_ms)
{
    g_backend_state.set_timeout_calls += 1;
    if (auto *client = as_fake(handle)) {
        client->timeout = timeout_ms;
    }
}

void fake_enable_logging(mb_simple_backend_client_t *handle, bool enable)
{
    g_backend_state.enable_logging_calls += 1;
    if (auto *client = as_fake(handle)) {
        client->logging_enabled = enable;
    }
}

mb_err_t fake_read_holding(mb_simple_backend_client_t *handle,
                           uint8_t /*unit_id*/,
                           uint16_t /*address*/,
                           uint16_t count,
                           uint16_t *out_registers)
{
    g_backend_state.read_holding_calls += 1;
    auto *client = as_fake(handle);
    if (!client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_err_t err = client->next_err;
    if (err > 0) {
        client->last_exception = static_cast<uint8_t>(err);
        return err;
    }

    for (uint16_t i = 0; i < count; ++i) {
        uint16_t value = 0;
        if (i < client->read_registers.size()) {
            value = client->read_registers[i];
        }
        out_registers[i] = value;
    }

    client->last_exception = 0;
    return err;
}

mb_err_t fake_read_input(mb_simple_backend_client_t *handle,
                         uint8_t unit_id,
                         uint16_t address,
                         uint16_t count,
                         uint16_t *out_registers)
{
    return fake_read_holding(handle, unit_id, address, count, out_registers);
}

mb_err_t fake_read_coils(mb_simple_backend_client_t *handle,
                         uint8_t /*unit_id*/,
                         uint16_t /*address*/,
                         uint16_t count,
                         uint8_t *out_coils)
{
    auto *client = as_fake(handle);
    if (!client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_err_t err = client->next_err;
    if (err > 0) {
        client->last_exception = static_cast<uint8_t>(err);
        return err;
    }

    const size_t bytes = static_cast<size_t>((count + 7U) / 8U);
    for (size_t i = 0; i < bytes; ++i) {
        uint8_t value = 0;
        if (i < client->coil_bytes.size()) {
            value = client->coil_bytes[i];
        }
        out_coils[i] = value;
    }

    client->last_exception = 0;
    return err;
}

mb_err_t fake_read_discrete(mb_simple_backend_client_t *handle,
                            uint8_t unit_id,
                            uint16_t address,
                            uint16_t count,
                            uint8_t *out_coils)
{
    return fake_read_coils(handle, unit_id, address, count, out_coils);
}

mb_err_t fake_write_register(mb_simple_backend_client_t *handle,
                             uint8_t /*unit_id*/,
                             uint16_t address,
                             uint16_t value)
{
    auto *client = as_fake(handle);
    if (!client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    client->last_address = address;
    client->last_count = 1;
    client->last_written_registers = {value};

    const mb_err_t err = client->next_err;
    client->last_exception = (err > 0) ? static_cast<uint8_t>(err) : 0U;
    return err;
}

mb_err_t fake_write_coil(mb_simple_backend_client_t *handle,
                         uint8_t /*unit_id*/,
                         uint16_t address,
                         bool value)
{
    auto *client = as_fake(handle);
    if (!client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    client->last_address = address;
    client->last_count = 1;
    client->last_bool_value = value;

    const mb_err_t err = client->next_err;
    client->last_exception = (err > 0) ? static_cast<uint8_t>(err) : 0U;
    return err;
}

mb_err_t fake_write_registers(mb_simple_backend_client_t *handle,
                              uint8_t /*unit_id*/,
                              uint16_t address,
                              uint16_t count,
                              const uint16_t *values)
{
    g_backend_state.write_registers_calls += 1;

    auto *client = as_fake(handle);
    if (!client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    client->last_address = address;
    client->last_count = count;
    client->last_written_registers.assign(values, values + count);

    const mb_err_t err = client->next_err;
    client->last_exception = (err > 0) ? static_cast<uint8_t>(err) : 0U;
    return err;
}

mb_err_t fake_write_coils(mb_simple_backend_client_t *handle,
                          uint8_t /*unit_id*/,
                          uint16_t address,
                          uint16_t count,
                          const uint8_t *values)
{
    g_backend_state.write_coils_calls += 1;

    auto *client = as_fake(handle);
    if (!client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    client->last_address = address;
    client->last_count = count;
    client->last_written_coils.assign(values, values + ((count + 7U) / 8U));

    const mb_err_t err = client->next_err;
    client->last_exception = (err > 0) ? static_cast<uint8_t>(err) : 0U;
    return err;
}

uint8_t fake_last_exception(mb_simple_backend_client_t *handle)
{
    auto *client = as_fake(handle);
    return client ? client->last_exception : 0U;
}

const char *fake_error_string(mb_err_t err)
{
    g_backend_state.last_error_string_arg = err;
    return g_backend_state.error_message.c_str();
}

const mb_simple_backend_t fake_backend = {
    fake_connect_tcp,
    fake_connect_rtu,
    fake_disconnect,
    fake_set_timeout,
    fake_enable_logging,
    fake_read_holding,
    fake_read_input,
    fake_read_coils,
    fake_read_discrete,
    fake_write_register,
    fake_write_coil,
    fake_write_registers,
    fake_write_coils,
    fake_last_exception,
    fake_error_string,
};

class MbSimpleTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        default_backend_ = mb_simple_get_backend();
        g_backend_state = FakeBackendState{};
        mb_simple_set_backend(&fake_backend);
    }

    void TearDown() override
    {
        EXPECT_TRUE(g_backend_state.live_clients.empty());
        for (auto *client : g_backend_state.live_clients) {
            delete client;
        }
        g_backend_state = FakeBackendState{};
        mb_simple_set_backend(default_backend_);
    }

private:
    const mb_simple_backend_t *default_backend_ = nullptr;
};

} // namespace

namespace managed_backend {

constexpr size_t kPoolSize = 2;
constexpr size_t kMaxRegisters = 16;

struct ManagedClient {
    bool in_use = false;
    uint32_t timeout = 0;
    bool logging_enabled = false;
    mb_err_t next_err = MB_OK;
    uint8_t last_exception = 0;
    std::array<uint16_t, kMaxRegisters> registers{};
    size_t register_count = 0;
};

struct State {
    std::array<ManagedClient, kPoolSize> clients{};
    size_t used = 0;
    ManagedClient *last_acquired = nullptr;
    const char *error_message = "managed-backend";
} state;

static void reset_state(void)
{
    for (auto &client : state.clients) {
        client = ManagedClient{};
    }
    state.used = 0;
    state.last_acquired = nullptr;
}

static ManagedClient *acquire_slot(void)
{
    for (auto &client : state.clients) {
        if (!client.in_use) {
            client.in_use = true;
            client.timeout = 1000U;
            client.logging_enabled = false;
            client.next_err = MB_OK;
            client.last_exception = 0U;
            client.register_count = 0U;
            state.used += 1U;
            state.last_acquired = &client;
            return &client;
        }
    }
    return nullptr;
}

static void release_slot(ManagedClient *client)
{
    if (!client || !client->in_use) {
        return;
    }

    client->in_use = false;
    client->timeout = 0U;
    client->logging_enabled = false;
    client->next_err = MB_OK;
    client->last_exception = 0U;
    client->register_count = 0U;
    if (state.used > 0U) {
        state.used -= 1U;
    }
}

static ManagedClient *as_client(mb_simple_backend_client_t *handle)
{
    return reinterpret_cast<ManagedClient *>(handle);
}

static mb_simple_backend_client_t *connect_common(void)
{
    ManagedClient *slot = acquire_slot();
    return reinterpret_cast<mb_simple_backend_client_t *>(slot);
}

static mb_simple_backend_client_t *connect_tcp(const char * /*endpoint*/)
{
    return connect_common();
}

static mb_simple_backend_client_t *connect_rtu(const char * /*device*/, uint32_t /*baudrate*/)
{
    return connect_common();
}

static void disconnect(mb_simple_backend_client_t *handle)
{
    release_slot(as_client(handle));
}

static void set_timeout(mb_simple_backend_client_t *handle, uint32_t timeout_ms)
{
    if (ManagedClient *client = as_client(handle)) {
        client->timeout = timeout_ms;
    }
}

static void enable_logging(mb_simple_backend_client_t *handle, bool enable)
{
    if (ManagedClient *client = as_client(handle)) {
        client->logging_enabled = enable;
    }
}

static mb_err_t read_holding(mb_simple_backend_client_t *handle,
                      uint8_t /*unit_id*/,
                      uint16_t /*address*/,
                      uint16_t count,
                      uint16_t *out_registers)
{
    ManagedClient *client = as_client(handle);
    if (!client || !client->in_use || !out_registers) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_err_t err = client->next_err;
    if (err > 0) {
        client->last_exception = static_cast<uint8_t>(err);
        return err;
    }

    const size_t copy_count = std::min<size_t>(count, client->register_count);
    for (size_t i = 0; i < copy_count; ++i) {
        out_registers[i] = client->registers[i];
    }
    for (size_t i = copy_count; i < count; ++i) {
        out_registers[i] = 0U;
    }

    client->last_exception = 0U;
    return err;
}

static mb_err_t read_input(mb_simple_backend_client_t *handle,
                    uint8_t unit_id,
                    uint16_t address,
                    uint16_t count,
                    uint16_t *out_registers)
{
    return read_holding(handle, unit_id, address, count, out_registers);
}

static mb_err_t read_coils(mb_simple_backend_client_t * /*handle*/,
                    uint8_t /*unit_id*/,
                    uint16_t /*address*/,
                    uint16_t /*count*/,
                    uint8_t * /*out_coils*/)
{
    return MB_ERR_OTHER;
}

static mb_err_t read_discrete(mb_simple_backend_client_t *handle,
                       uint8_t unit_id,
                       uint16_t address,
                       uint16_t count,
                       uint8_t *out_inputs)
{
    (void)out_inputs;
    return read_coils(handle, unit_id, address, count, out_inputs);
}

static mb_err_t write_register(mb_simple_backend_client_t *handle,
                        uint8_t /*unit_id*/,
                        uint16_t /*address*/,
                        uint16_t value)
{
    ManagedClient *client = as_client(handle);
    if (!client || !client->in_use) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    client->register_count = 1U;
    client->registers[0] = value;
    if (client->next_err > 0) {
        client->last_exception = static_cast<uint8_t>(client->next_err);
    } else {
        client->last_exception = 0U;
    }
    return client->next_err;
}

static mb_err_t write_coil(mb_simple_backend_client_t * /*handle*/,
                    uint8_t /*unit_id*/,
                    uint16_t /*address*/,
                    bool /*value*/)
{
    return MB_OK;
}

static mb_err_t write_registers(mb_simple_backend_client_t *handle,
                         uint8_t /*unit_id*/,
                         uint16_t /*address*/,
                         uint16_t count,
                         const uint16_t *values)
{
    ManagedClient *client = as_client(handle);
    if (!client || !client->in_use || !values) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const size_t copy_count = std::min<size_t>(count, client->registers.size());
    for (size_t i = 0; i < copy_count; ++i) {
        client->registers[i] = values[i];
    }
    client->register_count = copy_count;
    if (client->next_err > 0) {
        client->last_exception = static_cast<uint8_t>(client->next_err);
    } else {
        client->last_exception = 0U;
    }
    return client->next_err;
}

static mb_err_t write_coils(mb_simple_backend_client_t * /*handle*/,
                     uint8_t /*unit_id*/,
                     uint16_t /*address*/,
                     uint16_t /*count*/,
                     const uint8_t * /*values*/)
{
    return MB_OK;
}

static uint8_t last_exception(mb_simple_backend_client_t *handle)
{
    ManagedClient *client = as_client(handle);
    return client ? client->last_exception : 0U;
}

static const char *error_string(mb_err_t /*err*/)
{
    return state.error_message;
}

static size_t used_slots(void)
{
    return state.used;
}

static ManagedClient *last_client(void)
{
    return state.last_acquired;
}

const mb_simple_backend_t backend = {
    connect_tcp,
    connect_rtu,
    disconnect,
    set_timeout,
    enable_logging,
    read_holding,
    read_input,
    read_coils,
    read_discrete,
    write_register,
    write_coil,
    write_registers,
    write_coils,
    last_exception,
    error_string,
};

} // namespace managed_backend

class MbSimpleManagedBackendTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        default_backend_ = mb_simple_get_backend();
        managed_backend::reset_state();
        mb_simple_set_backend(&managed_backend::backend);
    }

    void TearDown() override
    {
        mb_simple_set_backend(default_backend_);
        managed_backend::reset_state();
    }

    const mb_simple_backend_t *default_backend_ = nullptr;
};

TEST_F(MbSimpleManagedBackendTest, PoolLimitsConnections)
{
    mb_t *first = mb_create_tcp("alpha");
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(1U, managed_backend::used_slots());

    mb_t *second = mb_create_tcp("beta");
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(2U, managed_backend::used_slots());

    EXPECT_EQ(nullptr, mb_create_tcp("gamma"));

    mb_destroy(second);
    EXPECT_EQ(1U, managed_backend::used_slots());
    mb_destroy(first);
    EXPECT_EQ(0U, managed_backend::used_slots());
}

TEST_F(MbSimpleManagedBackendTest, ReusesSlotsAfterDestroy)
{
    mb_t *first = mb_create_tcp("alpha");
    ASSERT_NE(nullptr, first);
    auto *slot = managed_backend::last_client();
    ASSERT_NE(nullptr, slot);

    mb_destroy(first);
    EXPECT_FALSE(slot->in_use);

    mb_t *again = mb_create_tcp("alpha");
    ASSERT_NE(nullptr, again);
    EXPECT_EQ(slot, managed_backend::last_client());

    mb_destroy(again);
    EXPECT_EQ(0U, managed_backend::used_slots());
}

TEST_F(MbSimpleManagedBackendTest, ReadHoldingReturnsManagedData)
{
    mb_t *mb = mb_create_tcp("alpha");
    ASSERT_NE(nullptr, mb);
    auto *slot = managed_backend::last_client();
    ASSERT_NE(nullptr, slot);

    slot->register_count = 3U;
    slot->registers[0] = 11U;
    slot->registers[1] = 22U;
    slot->registers[2] = 33U;
    slot->next_err = MB_OK;

    uint16_t regs[3] = {0};
    EXPECT_EQ(MB_OK, mb_read_holding(mb, 1U, 0U, 3U, regs));
    EXPECT_EQ(0U, mb_last_exception(mb));
    EXPECT_EQ(11U, regs[0]);
    EXPECT_EQ(22U, regs[1]);
    EXPECT_EQ(33U, regs[2]);

    mb_destroy(mb);
    EXPECT_EQ(0U, managed_backend::used_slots());
}

TEST_F(MbSimpleManagedBackendTest, ReconnectReusesSlotAndKeepsOptions)
{
    mb_t *mb = mb_create_tcp("alpha");
    ASSERT_NE(nullptr, mb);
    auto *slot = managed_backend::last_client();
    ASSERT_NE(nullptr, slot);

    slot->next_err = static_cast<mb_err_t>(4);
    uint16_t value = 0U;
    EXPECT_EQ(static_cast<mb_err_t>(4), mb_read_holding(mb, 1U, 0U, 1U, &value));
    EXPECT_EQ(4U, mb_last_exception(mb));

    mb_set_timeout(mb, 4321U);
    mb_set_logging(mb, true);

    EXPECT_EQ(MB_OK, mb_reconnect(mb));
    auto *reconnected = managed_backend::last_client();
    ASSERT_NE(nullptr, reconnected);
    EXPECT_EQ(slot, reconnected);
    EXPECT_EQ(4321U, reconnected->timeout);
    EXPECT_TRUE(reconnected->logging_enabled);

    reconnected->register_count = 1U;
    reconnected->registers[0] = 77U;
    reconnected->next_err = MB_OK;

    EXPECT_EQ(MB_OK, mb_read_holding(mb, 1U, 0U, 1U, &value));
    EXPECT_EQ(77U, value);
    EXPECT_EQ(0U, mb_last_exception(mb));

    mb_destroy(mb);
    EXPECT_EQ(0U, managed_backend::used_slots());
}

TEST(MbSimpleStandalone, OptionsInitSetsDefaults)
{
    mb_options_t opts{};
    opts.timeout_ms = 42;
    opts.max_retries = 7;
    opts.pool_size = 1;
    opts.enable_logging = true;
    opts.enable_diagnostics = false;

    mb_options_init(&opts);

    EXPECT_EQ(1000U, opts.timeout_ms);
    EXPECT_EQ(3U, opts.max_retries);
    EXPECT_EQ(8U, opts.pool_size);
    EXPECT_FALSE(opts.enable_logging);
    EXPECT_TRUE(opts.enable_diagnostics);
}

TEST_F(MbSimpleTest, CreateTcpAppliesDefaultOptions)
{
    mb_t *mb = mb_create_tcp("host:502");
    ASSERT_NE(nullptr, mb);
    ASSERT_NE(nullptr, g_backend_state.last_client);

    EXPECT_EQ("host:502", g_backend_state.last_client->endpoint);
    EXPECT_EQ(1000U, g_backend_state.last_client->timeout);
    EXPECT_FALSE(g_backend_state.last_client->logging_enabled);
    EXPECT_EQ(1, g_backend_state.connect_tcp_calls);
    EXPECT_EQ(1, g_backend_state.set_timeout_calls);
    EXPECT_EQ(1, g_backend_state.enable_logging_calls);

    mb_destroy(mb);
    EXPECT_EQ(1, g_backend_state.disconnect_calls);
}

TEST_F(MbSimpleTest, CreateTcpExHonorsCustomOptions)
{
    mb_options_t opts{};
    mb_options_init(&opts);
    opts.timeout_ms = 2500U;
    opts.enable_logging = true;

    mb_t *mb = mb_create_tcp_ex("example.org", &opts);
    ASSERT_NE(nullptr, mb);
    ASSERT_NE(nullptr, g_backend_state.last_client);

    EXPECT_EQ("example.org", g_backend_state.last_client->endpoint);
    EXPECT_EQ(2500U, g_backend_state.last_client->timeout);
    EXPECT_TRUE(g_backend_state.last_client->logging_enabled);

    mb_destroy(mb);
}

TEST_F(MbSimpleTest, CreateRtuReconnectsWithSavedBaudrate)
{
    mb_t *mb = mb_create_rtu("/dev/ttyUSB0", 38400);
    ASSERT_NE(nullptr, mb);
    ASSERT_NE(nullptr, g_backend_state.last_client);
    EXPECT_EQ(1, g_backend_state.connect_rtu_calls);
    EXPECT_EQ(38400U, g_backend_state.last_client->baudrate);

    EXPECT_EQ(MB_OK, mb_reconnect(mb));
    EXPECT_EQ(2, g_backend_state.connect_rtu_calls);
    EXPECT_EQ(38400U, g_backend_state.last_rtu_baudrate);

    mb_destroy(mb);
}

TEST_F(MbSimpleTest, ReadHoldingCopiesValuesFromBackend)
{
    mb_t *mb = mb_create_tcp("unit");
    ASSERT_NE(nullptr, mb);
    auto *client = g_backend_state.last_client;
    ASSERT_NE(nullptr, client);

    client->read_registers = {0x0102U, 0x0304U, 0x0506U};
    client->next_err = MB_OK;

    uint16_t buffer[3] = {0};
    mb_err_t err = mb_read_holding(mb, 1, 0, 3, buffer);
    EXPECT_EQ(MB_OK, err);
    EXPECT_EQ(0x0102U, buffer[0]);
    EXPECT_EQ(0x0304U, buffer[1]);
    EXPECT_EQ(0x0506U, buffer[2]);
    EXPECT_EQ(0U, mb_last_exception(mb));

    mb_destroy(mb);
}

TEST_F(MbSimpleTest, ReadHoldingPropagatesExceptions)
{
    mb_t *mb = mb_create_tcp("unit");
    ASSERT_NE(nullptr, mb);
    auto *client = g_backend_state.last_client;
    ASSERT_NE(nullptr, client);

    client->next_err = static_cast<mb_err_t>(2); /* Illegal data address */

    uint16_t buffer[1] = {0};
    mb_err_t err = mb_read_holding(mb, 1, 0, 1, buffer);
    EXPECT_EQ(2, err);
    EXPECT_EQ(2U, mb_last_exception(mb));

    mb_destroy(mb);
}

TEST_F(MbSimpleTest, WriteRegistersForwardsPayload)
{
    mb_t *mb = mb_create_tcp("unit");
    ASSERT_NE(nullptr, mb);
    auto *client = g_backend_state.last_client;
    ASSERT_NE(nullptr, client);
    client->next_err = MB_OK;

    const uint16_t values[3] = {10U, 20U, 30U};
    EXPECT_EQ(MB_OK, mb_write_registers(mb, 1, 0x1234, 3, values));
    EXPECT_EQ(0x1234U, client->last_address);
    ASSERT_EQ(std::vector<uint16_t>({10U, 20U, 30U}), client->last_written_registers);

    mb_destroy(mb);
}

TEST_F(MbSimpleTest, SetTimeoutUpdatesBackendImmediately)
{
    mb_t *mb = mb_create_tcp("unit");
    ASSERT_NE(nullptr, mb);
    auto *client = g_backend_state.last_client;
    ASSERT_NE(nullptr, client);

    mb_set_timeout(mb, 5555U);
    EXPECT_EQ(5555U, client->timeout);
    EXPECT_EQ(2, g_backend_state.set_timeout_calls); // initial + update

    mb_destroy(mb);
}

TEST_F(MbSimpleTest, SetLoggingUpdatesBackendImmediately)
{
    mb_t *mb = mb_create_tcp("unit");
    ASSERT_NE(nullptr, mb);
    auto *client = g_backend_state.last_client;
    ASSERT_NE(nullptr, client);

    mb_set_logging(mb, true);
    EXPECT_TRUE(client->logging_enabled);
    EXPECT_EQ(2, g_backend_state.enable_logging_calls); // initial + update

    mb_destroy(mb);
}

TEST_F(MbSimpleTest, ErrorStringDelegatesToBackend)
{
    g_backend_state.error_message = "backend-string";
    EXPECT_STREQ("backend-string", mb_error_string(MB_ERR_TIMEOUT));
    EXPECT_EQ(MB_ERR_TIMEOUT, g_backend_state.last_error_string_arg);
}

TEST_F(MbSimpleTest, SuccessfulCallClearsLastException)
{
    mb_t *mb = mb_create_tcp("unit");
    ASSERT_NE(nullptr, mb);
    auto *client = g_backend_state.last_client;
    ASSERT_NE(nullptr, client);

    client->next_err = static_cast<mb_err_t>(5);
    uint16_t read_value{};
    (void)mb_read_holding(mb, 1, 0, 1, &read_value);
    EXPECT_EQ(5U, mb_last_exception(mb));

    client = g_backend_state.last_client;
    client->next_err = MB_OK;
    EXPECT_EQ(MB_OK, mb_write_coil(mb, 1, 0x77, true));
    EXPECT_EQ(0U, mb_last_exception(mb));

    mb_destroy(mb);
}
