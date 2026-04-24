#pragma once

// ---------------------------------------------------------------------------
// inputs.h
// Debounced reader for two 4-channel optocouplers, one per taillight side.
// Each side exposes: brake, running, turn, reverse.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "config.h"

class Inputs {
public:
    // Call once in setup()
    void begin();

    // Call every loop iteration.  Returns true when any input changed.
    bool update();

    // ── Atomic packed snapshots (safe to read from a different RTOS task) ───
    // Bit layout per byte:  bit0=brake  bit1=running  bit2=turn  bit3=reverse
    // Written as a single atomic byte store at the end of every update().
    uint8_t leftSnapshot()  const { return _leftSnapshot;  }
    uint8_t rightSnapshot() const { return _rightSnapshot; }

    // ── Left-side debounced accessors ────────────────────────────────────────
    bool leftBrake()   const { return _leftBrake;   }
    bool leftRunning() const { return _leftRunning; }
    bool leftTurn()    const { return _leftTurn;    }
    bool leftReverse() const { return _leftReverse; }

    // ── Right-side debounced accessors ───────────────────────────────────────
    bool rightBrake()   const { return _rightBrake;   }
    bool rightRunning() const { return _rightRunning; }
    bool rightTurn()    const { return _rightTurn;    }
    bool rightReverse() const { return _rightReverse; }

private:
    // One debounce tracker per channel
    struct Channel {
        int           pin;
        unsigned long debounceMs   = DEBOUNCE_MS;  // per-channel; set in begin()
        bool          state        = false;
        bool          lastRaw      = false;
        unsigned long lastChangeMs = 0;
    };

    // Channels 0-3: left side  (brake, running, turn, reverse)
    // Channels 4-7: right side (brake, running, turn, reverse)
    Channel _channels[8];

    // All output flags are volatile so the compiler never optimises away reads
    // on Core 1 when Core 0 (the input task) is the writer.
    volatile bool _leftBrake   = false;
    volatile bool _leftRunning = false;
    volatile bool _leftTurn    = false;
    volatile bool _leftReverse = false;

    volatile bool _rightBrake   = false;
    volatile bool _rightRunning = false;
    volatile bool _rightTurn    = false;
    volatile bool _rightReverse = false;

    // Packed atomic snapshots — single byte written at the end of update().
    // On ESP32 (Xtensa LX7), aligned byte stores are atomic, so Core 1 always
    // reads a coherent frame without a mutex.
    // Bit layout: bit0=brake  bit1=running  bit2=turn  bit3=reverse
    volatile uint8_t _leftSnapshot  = 0;
    volatile uint8_t _rightSnapshot = 0;

    // Debounce a single channel; returns true if state changed
    bool _debounce(Channel& ch);
};
