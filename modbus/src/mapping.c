#include <modbus/internal/mapping.h>

#if MB_CONF_BUILD_SERVER

mb_err_t mb_server_mapping_apply(mb_server_t *server,
                                 const mb_server_mapping_bank_t *banks,
                                 mb_size_t bank_count)
{
    if (server == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if ((banks == NULL && bank_count > 0U) || (banks != NULL && bank_count == 0U)) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (banks == NULL || bank_count == 0U) {
        return MB_OK;
    }

    for (mb_size_t i = 0U; i < bank_count; ++i) {
        const mb_server_mapping_bank_t *bank = &banks[i];
        if (bank->count == 0U) {
            continue;
        }
        if (bank->storage == NULL) {
            return MB_ERR_INVALID_ARGUMENT;
        }

        mb_err_t status = mb_server_add_storage(server,
                                                bank->start,
                                                bank->count,
                                                bank->read_only,
                                                bank->storage);
        if (!mb_err_is_ok(status)) {
            return status;
        }
    }

    return MB_OK;
}

mb_err_t mb_server_mapping_init(mb_server_t *server,
                                const mb_server_mapping_config_t *config)
{
    if (server == NULL || config == NULL || config->iface == NULL || config->regions == NULL ||
        config->region_capacity == 0U || config->request_pool == NULL || config->request_capacity == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = mb_server_init(server,
                                     config->iface,
                                     config->unit_id,
                                     config->regions,
                                     config->region_capacity,
                                     config->request_pool,
                                     config->request_capacity);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    return mb_server_mapping_apply(server, config->banks, config->bank_count);
}

#endif /* MB_CONF_BUILD_SERVER */
