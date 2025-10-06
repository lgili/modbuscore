#include <modbus/conf.h>

#if MB_CONF_TRANSPORT_TCP

#include <modbus/transport/tcp.h>

#include <string.h>

#define MB_TCP_PROTOCOL_ID 0U

static void mb_tcp_invoke_callback(mb_tcp_transport_t *tcp,
                                   const mb_adu_view_t *adu,
                                   mb_u16 transaction_id,
                                   mb_err_t status)
{
    if (tcp->callback) {
        tcp->callback(tcp, adu, transaction_id, status, tcp->user_ctx);
    }
}

mb_err_t mb_tcp_init(mb_tcp_transport_t *tcp,
                     const mb_transport_if_t *iface,
                     mb_tcp_frame_callback_t callback,
                     void *user_ctx)
{
    if (!tcp || !iface || !iface->send || !iface->recv || !iface->now) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(tcp, 0, sizeof(*tcp));
    tcp->iface = iface;
    tcp->callback = callback;
    tcp->user_ctx = user_ctx;
    return MB_OK;
}

void mb_tcp_reset(mb_tcp_transport_t *tcp)
{
    if (!tcp) {
        return;
    }
    tcp->rx_len = 0U;
}

static mb_err_t mb_tcp_append_rx(mb_tcp_transport_t *tcp,
                                 const mb_u8 *data,
                                 mb_size_t len)
{
    if ((tcp->rx_len + len) > sizeof(tcp->rx_buffer)) {
        tcp->rx_len = 0U;
        return MB_ERR_INVALID_REQUEST;
    }

    memcpy(&tcp->rx_buffer[tcp->rx_len], data, len);
    tcp->rx_len += len;
    return MB_OK;
}

static void mb_tcp_consume_bytes(mb_tcp_transport_t *tcp, mb_size_t count)
{
    if (count >= tcp->rx_len) {
        tcp->rx_len = 0U;
        return;
    }

    memmove(tcp->rx_buffer, &tcp->rx_buffer[count], tcp->rx_len - count);
    tcp->rx_len -= count;
}

mb_err_t mb_tcp_submit(mb_tcp_transport_t *tcp,
                       const mb_adu_view_t *adu,
                       mb_u16 transaction_id)
{
    if (!tcp || !adu || !tcp->iface || !tcp->iface->send) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_size_t pdu_len = 1U + adu->payload_len; /* function + payload */
    const mb_size_t length_field = 1U + pdu_len;     /* unit id + PDU */
    if (length_field > MB_PDU_MAX + 1U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_u8 frame[MB_TCP_BUFFER_SIZE];
    mb_size_t index = 0U;

    frame[index++] = (mb_u8)((transaction_id >> 8) & 0xFFU);
    frame[index++] = (mb_u8)(transaction_id & 0xFFU);
    frame[index++] = 0x00U; /* protocol high */
    frame[index++] = 0x00U; /* protocol low */
    frame[index++] = (mb_u8)((length_field >> 8) & 0xFFU);
    frame[index++] = (mb_u8)(length_field & 0xFFU);
    frame[index++] = adu->unit_id;
    frame[index++] = adu->function;

    if (adu->payload_len > 0U) {
        memcpy(&frame[index], adu->payload, adu->payload_len);
        index += adu->payload_len;
    }

    mb_transport_io_result_t io = {0};
    mb_err_t err = mb_transport_send(tcp->iface, frame, index, &io);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    if (io.processed != index) {
        return MB_ERR_TRANSPORT;
    }

    return MB_OK;
}
static mb_err_t mb_tcp_process_frame(mb_tcp_transport_t *tcp)
{
    while (tcp->rx_len >= MB_TCP_HEADER_SIZE) {
        const mb_u16 transaction_id = (mb_u16)((tcp->rx_buffer[0] << 8) | tcp->rx_buffer[1]);
        const mb_u16 protocol_id = (mb_u16)((tcp->rx_buffer[2] << 8) | tcp->rx_buffer[3]);
        const mb_u16 length_field = (mb_u16)((tcp->rx_buffer[4] << 8) | tcp->rx_buffer[5]);

        if (protocol_id != MB_TCP_PROTOCOL_ID) {
            mb_tcp_invoke_callback(tcp, NULL, transaction_id, MB_ERR_INVALID_REQUEST);
            mb_tcp_consume_bytes(tcp, MB_TCP_HEADER_SIZE);
            continue;
        }

        if (length_field == 0U || length_field > (MB_PDU_MAX + 1U)) {
            mb_tcp_invoke_callback(tcp, NULL, transaction_id, MB_ERR_INVALID_REQUEST);
            mb_tcp_consume_bytes(tcp, MB_TCP_HEADER_SIZE);
            continue;
        }

        const mb_size_t total_len = 6U + (mb_size_t)length_field;
        if (tcp->rx_len < total_len) {
            return MB_ERR_TIMEOUT;
        }

        const mb_u8 unit_id = tcp->rx_buffer[6];
        const mb_u8 *pdu_bytes = &tcp->rx_buffer[7];
        const mb_size_t pdu_len = length_field - 1U;

        if (pdu_len == 0U) {
            mb_tcp_invoke_callback(tcp, NULL, transaction_id, MB_ERR_INVALID_REQUEST);
            mb_tcp_consume_bytes(tcp, total_len);
            continue;
        }

        if (pdu_len > MB_PDU_MAX) {
            mb_tcp_invoke_callback(tcp, NULL, transaction_id, MB_ERR_INVALID_REQUEST);
            mb_tcp_consume_bytes(tcp, total_len);
            continue;
        }

        const mb_u8 function = pdu_bytes[0];
        const mb_size_t payload_len = pdu_len - 1U;
        if (payload_len > 0U) {
            memcpy(tcp->payload_buffer, &pdu_bytes[1], payload_len);
        }

        mb_adu_view_t view = {
            .unit_id = unit_id,
            .function = function,
            .payload = (payload_len > 0U) ? tcp->payload_buffer : NULL,
            .payload_len = payload_len,
        };

        mb_tcp_invoke_callback(tcp, &view, transaction_id, MB_OK);
        mb_tcp_consume_bytes(tcp, total_len);
    }

    return MB_OK;
}

mb_err_t mb_tcp_poll(mb_tcp_transport_t *tcp)
{
    if (!tcp || !tcp->iface) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_u8 chunk[64];
    mb_transport_io_result_t io = {0};
    mb_err_t status = mb_transport_recv(tcp->iface, chunk, sizeof chunk, &io);

    if (status == MB_OK && io.processed > 0U) {
        mb_err_t append = mb_tcp_append_rx(tcp, chunk, io.processed);
        if (!mb_err_is_ok(append)) {
            mb_tcp_invoke_callback(tcp, NULL, 0U, append);
            return append;
        }
    } else if (status != MB_ERR_TIMEOUT) {
        if (!mb_err_is_ok(status)) {
            mb_tcp_invoke_callback(tcp, NULL, 0U, status);
        }
        return status;
    }

    return mb_tcp_process_frame(tcp);
}

#endif /* MB_CONF_TRANSPORT_TCP */
