# Riconoscimento targhe distribuito su due Nucleo STM32

Progetto per il corso di **Sistemi Operativi Dedicati** — Università Politecnica delle Marche, A.A. 2025–2026.

Sistema di *Automatic License Plate Recognition* (ALPR) realizzato su due microcontrollori STM32 Nucleo-H745ZI-Q che cooperano via UART per eseguire detection e OCR di targhe automobilistiche, senza supporto di GPU o NPU.

---

## Indice

- [Architettura del sistema](#architettura-del-sistema)
- [Risultati](#risultati)
- [Hardware e collegamenti](#hardware-e-collegamenti)
- [Stack software](#stack-software)
- [Struttura della repository](#struttura-della-repository)
- [Build e flashing](#build-e-flashing)
- [Esecuzione dello script di inferenza](#esecuzione-dello-script-di-inferenza)
- [Protocollo di comunicazione](#protocollo-di-comunicazione)
- [Modelli neurali](#modelli-neurali)
- [Limitazioni note](#limitazioni-note)
- [Autori](#autori)

---

## Architettura del sistema

Il sistema è composto da tre nodi che comunicano via UART seriale a 115200 baud:

```
┌──────────────┐  USART3   ┌──────────────────┐  USART2   ┌──────────────────┐
│   PC host    │──────────▶│   Board A (H745) │──────────▶│   Board B (H745) │
│   (Python)   │           │  YOLO detection  │           │   CCT-XS OCR     │
│              │◀──────────│  + crop targa    │◀──────────│   + decode       │
└──────────────┘           └──────────────────┘           └──────────────────┘
   immagine                    crop 128×64                    stringa targa
   192×192 GRAY                grayscale                      (es. "AB123CD")
```

**Board A — Detection**
1. Riceve dal PC un'immagine 192×192 grayscale (36 864 byte).
2. La quantizza a INT8 (zero-point 128).
3. Esegue inferenza con una rete **YOLOv8-tiny** custom su singolo canale.
4. Dequantizza l'output (756 anchor box + confidenze), applica Non-Maximum Suppression con soglia di confidenza 0,5 e IoU 0,5.
5. Ritaglia la regione contenente la targa con un padding di 5 pixel e la ridimensiona a 128×64 con interpolazione nearest-neighbor.
6. Invia il crop alla Board B.

**Board B — OCR**
1. Riceve il crop 128×64 grayscale (8 192 byte).
2. Espande il singolo canale a RGB in-place (richiesto dall'input della rete).
3. Esegue inferenza con un **Compact Convolutional Transformer XS** (`cct_xs_v1_global` del progetto open-source [fast-plate-ocr](https://github.com/ankandrew/fast-plate-ocr)).
4. Estrae argmax su 9 slot × 37 classi (`0-9 A-Z _`) per ottenere la stringa della targa.
5. Restituisce la stringa alla Board A, che la inoltra al PC.

## Risultati

Il sistema riconosce con successo targhe italiane, svizzere e tedesche, in circa **4,5 secondi end-to-end** per immagine.

Esempi dimostrati:

| Targa reale | Riconosciuta | Tempo (ms) |
|---|---|---|
| GX597MY | GGX597MY | 4594,2 |
| GL814LN | CL814LN | 4593,0 |
| FF144AC | FF144AC | 4597,7 |
| TI 49959 | TI49959 | 4594,6 |
| M:G 5387 | MG5387 | 4594,7 |

In uno dei test (`plate_17.webp`) la pipeline non rileva alcuna targa e termina in ~3,5 s; questo caso è discusso nella sezione [Limitazioni note](#limitazioni-note).

## Hardware e collegamenti

| Componente | Descrizione |
|---|---|
| Board A | STM32 Nucleo-H745ZI-Q (Cortex-M7 @ 480 MHz, 1 MB SRAM totale, 2 MB Flash) |
| Board B | STM32 Nucleo-H745ZI-Q (stessa configurazione) |
| Host PC | Qualsiasi sistema con Python ≥ 3.9 e una porta USB libera |

**Collegamenti UART**

- **PC ↔ Board A**: cavo USB-C / Micro-USB sulla porta ST-Link della Board A, che espone una USART virtuale (per default USART3 nelle Nucleo-H745).
- **Board A ↔ Board B**: collegamento dei pin USART2 incrociati (TX di A su RX di B e viceversa, GND comune).

Pinout USART2 sulla Nucleo-H745 secondo l'overlay devicetree del progetto:

| Funzione | Pin Arduino | Pin MCU |
|---|---|---|
| TX | D53 / morphological | PD5 |
| RX | D52 / morphological | PD6 |
| GND | GND | GND |

> Verificare il collegamento del GND comune fra le due board — senza riferimento di massa la comunicazione UART non funziona.

## Stack software

| Strato | Tecnologia |
|---|---|
| RTOS | Zephyr (build con CMake + KConfig + devicetree overlay) |
| AI runtime | STM32Cube.AI (X-CUBE-AI), libreria `NetworkRuntime1200_CM7_GCC.a` linkata staticamente |
| Toolchain MCU | `arm-none-eabi-gcc` (incluso nello Zephyr SDK) |
| Training | Google Colab + Ultralytics YOLOv8 + Roboflow |
| Host PC | Python 3, OpenCV, NumPy, pyserial |

## Struttura della repository

```
SistemiOperativiDedicati/
├── README.md                              ← questo file
├── YOLO training notebook/
│   └── Training_YOLO_network.ipynb        ← notebook Colab di training e
│                                            export ONNX della rete di detection
├── models/
│   ├── yolov8t_lp_gray_best.pt            ← pesi PyTorch del modello YOLO
│   ├── yolov8t_lp_gray_split.onnx         ← export ONNX con output splittati
│   └── cct_xs_v1_global.onnx              ← modello OCR pre-addestrato
├── python/
│   ├── plate_recognition.py               ← script host di inferenza
│   └── plate_dataset/                     ← 26 immagini di test
└── zephyr/
    ├── board_A_project/                   ← firmware detection
    │   ├── CMakeLists.txt
    │   ├── prj.conf                       ← config Zephyr (UART, FPU, SRAM)
    │   ├── boards/                        ← overlay devicetree H745
    │   ├── cube_ai/                       ← runtime e pesi Cube.AI generati
    │   └── src/main.c                     ← applicazione principale
    └── board_B_project/                   ← firmware OCR
        ├── CMakeLists.txt
        ├── prj.conf
        ├── boards/
        ├── cube_ai/
        └── src/main.c
```

## Build e flashing

### Prerequisiti

- Installazione di Zephyr seguendo la [getting started guide ufficiale](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
- Zephyr SDK con toolchain `arm-zephyr-eabi`
- `west` configurato e funzionante
- ST-Link Utility o `OpenOCD` per il flashing

### Build di ciascuna scheda

Da dentro la cartella di ciascun progetto (`zephyr/board_A_project` o `zephyr/board_B_project`):

```bash
west build -b nucleo_h745zi_q/stm32h745xx/m7 --pristine
```

Per fare il flashing della scheda corrispondente, collegata via USB:

```bash
west flash
```

### Note sul build

- Entrambi i progetti compilano sul **core Cortex-M7** della H745 (il core M4 non viene usato).
- L'opzione `-mfpu=fpv5-d16 -mfloat-abi=hard` è forzata in `CMakeLists.txt` per garantire la compatibilità ABI con la libreria precompilata di X-CUBE-AI.
- La SRAM è mappata a `0x24000000` con dimensione `0x80000` (512 KB) tramite overlay devicetree.

## Esecuzione dello script di inferenza

Dopo aver flashato entrambe le schede e collegato i cavi:

1. Aprire un terminale con Python 3 e creare un virtual environment:
   ```bash
   python -m venv .venv
   source .venv/bin/activate    # su Windows: .venv\Scripts\activate
   pip install opencv-python numpy pyserial
   ```

2. Editare `python/plate_recognition.py` per impostare:
   - `PORT`: la porta seriale della Board A (es. `COM3` su Windows, `/dev/ttyACM0` su Linux).
   - `IMG_PATH`: il percorso dell'immagine da analizzare.

3. Lanciare:
   ```bash
   python python/plate_recognition.py
   ```

L'output atteso è qualcosa come:

```
Immagine: 192x192 GRAY CHW = 36864 byte
Attesa boot board A + board B...
Sistema pronto (Board A + Board B inizializzate)
Invio immagine GRAY...
Attesa risultato OCR...
Targa riconosciuta: GX597MY  [Tempo totale: 4594.2 ms]
```

Lo script visualizza l'immagine elaborata sovrapponendo la targa riconosciuta.

## Protocollo di comunicazione

Tutti i messaggi UART sono incapsulati in **frame con header fisso** per garantire la sincronizzazione:

```
┌──────────┬──────────┬──────┬─────────────────────┬───────────────┐
│ MAGIC_0  │ MAGIC_1  │ TYPE │  LENGTH (4 byte BE) │  PAYLOAD ...  │
│  (0xAA)  │  (0xBB)  │ (1B) │     (big-endian)    │   (N byte)    │
└──────────┴──────────┴──────┴─────────────────────┴───────────────┘
```

I tipi di messaggio definiti sono:

| Codice | Nome | Direzione | Significato |
|---|---|---|---|
| `0x01` | `TYPE_GRAY` | PC → A | Immagine grayscale da analizzare |
| `0x02` | `TYPE_BBOX` | A → PC | Bounding box (riservato, attualmente non usato) |
| `0x03` | `TYPE_ACK` | A → PC / B → A | Acknowledgement, sistema pronto |
| `0x04` | `TYPE_ERR` | varie | Notifica di errore (con codice in payload) |
| `0x05` | `TYPE_NONE` | A → PC | Nessuna targa rilevata |
| `0x06` | `TYPE_CROP` | A → B | Crop della targa per OCR |
| `0x07` | `TYPE_TEXT` | B → A → PC | Stringa di caratteri riconosciuta |

All'avvio del sistema, Board B invia un ACK a Board A, che a sua volta inoltra l'ACK al PC: solo a questo punto il PC può inviare la prima immagine.

## Modelli neurali

### Detection — YOLOv8-tiny custom

Variante di YOLOv8-nano con la scala `t` aggiunta manualmente (width 0,125, esattamente la metà di YOLOv8n). Il modello è stato addestrato a riconoscere una singola classe (`license_plate`) su immagini grayscale a singolo canale e risoluzione 192×192.

Risorse impiegate sulla Board A (lette da `cube_ai/network.h`):

| Risorsa | Valore | Memoria |
|---|---|---|
| Pesi del modello | 617 KB | Flash |
| Activation buffer | 410 KB | RAM |
| Input tensor | 36 KB | RAM |
| Output tensor | ~3,7 KB | RAM |

### OCR — Compact Convolutional Transformer XS

Modello `cct_xs_v1_global` scaricato dal progetto open-source [fast-plate-ocr](https://github.com/ankandrew/fast-plate-ocr) di ankandrew, basato sull'architettura descritta nel paper *"Escaping the Big Data Paradigm with Compact Transformers"* (Hassani et al., 2021, [arXiv:2104.05704](https://arxiv.org/abs/2104.05704)).

Caratteristiche:
- Input: 128×64 RGB (il singolo canale grayscale viene replicato sui 3 canali in-place).
- Output: 9 slot × 37 classi (alfabeto `0-9 A-Z _`, dove `_` è padding).
- Decoding: argmax indipendente per slot, terminazione al primo `_`.

Risorse impiegate sulla Board B:

| Risorsa | Valore | Memoria |
|---|---|---|
| Pesi del modello | 494 KB | Flash |
| Activation buffer | 296 KB | RAM |
| Input tensor | 24 KB | RAM |
| Output tensor | 333 byte | RAM |

## Limitazioni note

- **Latenza elevata (~4,5 s)**: gran parte del tempo è dovuta al trasferimento UART a 115200 baud (un'immagine 192×192 grayscale richiede teoricamente ~3,2 s solo per essere trasferita). Un cambio a SPI ad alta velocità o un baud rate UART più elevato ridurrebbe significativamente questa latenza.
- **Inferenza pura CPU**: la H745 non ha NPU dedicata, quindi le reti girano interamente sul Cortex-M7. Una STM32N6 con Neural-ART ridurrebbe i tempi di inferenza di due ordini di grandezza.
- **Confusione di caratteri**: il modello OCR può confondere caratteri simili (es. `G`→`C`) e occasionalmente duplica caratteri in slot adiacenti (es. `GG` invece di `G`).
- **Falsi negativi**: alcune immagini con targa poco contrastata o angolazioni estreme producono detection a confidenza inferiore a 0,5 e il sistema risponde con `TYPE_NONE` (nessuna targa rilevata).
- **Input statico**: il sistema attualmente accetta una sola immagine alla volta inviata da PC; non c'è acquisizione da camera (sarebbe possibile su una H745 con interfaccia DCMI ma non è stato implementato).

## Autori

- [Anass Chebbaki] 
- [Andrea Pizzuto]
- [Matteo Stronati]
- [Matteo Talè]
- [Roberto Dimitri]

Università Politecnica delle Marche · Corso di laurea in Ingegneria Informatica · A.A. 2025–2026.
