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

    // Raw electrical levels in physical OPTO1..OPTO6 order, before polarity
    // interpretation or debounce. Bit 0 is OPTO1; HIGH sets the bit.
    uint8_t rawPcbLevels() const;

    // ── Atomic packed snapshots (safe to read from a different RTOS task) ───
    // Bit layout per byte:  bit0=brake  bit1=running  bit2=turn  bit3=reverse
    // Written as a single atomic byte store at the end of every update().
    uint8_t driverSnapshot()  const { return _driverSnapshot;  }
    uint8_t passengerSnapshot() const { return _passengerSnapshot; }

    // ── Left-side debounced accessors ────────────────────────────────────────
    bool driverBrake()   const { return _driverBrake;   }
    bool driverRunning() const { return _driverRunning; }
    bool driverTurn()    const { return _driverTurn;    }
    bool driverReverse() const { return _driverReverse; }

    // ── Right-side debounced accessors ───────────────────────────────────────
    bool passengerBrake()   const { return _passengerBrake;   }
    bool passengerRunning() const { return _passengerRunning; }
    bool passengerTurn()    const { return _passengerTurn;    }
    bool passengerReverse() const { return _passengerReverse; }

private:
    // One debounce tracker per channel
    struct Channel {
        int           pin;
        unsigned long debounceMs   = DEBOUNCE_MS;  // per-channel; set in begin()
        bool          state        = false;
        bool          lastRaw      = false;
        unsigned long lastChangeMs = 0;
    };

    // Channels 0-3: driver side    (brake, running, turn, reverse)  — US left
    // Channels 4-7: passenger side (brake, running, turn, reverse)  — US right
    Channel _channels[8];

    // All output flags are volatile so the compiler never optimises away reads
    // on Core 1 when Core 0 (the input task) is the writer.
    volatile bool _driverBrake   = false;
    volatile bool _driverRunning = false;
    volatile bool _driverTurn    = false;
    volatile bool _driverReverse = false;

    volatile bool _passengerBrake   = false;
    volatile bool _passengerRunning = false;
    volatile bool _passengerTurn    = false;
    volatile bool _passengerReverse = false;

    // Packed atomic snapshots — single byte written at the end of update().
    // On ESP32 (Xtensa LX7), aligned byte stores are atomic, so Core 1 always
    // reads a coherent frame without a mutex.
    // Bit layout: bit0=brake  bit1=running  bit2=turn  bit3=reverse
    volatile uint8_t _driverSnapshot  = 0;
    volatile uint8_t _passengerSnapshot = 0;

    // Debounce a single channel; returns true if state changed
    bool _debounce(Channel& ch);
};
