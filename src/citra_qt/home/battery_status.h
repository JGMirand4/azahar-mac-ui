// Copyright 2016-2025 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

namespace Home {

/// Consulta a bateria interna. Retorna false se nao houver bateria ou se a plataforma
/// nao oferecer essa informacao (por exemplo, um Mac de mesa).
bool QueryBattery(int& percent, bool& charging);

} // namespace Home
