#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <math.h>
#include <string.h>


#include "network.h"
#include "stai.h"


// Protocollo di comunicazione tra il PC e la board
#define MAGIC_0     0xAA
#define MAGIC_1     0xBB
#define TYPE_GRAY   0x01   // PC → Board: immagine GRAY 
#define TYPE_BBOX   0x02   // Board → PC: bounding box 
#define TYPE_ACK    0x03   // Board → PC: pronto 
#define TYPE_ERR    0x04   // Board → PC: errore 
#define TYPE_NONE   0x05   // Board → PC: nessuna targa
#define TYPE_CROP   0x06   // Board A → Board B: pixel del crop
#define TYPE_TEXT   0x07   // Board B → Board A: testo OCR 


// Dimensioni dell'immagine
#define IMG_W       192
#define IMG_H       192
#define GRAY_SIZE    (IMG_W * IMG_H * 1)
#define NUM_ANCHORS 756


// Dimensioni crop output per OCR (input atteso da Board B)
#define OCR_W           128
#define OCR_H           64
#define OCR_SIZE        (OCR_W * OCR_H)


// Padding attorno al bbox prima del resize
#define CROP_PAD        5


// Confidenze per la NMS
#define CONF_THRESHOLD  0.5f
#define IOU_THRESHOLD   0.5f
#define MAX_DETECTIONS  10


// Gestione della USART3 verso il PC
const struct device *uart_pc = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
RING_BUF_DECLARE(rx_rb_pc, 8192);


// Callback della USART3 ogni volta che arriva un byte lo scrive nel buffer
static void uart_cb_pc(const struct device *dev, void *user_data)
{
    if (!uart_irq_update(dev)) return;
    if (!uart_irq_rx_ready(dev)) return;
    uint8_t tmp[64];
    int n = uart_fifo_read(dev, tmp, sizeof(tmp));
    if (n > 0) ring_buf_put(&rx_rb_pc, tmp, n);
}


// Gestione della USART2 verso la Board B
const struct device *uart_b2b = DEVICE_DT_GET(DT_NODELABEL(usart2));
RING_BUF_DECLARE(rx_rb_b2b, 256);   


// Callback della USART2 tra le board
static void uart_cb_b2b(const struct device *dev, void *user_data)
{
    if (!uart_irq_update(dev)) return;
    if (!uart_irq_rx_ready(dev)) return;
    uint8_t tmp[64];
    int n = uart_fifo_read(dev, tmp, sizeof(tmp));
    if (n > 0) ring_buf_put(&rx_rb_b2b, tmp, n);
}

// Primitive di LETTURA e SCRITTURA con il PC
// Funzione che legge dal ring buffer e si addormenta finchè non c'è un byte
static uint8_t read_byte_pc(void)
{
    uint8_t c;
    while (ring_buf_get(&rx_rb_pc, &c, 1) == 0) k_sleep(K_MSEC(1));
    return c;
}

static void write_byte_pc(uint8_t b) { uart_poll_out(uart_pc, b); }

static void write_buf_pc(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) write_byte_pc(buf[i]);
}


// Primitive di LETTURA e SCRITTURA con la Board B
static uint8_t read_byte_b2b(void)
{
    uint8_t c;
    while (ring_buf_get(&rx_rb_b2b, &c, 1) == 0) k_sleep(K_MSEC(1));
    return c;
}

static void write_byte_b2b(uint8_t b) { uart_poll_out(uart_b2b, b); }

static void write_buf_b2b(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) write_byte_b2b(buf[i]);
}


// Funzione che costruisce il frame del messaggio
static void send_msg_on(void (*wb)(uint8_t), void (*wbuf)(const uint8_t *, uint32_t),
                        uint8_t type, const uint8_t *payload, uint32_t len)
{
    wb(MAGIC_0); wb(MAGIC_1); wb(type);
    wb((len >> 24) & 0xFF);
    wb((len >> 16) & 0xFF);
    wb((len >>  8) & 0xFF);
    wb( len        & 0xFF);
    if (payload && len > 0) wbuf(payload, len);
}


static int recv_msg_on(uint8_t (*rb)(void),
                       uint8_t *type, uint8_t *payload,
                       uint32_t max_len, uint32_t *out_len)
{
    while (1) {
        uint8_t b = rb();
        if (b != MAGIC_0) continue;
        b = rb();
        if (b == MAGIC_1) break;
    }
    *type = rb();
    uint32_t size = ((uint32_t)rb() << 24) |
                    ((uint32_t)rb() << 16) |
                    ((uint32_t)rb() <<  8) |
                     (uint32_t)rb();
    if (size > max_len) return -1;
    for (uint32_t i = 0; i < size; i++) payload[i] = rb();
    *out_len = size;
    return 0;
}


#define send_msg_pc(type, payload, len)  \
    send_msg_on(write_byte_pc,  write_buf_pc,  type, payload, len)
#define send_msg_b2b(type, payload, len) \
    send_msg_on(write_byte_b2b, write_buf_b2b, type, payload, len)
#define recv_msg_pc(type, payload, max, out)  \
    recv_msg_on(read_byte_pc,  type, payload, max, out)
#define recv_msg_b2b(type, payload, max, out) \
    recv_msg_on(read_byte_b2b, type, payload, max, out)


// Gestione della memoria con union così risparmio RAm per immagine ricevuta e ingresso nella rete
static union {
    uint8_t gray[GRAY_SIZE];                  
    int8_t  net_in[STAI_NETWORK_IN_1_SIZE];  
} io_buf __attribute__((aligned(4)));


// Buffer allocati per la rete neurale
static int8_t out_boxes[STAI_NETWORK_OUT_1_SIZE]
    __attribute__((aligned(STAI_NETWORK_OUT_1_ALIGNMENT)));


static int8_t out_conf[STAI_NETWORK_OUT_2_SIZE]
    __attribute__((aligned(STAI_NETWORK_OUT_2_ALIGNMENT)));


static uint8_t act_buf[STAI_NETWORK_ACTIVATIONS_SIZE_BYTES]
    __attribute__((aligned(STAI_NETWORK_ACTIVATION_1_ALIGNMENT)));


static uint8_t net_ctx[STAI_NETWORK_CONTEXT_SIZE]
    __attribute__((aligned(STAI_NETWORK_CONTEXT_ALIGNMENT)));


static uint8_t crop_buf[4 + OCR_SIZE];  // Buffer del crop ridimensionato


static uint8_t ocr_result[64];  // Buffer della risposta OCR da Board B


// Bounding Box
typedef struct { float x, y, w, h, conf; } BBox;


// Dequantizzazione che riconverte in float
static float dequant_boxes(int8_t v)
{
    return (v - STAI_NETWORK_OUT_1_ZERO_POINT) * STAI_NETWORK_OUT_1_SCALE;
}


static float dequant_conf(int8_t v)
{
    return (v - STAI_NETWORK_OUT_2_ZERO_POINT) * STAI_NETWORK_OUT_2_SCALE;
}


// Calcolo della Intersection over Union tra due bounding box
static float iou(BBox *a, BBox *b)
{
    float ax1=a->x-a->w/2, ay1=a->y-a->h/2;
    float ax2=a->x+a->w/2, ay2=a->y+a->h/2;
    float bx1=b->x-b->w/2, by1=b->y-b->h/2;
    float bx2=b->x+b->w/2, by2=b->y+b->h/2;
    float ix1=ax1>bx1?ax1:bx1, iy1=ay1>by1?ay1:by1;
    float ix2=ax2<bx2?ax2:bx2, iy2=ay2<by2?ay2:by2;
    if (ix2<=ix1 || iy2<=iy1) return 0.0f;
    float inter=(ix2-ix1)*(iy2-iy1);
    return inter/((ax2-ax1)*(ay2-ay1)+(bx2-bx1)*(by2-by1)-inter);
}


// Per ogni anchor con confidenza sopra soglia dequantizza e salva le coordinate
// poi applica la NMS al box con confidenza massima
static int parse_output(BBox *best)
{
    BBox candidates[MAX_DETECTIONS];
    int n = 0;


    for (int i = 0; i < NUM_ANCHORS; i++) {
        float conf = dequant_conf(out_conf[i]);
        if (conf < CONF_THRESHOLD) continue;
        if (n >= MAX_DETECTIONS) break;


        candidates[n].x    = dequant_boxes(out_boxes[0 * NUM_ANCHORS + i]) / IMG_W;
        candidates[n].y    = dequant_boxes(out_boxes[1 * NUM_ANCHORS + i]) / IMG_H;
        candidates[n].w    = dequant_boxes(out_boxes[2 * NUM_ANCHORS + i]) / IMG_W;
        candidates[n].h    = dequant_boxes(out_boxes[3 * NUM_ANCHORS + i]) / IMG_H;
        candidates[n].conf = conf;
        n++;
    }


    if (n == 0) return 0;


    // Confidenza massima
    int best_idx = 0;
    for (int i = 1; i < n; i++)
        if (candidates[i].conf > candidates[best_idx].conf) best_idx = i;


    // Applica la NMS
    for (int i = 0; i < n; i++) {
        if (i == best_idx) continue;
        if (iou(&candidates[best_idx], &candidates[i]) > IOU_THRESHOLD)
            candidates[i].conf = 0.0f;
    }


    *best = candidates[best_idx];
    return 1;
}


// Funzione che costruisce il crop sulla targa ridimensionando correttamente
static void build_crop(BBox *best)
{
    int x1 = (int)((best->x - best->w / 2) * IMG_W) - CROP_PAD;
    int y1 = (int)((best->y - best->h / 2) * IMG_H) - CROP_PAD;
    int x2 = (int)((best->x + best->w / 2) * IMG_W) + CROP_PAD;
    int y2 = (int)((best->y + best->h / 2) * IMG_H) + CROP_PAD;

    if (x1 < 0)     x1 = 0;
    if (y1 < 0)     y1 = 0;
    if (x2 > IMG_W) x2 = IMG_W;
    if (y2 > IMG_H) y2 = IMG_H;

    int src_w = x2 - x1;
    int src_h = y2 - y1;

    // Header con dimensioni fisse 
    crop_buf[0] = (OCR_W >> 8) & 0xFF;
    crop_buf[1] =  OCR_W       & 0xFF;
    crop_buf[2] = (OCR_H >> 8) & 0xFF;
    crop_buf[3] =  OCR_H       & 0xFF;

    // Resize fatto con nearest-neighbor
    for (int dy = 0; dy < OCR_H; dy++) {
        int sy = y1 + (dy * src_h) / OCR_H;
        for (int dx = 0; dx < OCR_W; dx++) {
            int sx = x1 + (dx * src_w) / OCR_W;
            crop_buf[4 + dy * OCR_W + dx] = io_buf.gray[sy * IMG_W + sx];
        }
    }
}

int main(void)
{
    // Inizializzazione USART3 per PC
    if (!device_is_ready(uart_pc)) return -1;
    uart_irq_callback_set(uart_pc, uart_cb_pc);
    uart_irq_rx_enable(uart_pc);

    // Inizializzazione USART2 per Board B
    if (!device_is_ready(uart_b2b)) {
        printk("USART2 (Board B) non pronta!\n");
        uint8_t e = 0x05;
        send_msg_pc(TYPE_ERR, &e, 2);
        return -1;
    }
    uart_irq_callback_set(uart_b2b, uart_cb_b2b);
    uart_irq_rx_enable(uart_b2b);


    k_sleep(K_MSEC(500));


    // Inizializzazione della rete YOLO
    stai_network *net = (stai_network *)net_ctx;
    if (stai_network_init(net) != STAI_SUCCESS) {
        uint8_t e = 0x01; send_msg_pc(TYPE_ERR, &e, 1); return -1;
    }


    stai_ptr acts[]    = { (stai_ptr)act_buf       };
    stai_ptr inputs[]  = { (stai_ptr)io_buf.net_in };
    stai_ptr outputs[] = { (stai_ptr)out_boxes, (stai_ptr)out_conf };
    stai_network_set_activations(net, acts,    STAI_NETWORK_ACTIVATIONS_NUM);
    stai_network_set_inputs(net,     inputs,   STAI_NETWORK_IN_NUM);
    stai_network_set_outputs(net,    outputs,  STAI_NETWORK_OUT_NUM);


    // Attesa ACK da Board B prima di segnalare prontezza al PC
    uint8_t b2b_type; uint32_t b2b_len;
    recv_msg_b2b(&b2b_type, ocr_result, sizeof(ocr_result), &b2b_len);

    send_msg_pc(TYPE_ACK, NULL, 0);


    while (1) {
        uint8_t type; uint32_t len;


        // Ricezione immagine grigia
        if (recv_msg_pc(&type, io_buf.gray, GRAY_SIZE, &len) != 0) {
            uint8_t e = 0x02; send_msg_pc(TYPE_ERR, &e, 1); continue;
        }
        if (type != TYPE_GRAY || len != GRAY_SIZE) {
            uint8_t e = 0x03; send_msg_pc(TYPE_ERR, &e, 1); continue;
        }


        // Quantizzazione
        for (int i = 0; i < STAI_NETWORK_IN_1_SIZE; i++)
            io_buf.net_in[i] = (int8_t)((int)io_buf.gray[i] - 128);


        // Inferenza
        stai_return_code rc = stai_network_run(net, STAI_MODE_SYNC);
        if (rc != STAI_SUCCESS) {
            uint8_t e[2] = { 0x04, (uint8_t)(rc & 0xFF) };
            send_msg_pc(TYPE_ERR, e, 2); continue;
        }


        // Post-processing con parse_output
        BBox best;
        if (!parse_output(&best)) {
            send_msg_pc(TYPE_NONE, NULL, 0);
            continue;
        }


        for (int i = 0; i < GRAY_SIZE; i++)
            io_buf.gray[i] = (uint8_t)((int)io_buf.net_in[i] + 128);


        // Ritaglio del Bounding Box
        build_crop(&best);


        // Invio del crop alla Board B
        send_msg_b2b(TYPE_CROP, crop_buf + 4, OCR_SIZE);


        // Attesa del risultato OCR da Board B
        uint8_t ocr_type;
        uint32_t ocr_len;
        if (recv_msg_b2b(&ocr_type, ocr_result, sizeof(ocr_result) - 1, &ocr_len) != 0) {
            uint8_t e = 0x05; send_msg_pc(TYPE_ERR, &e, 1); continue;
        }
        ocr_result[ocr_len] = '\0';

        // Manda il risultato al PC
        if (ocr_type == TYPE_TEXT) {
            send_msg_pc(TYPE_TEXT, ocr_result, ocr_len);
        } else {
            send_msg_pc(TYPE_NONE, NULL, 0);
        }
    }
}
