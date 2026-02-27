# Uitleg: MIDI Knoppenmatrix op STM32H5

Dit document legt de werking van de code uit met nadruk op de **MIDI noot-mapping formule**.

---

## Inhoudsopgave

1. [Wat doet dit project?](#wat-doet-dit-project)
2. [De knoppenmatrix](#de-knoppenmatrix)
3. [De MIDI noot-mapping formule](#de-midi-noot-mapping-formule)
4. [Hoe werkt de hardware-scan?](#hoe-werkt-de-hardware-scan)
5. [Debouncing: waarom en hoe?](#debouncing-waarom-en-hoe)
6. [MIDI-berichten versturen](#midi-berichten-versturen)
7. [De hoofdlus](#de-hoofdlus)

---

## Wat doet dit project?

De STM32H5 microcontroller leest een **4×4 matrix van 16 knoppen** uit via een externe I/O-expander (MCP23S17).  
Elke knop stuurt een **MIDI Note ON of Note OFF bericht** via USB naar een computer.  
Zo werkt het apparaat als een eenvoudig MIDI-instrument (zoals een drumpad of keyboard).

---

## De knoppenmatrix

### Configuratie (in `main.h`)

```c
#define MATRIX_ROWS  4   // Aantal rijen in de matrix
#define MATRIX_COLS  4   // Aantal kolommen in de matrix
#define BASE_NOTE   60   // BASIS_NOOT = 60 = Midden C (C4)
```

### Toestand bijhouden (in `main.c`)

Voor elke knop slaan we vier dingen op in een `struct`:

```c
typedef struct {
  bool current_state;        // Huidige toestand: true = ingedrukt, false = losgelaten
  bool previous_state;       // Vorige toestand (voor vergelijking)
  uint32_t last_change_time; // Tijdstip (ms) van de laatste verandering
  bool debounce_stable;      // true = verandering is stabiel, klaar om te verwerken
} ButtonState_t;

// Eén array voor alle 16 knoppen, geordend per [rij][kolom]
ButtonState_t button_matrix[MATRIX_ROWS][MATRIX_COLS];
```

---

## De MIDI noot-mapping formule

### De formule

$$\text{midi\_noot} = \text{BASIS\_NOOT} + (\text{rij} \times 4) + \text{kolom}$$

Met `BASIS_NOOT = 60` (Midden C, ook wel **C4** genoemd).

### Uitleg

| Onderdeel | Betekenis |
|---|---|
| `BASIS_NOOT` (60) | Startpunt: knop linksboven speelt C4 |
| `rij × 4` | Elke rij lager = 4 halve tonen hoger (want 4 kolommen per rij) |
| `kolom` | Elke stap naar rechts = 1 halve toon hoger |

### Code (in `main.c`)

```c
// Pas de formule toe: midi_noot = BASIS_NOOT + (rij x 4) + kolom
uint8_t midi_note = BASE_NOTE + (row * 4) + col;
```

### Voorbeelden

| Knop | Berekening | MIDI-noot | Naam |
|---|---|---|---|
| Rij 0, Kolom 0 | 60 + (0×4) + 0 = **60** | 60 | C4 (Midden C) |
| Rij 0, Kolom 1 | 60 + (0×4) + 1 = **61** | 61 | C#4 |
| Rij 0, Kolom 3 | 60 + (0×4) + 3 = **63** | 63 | D#4 |
| Rij 1, Kolom 0 | 60 + (1×4) + 0 = **64** | 64 | E4 |
| Rij 2, Kolom 1 | 60 + (2×4) + 1 = **69** | 69 | A4 (concertstemtoon) |
| Rij 3, Kolom 3 | 60 + (3×4) + 3 = **75** | 75 | D#5 |

### Volledige mapping van alle 16 knoppen

```
         Kolom 0    Kolom 1    Kolom 2    Kolom 3
        +----------+----------+----------+----------+
Rij 0  |  60  C4  |  61  C#4 |  62  D4  |  63  D#4 |
        +----------+----------+----------+----------+
Rij 1  |  64  E4  |  65  F4  |  66  F#4 |  67  G4  |
        +----------+----------+----------+----------+
Rij 2  |  68  G#4 |  69  A4  |  70  A#4 |  71  B4  |
        +----------+----------+----------+----------+
Rij 3  |  72  C5  |  73  C#5 |  74  D5  |  75  D#5 |
        +----------+----------+----------+----------+
```

Met de 16 knoppen speel je de noten **C4 t/m D#5** — dat is één octaaf plus een grote secunde.

---

## Hoe werkt de hardware-scan?

De MCP23S17 is een I/O-expander die via SPI is aangesloten:
- **Port A (GPA0–GPA3)**: kolommen — geconfigureerd als **uitgangen**
- **Port B (GPB0–GPB3)**: rijen — geconfigureerd als **ingangen met pull-up weerstand**

### Principe: één kolom per keer activeren

We zetten telkens **één kolom LOW** en lezen dan de vier rijen.  
Als een knop in die kolom ingedrukt is, verbindt hij de rij met de lage kolom → de rij leest **LOW**.

```c
for (uint8_t col = 0; col < MATRIX_COLS; col++) {

    // Zet de actieve kolom LOW, alle andere HIGH.
    // Voorbeeld: col=0 → col_pattern = 0b11111110
    uint8_t col_pattern = ~(1 << col);
    MCP23S17_WriteRegister(MCP23S17_GPIOA, col_pattern);

    // Kleine vertraging zodat het signaal stabiel is
    for(volatile int i = 0; i < 100; i++); // ca. 10 microseconden

    // Lees de 4 rijen: bit = 0 betekent knop ingedrukt
    uint8_t row_values = MCP23S17_ReadRegister(MCP23S17_GPIOB);

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        // '!' keert om: LOW (0) → true (ingedrukt)
        bool is_pressed = !(row_values & (1 << row));
        // ... zie debounce hieronder
    }
}

// Na de scan: alle kolommen weer HIGH (inactief)
MCP23S17_WriteRegister(MCP23S17_GPIOA, 0xFF);
```

> **Waarom maar elke 5 ms scannen?**  
> Om de processor niet onnodig te belasten. 5 ms is snel genoeg — mensen kunnen niet sneller dan ~50 ms drukken.

```c
if (now - last_scan < 5) {
    return; // Nog geen 5 ms verstreken → sla over
}
```

---

## Debouncing: waarom en hoe?

### Het probleem

Mechanische knoppen "stuiteren" (**bounce**) even bij indrukken of loslaten.  
In die eerste milliseconden wisselt het signaal razendsnel tussen LOW en HIGH.  
Zonder debounce zou je voor één druk tientallen MIDI-berichten sturen.

### De oplossing

We accepteren een toestandsverandering pas als die **minstens 20 ms stabiel** is.

```c
#define DEBOUNCE_TIME_MS  20  // Wacht 20 ms voor we de toestand accepteren
```

```c
if (is_pressed != btn->current_state) {
    // Er is een verschil met de vorige toestand

    if ((now - btn->last_change_time) > DEBOUNCE_TIME_MS) {
        // Genoeg tijd verstreken → toestandsverandering is echt

        btn->previous_state = btn->current_state;
        btn->current_state = is_pressed;    // Sla nieuwe toestand op
        btn->last_change_time = now;         // Onthoud het tijdstip
        btn->debounce_stable = true;         // Klaarzetten voor verwerking
    }
}
```

---

## MIDI-berichten versturen

### Het MIDI-berichtformaat

Een MIDI-bericht bestaat uit **3 bytes**:

| Byte | Naam | Waarde indrukken | Waarde loslaten |
|---|---|---|---|
| Byte 0 | Status (commando) | `0x90` = Note ON, kanaal 1 | `0x80` = Note OFF, kanaal 1 |
| Byte 1 | Nootnummer | bijv. `60` voor C4 | zelfde noot |
| Byte 2 | Velocity (kracht) | `127` = maximaal | `0` = stil |

### Code (in `main.c`)

```c
// Bereken het MIDI-nootnummer met de formule
uint8_t midi_note = BASE_NOTE + (row * 4) + col;

if (btn->current_state) {
    // Knop INGEDRUKT → stuur Note ON
    uint8_t note_on[3] = {0x90, midi_note, 127};

    if (tud_mounted()) {                     // Is de USB-verbinding actief?
        tud_midi_stream_write(0, note_on, 3); // Stuur 3 bytes via USB-MIDI
    }
} else {
    // Knop LOSGELATEN → stuur Note OFF
    uint8_t note_off[3] = {0x80, midi_note, 0};

    if (tud_mounted()) {
        tud_midi_stream_write(0, note_off, 3);
    }
}

// Markeer als verwerkt zodat we dit bericht niet opnieuw sturen
btn->debounce_stable = false;
```

---

## De hoofdlus

Het programma herhaalt drie stappen eindeloos:

```c
while (1)
{
    // Stap 1: Houd de USB-verbinding actief en verwerk inkomende/uitgaande data
    tud_task();

    // Stap 2: Scan de 16 knoppen via de MCP23S17 (elke 5 ms)
    ScanButtonMatrix();

    // Stap 3: Berekend MIDI-nootnummer en stuur Note ON/OFF berichten
    ProcessButtonEvents();
}
```

### Samenvatting van de stroom

```
Loop
 │
 ├─ tud_task()           → USB actief houden
 │
 ├─ ScanButtonMatrix()   → MCP23S17 uitlezen via SPI
 │    │
 │    ├─ Kolom LOW zetten
 │    ├─ Rijen lezen
 │    └─ Debounce controleren
 │
 └─ ProcessButtonEvents()
      │
      ├─ midi_note = 60 + (rij × 4) + kolom
      ├─ Knop ingedrukt? → Note ON  (0x90, noot, 127)
      └─ Knop losgelaten? → Note OFF (0x80, noot, 0)
```
