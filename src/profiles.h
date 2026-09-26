#pragma once
#include "settings.h"
static constexpr int PROFILE_COUNT = 6;
static constexpr int PROFILE_NAME_SIZE = 25;
bool profile_read(int slot, char* name, Settings& settings);
bool profile_write(int slot, const char* name, const Settings& settings);
bool profile_delete(int slot);
