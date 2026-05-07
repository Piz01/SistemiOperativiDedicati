#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <string.h>

#include "network.h"
#include "stai.h"

// Protocollo di comunicazione tra il PC e la board
#define MAGIC_0      0xAA
#define MAGIC_1      0xBB
#define TYPE_ACK     0x03   // B → A: pronto
#define TYPE_ERR     0x04   // B → A: errore
#define TYPE_NONE    0x05   // B → A: nessuna targa
#define TYPE_CROP    0x06   // A → B: crop 128×64 grayscale
#define TYPE_TEXT    0x07   // B → A: stringa targa

// Dimensioni dell'immagine
#define IMG_W        128
#define IMG_H         64
#define GRAY_SIZE    (IMG_W * IMG_H)   // 8192 byte — quello che arriva via UART
#define MSG_HEADER_SIZE  7

// Parametri OCR
#define NUM_SLOTS    9     // posizioni della targa
#define NUM_CLASSES  37   // 0-9 + A-Z + blank '_'
static const char ALPHABET[NUM_CLASSES] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_";

// Gestione della USART2 per comunicazione con la board
#define UART_B2B     DT_NODELABEL(usart2)

const struct device *uart_b2b = DEVICE_DT_GET(UART_B2B);
RING_BUF_DECLARE(rx_rb, GRAY_SIZE + MSG_HEADER_SIZE);

// Callback della UART: ogni volta che arriva un byte lo scrive nel buffer
static void uart_cb(const struct device *dev, void *user_data)
{
    if (!uart_irq_update(dev)) return;
    if (!uart_irq_rx_ready(dev)) return;
    uint8_t tmp[64];
    int n = uart_fifo_read(dev, tmp, sizeof(tmp));
    if (n > 0) ring_buf_put(&rx_rb, tmp, n);
}

// Legge dal ring buffer, si addormenta finché non c'è un byte
static uint8_t read_byte(void)
{
    uint8_t c;
    while (ring_buf_get(&rx_rb, &c, 1) == 0) k_sleep(K_MSEC(1));
    return c;
}

static void write_byte(uint8_t b) { uart_poll_out(uart_b2b, b); }

static void write_buf(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) write_byte(buf[i]);
}

// Costruisce e invia un frame del protocollo
static void send_msg_b2b(uint8_t type, const uint8_t *payload, uint32_t len)
{
    write_byte(MAGIC_0);
    write_byte(MAGIC_1);
    write_byte(type);
    write_byte((len >> 24) & 0xFF);
    write_byte((len >> 16) & 0xFF);
    write_byte((len >>  8) & 0xFF);
    write_byte( len        & 0xFF);
    if (payload && len > 0) write_buf(payload, len);
}

static int recv_msg_b2b(uint8_t *type, uint8_t *payload,
                        uint32_t max_len, uint32_t *out_len)
{
    while (1) {
        uint8_t b = read_byte();
        if (b != MAGIC_0) continue;
        b = read_byte();
        if (b == MAGIC_1) break;
    }
    *type = read_byte();
    uint32_t size = ((uint32_t)read_byte() << 24) |
                    ((uint32_t)read_byte() << 16) |
                    ((uint32_t)read_byte() <<  8) |
                     (uint32_t)read_byte();
    if (size > max_len) { send_msg_b2b(TYPE_ERR, NULL, 0); return -1; }
    for (uint32_t i = 0; i < size; i++) payload[i] = read_byte();
    *out_len = size;
    return 0;
}

// Union: gray (8192 B ricevuti via UART) e net_in (24576 B = 64×128×3) condividono la RAM.
// net_in è il membro più grande, la union ha dimensione sufficiente per entrambi.
static union {
    uint8_t gray[GRAY_SIZE];
    uint8_t net_in[STAI_NETWORK_IN_1_SIZE];
} io_buf __attribute__((aligned(4)));

// Buffer output OCR: layout [NUM_SLOTS * NUM_CLASSES]
static int8_t out_ocr[STAI_NETWORK_OUT_1_SIZE]
    __attribute__((aligned(STAI_NETWORK_OUT_1_ALIGNMENT)));

static uint8_t act_buf[STAI_NETWORK_ACTIVATIONS_SIZE_BYTES]
    __attribute__((aligned(STAI_NETWORK_ACTIVATION_1_ALIGNMENT)));

static uint8_t net_ctx[STAI_NETWORK_CONTEXT_SIZE]
    __attribute__((aligned(STAI_NETWORK_CONTEXT_ALIGNMENT)));

// Dequantizzazione dell'output OCR
static float dequant_ocr(int8_t v)
{
    return (v - STAI_NETWORK_OUT_1_ZERO_POINT) * STAI_NETWORK_OUT_1_SCALE;
}

// Argmax per ogni slot → carattere. Ritorna il numero di caratteri validi (esclude '_').
static int decode_plate(char *out_str)
{
    int n_chars = 0;
    for (int slot = 0; slot < NUM_SLOTS; slot++) {
        int   best_cls   = 0;
        float best_score = -1e9f;
        for (int cls = 0; cls < NUM_CLASSES; cls++) {
            float score = dequant_ocr(out_ocr[slot * NUM_CLASSES + cls]);
            if (score > best_score) { best_score = score; best_cls = cls; }
        }
        if (ALPHABET[best_cls] == '_') break;
        out_str[n_chars++] = ALPHABET[best_cls];
    }
    out_str[n_chars] = '\0';
    return n_chars;
}

int main(void)
{
    if (!device_is_ready(uart_b2b)) return -1;
    uart_irq_callback_set(uart_b2b, uart_cb);
    uart_irq_rx_enable(uart_b2b);

    k_sleep(K_MSEC(500));

    // Inizializzazione della rete
    stai_network *net = (stai_network *)net_ctx;
    if (stai_network_init(net) != STAI_SUCCESS) {
        uint8_t e = 0x01; send_msg_b2b(TYPE_ERR, &e, 1); return -1;
    }

    stai_ptr acts[]    = { (stai_ptr)act_buf       };
    stai_ptr inputs[]  = { (stai_ptr)io_buf.net_in };
    stai_ptr outputs[] = { (stai_ptr)out_ocr        };
    stai_network_set_activations(net, acts,    STAI_NETWORK_ACTIVATIONS_NUM);
    stai_network_set_inputs(net,     inputs,   STAI_NETWORK_IN_NUM);
    stai_network_set_outputs(net,    outputs,  STAI_NETWORK_OUT_NUM);

    // Segnala a Board A che Board B è pronta
    send_msg_b2b(TYPE_ACK, NULL, 0);

    while (1) {
        uint8_t type;
        uint32_t len;

        // 1. Ricevi crop da Board A (GRAY_SIZE byte)
        if (recv_msg_b2b(&type, io_buf.gray, GRAY_SIZE, &len) != 0) {
            uint8_t e = 0x02; send_msg_b2b(TYPE_ERR, &e, 1); continue;
        }
        if (type != TYPE_CROP || len != GRAY_SIZE) {
            uint8_t e = 0x03; send_msg_b2b(TYPE_ERR, &e, 1); continue;
        }

        // 2. Espansione gray → RGB in-place (dal fondo per non sovrascrivere)
        for (int i = GRAY_SIZE - 1; i >= 0; i--) {
            uint8_t g = io_buf.gray[i];
            io_buf.net_in[i * 3 + 0] = g;  // R
            io_buf.net_in[i * 3 + 1] = g;  // G
            io_buf.net_in[i * 3 + 2] = g;  // B
        }

        // 3. Inferenza OCR
        stai_return_code rc = stai_network_run(net, STAI_MODE_SYNC);
        if (rc != STAI_SUCCESS) {
            uint8_t e[2] = { 0x04, (uint8_t)(rc & 0xFF) };
            send_msg_b2b(TYPE_ERR, e, 2); continue;
        }

        // 4. Decodifica targa e risposta a Board A
        char plate[NUM_SLOTS + 1];
        int n_chars = decode_plate(plate);

        if (n_chars == 0) {
            send_msg_b2b(TYPE_NONE, NULL, 0);
        } else {
            send_msg_b2b(TYPE_TEXT, (uint8_t *)plate, (uint32_t)n_chars);
        }
    }
}