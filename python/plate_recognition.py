import serial
import time
import cv2
import numpy as np
import struct

PORT     = "COM3"
BAUD     = 115200
IMG_W    = 192
IMG_H    = 192
IMG_PATH = r"plate_dataset\plate_1.jpg"

# Configurazione del protocollo
MAGIC_0   = 0xAA
MAGIC_1   = 0xBB
TYPE_GRAY = 0x01   
TYPE_BBOX = 0x02
TYPE_ACK  = 0x03
TYPE_ERR  = 0x04
TYPE_NONE = 0x05
TYPE_CROP = 0x06   
TYPE_TEXT = 0x07 


# Invio messaggio UART
def send_msg(ser, msg_type, payload=b""):
    size = len(payload)
    header = bytes([
        MAGIC_0, MAGIC_1, msg_type,
        (size >> 24) & 0xFF,
        (size >> 16) & 0xFF,
        (size >>  8) & 0xFF,
         size        & 0xFF
    ])
    ser.write(header + payload)
    ser.flush()


# Ricezione messaggio UART ricercando MAGIC_0 e MAGIC_1
def recv_msg(ser):
    while True:
        b = ser.read(1)
        if not b:
            raise TimeoutError("Timeout sync")
        if b[0] != MAGIC_0:
            continue
        b = ser.read(1)
        if b and b[0] == MAGIC_1:
            break
    msg_type = ser.read(1)[0]
    size_bytes = ser.read(4)
    size = ((size_bytes[0] << 24) | (size_bytes[1] << 16) |
            (size_bytes[2] <<  8) |  size_bytes[3])
    payload = b""
    while len(payload) < size:
        chunk = ser.read(size - len(payload))
        if not chunk:
            raise TimeoutError("Timeout payload")
        payload += chunk
    return msg_type, payload


# Pre-processing dell'immagine per inviarla alla board
def prepara_immagine(path):
    img = cv2.imread(path)
    if img is None:
        raise FileNotFoundError(f"Immagine non trovata: {path}")
    img = cv2.resize(img, (IMG_W, IMG_H))        # Ridimensione dell'immagine
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)   # Conversione in grayscale
    gray_chw = gray[np.newaxis, :, :]               # Aggiunta della dimensione del canale in formato CHW
    return gray_chw.astype(np.uint8).tobytes()


# Funzione che visualizza il risultato con la targa riconosciuta
def mostra_risultato(path, plate_text):
    """Mostra l'immagine originale con la targa riconosciuta sovrapposta."""
    img = cv2.resize(cv2.imread(path), (IMG_W, IMG_H))
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    img_draw = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)

    cv2.putText(img_draw, plate_text,
                (10, IMG_H - 10),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
    cv2.imwrite("result.jpg", img_draw)
    cv2.imshow("Targa riconosciuta", img_draw)
    cv2.waitKey(0)


data = prepara_immagine(IMG_PATH)
print(f"Immagine: {IMG_W}x{IMG_H} GRAY CHW = {len(data)} byte")

with serial.Serial(PORT, BAUD, timeout=30) as ser:
    time.sleep(1)

    print("Attendo boot board A + board B...")
    msg_type, _ = recv_msg(ser)
    if msg_type != TYPE_ACK:
        print(f"Errore boot: risposta inattesa 0x{msg_type:02X}")
        exit(1)
    print("Sistema pronto (Board A + Board B inizializzate)")

    print("Invio immagine GRAY...")
    send_msg(ser, TYPE_GRAY, data)

    print("Attendo risultato OCR...")
    msg_type, payload = recv_msg(ser)

    if msg_type == TYPE_TEXT:
        plate = payload.decode("ascii")
        print(f"Targa riconosciuta: {plate}")
        mostra_risultato(IMG_PATH, plate)

    elif msg_type == TYPE_NONE:
        print("Nessuna targa rilevata")

    elif msg_type == TYPE_ERR:
        codes = {
            0x01: "Init rete Board A fallita",
            0x02: "Ricezione immagine fallita",
            0x03: "Tipo o dimensione immagine sbagliata",
            0x04: "Inferenza YOLO fallita",
            0x05: "Ricezione OCR da Board B fallita",
        }
        code = payload[0] if payload else 0
        print(f"Errore: {codes.get(code, 'sconosciuto')} (0x{code:02X})")

    else:
        print(f"Messaggio inatteso: type=0x{msg_type:02X}, payload={list(payload)}")