#pragma once

// Centralized FastLED output path:
// - Enforces a minimum interval between hardware show() calls
// - Provides a clear-and-show helper that still routes through the limiter

void ledOutputShow(bool force = false);
void ledOutputClear(bool forceShow = true, bool force = false);
