#include <modbus/internal/features.h>

#include <modbus/conf.h>
#include <modbus/internal/pdu.h>

#define MB_BOOL_FROM_FLAG(flag) ((flag) ? true : false)

#if MB_CONF_PROFILE == MB_CONF_PROFILE_TINY
#define MB_FEATURE_PROFILE_NAME "TINY"
#elif MB_CONF_PROFILE == MB_CONF_PROFILE_LEAN
#define MB_FEATURE_PROFILE_NAME "LEAN"
#elif MB_CONF_PROFILE == MB_CONF_PROFILE_FULL
#define MB_FEATURE_PROFILE_NAME "FULL"
#elif MB_CONF_PROFILE == MB_CONF_PROFILE_CUSTOM
#define MB_FEATURE_PROFILE_NAME "CUSTOM"
#else
#define MB_FEATURE_PROFILE_NAME "UNKNOWN"
#endif

static const mb_u8 mb_feature_function_codes_data[] = {
#if MB_CONF_ENABLE_FC01
    MB_PDU_FC_READ_COILS,
#endif
#if MB_CONF_ENABLE_FC02
    MB_PDU_FC_READ_DISCRETE_INPUTS,
#endif
#if MB_CONF_ENABLE_FC03
    MB_PDU_FC_READ_HOLDING_REGISTERS,
#endif
#if MB_CONF_ENABLE_FC04
    MB_PDU_FC_READ_INPUT_REGISTERS,
#endif
#if MB_CONF_ENABLE_FC05
    MB_PDU_FC_WRITE_SINGLE_COIL,
#endif
#if MB_CONF_ENABLE_FC06
    MB_PDU_FC_WRITE_SINGLE_REGISTER,
#endif
#if MB_CONF_ENABLE_FC07
    MB_PDU_FC_READ_EXCEPTION_STATUS,
#endif
#if MB_CONF_ENABLE_FC0F
    MB_PDU_FC_WRITE_MULTIPLE_COILS,
#endif
#if MB_CONF_ENABLE_FC10
    MB_PDU_FC_WRITE_MULTIPLE_REGISTERS,
#endif
#if MB_CONF_ENABLE_FC11
    MB_PDU_FC_REPORT_SERVER_ID,
#endif
#if MB_CONF_ENABLE_FC16
    MB_PDU_FC_MASK_WRITE_REGISTER,
#endif
#if MB_CONF_ENABLE_FC17
    MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS,
#endif
    0U
};

#define MB_FEATURE_FUNCTION_CODE_COUNT \
    ((sizeof(mb_feature_function_codes_data) / sizeof(mb_feature_function_codes_data[0])) - 1U)

#define MB_FEATURE_FUNCTION_CODES_PTR \
    ((MB_FEATURE_FUNCTION_CODE_COUNT > 0U) ? mb_feature_function_codes_data : NULL)

const mb_feature_profile_t mb_feature_profile = {
    .profile_id = MB_CONF_PROFILE,
    .profile_name = MB_FEATURE_PROFILE_NAME,
    .build_client = MB_BOOL_FROM_FLAG(MB_CONF_BUILD_CLIENT),
    .build_server = MB_BOOL_FROM_FLAG(MB_CONF_BUILD_SERVER),
    .transport_rtu = MB_BOOL_FROM_FLAG(MB_CONF_TRANSPORT_RTU),
    .transport_ascii = MB_BOOL_FROM_FLAG(MB_CONF_TRANSPORT_ASCII),
    .transport_tcp = MB_BOOL_FROM_FLAG(MB_CONF_TRANSPORT_TCP),
    .port_mutex = MB_BOOL_FROM_FLAG(MB_CONF_PORT_MUTEX),
    .port_posix = MB_BOOL_FROM_FLAG(MB_CONF_PORT_POSIX),
    .function_codes = MB_FEATURE_FUNCTION_CODES_PTR,
    .function_code_count = MB_FEATURE_FUNCTION_CODE_COUNT,
};
