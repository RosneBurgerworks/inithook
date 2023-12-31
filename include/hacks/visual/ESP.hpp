#pragma once

#include "config.h"
#if ENABLE_VISUALS
#include "colors.hpp"

namespace hacks::shared::esp
{

// Init
void Init();

// Strings
void SetEntityColor(CachedEntity *entity, const rgba_t &color);
} // namespace hacks::shared::esp
#endif
