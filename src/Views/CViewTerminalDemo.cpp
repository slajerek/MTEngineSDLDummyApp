#include "CViewTerminalDemo.h"
#include <cstring>

CViewTerminalDemo::CViewTerminalDemo(const char *name, float posX, float posY, float posZ,
									  float sizeX, float sizeY, int cols, int rows)
: CGuiViewTerminal(name, posX, posY, posZ, sizeX, sizeY, cols, rows)
{
	// Local echo: whatever the user types comes straight back into the
	// terminal's own display, instead of going anywhere over a network.
	SetWriteCallback([this](const uint8_t *data, size_t len)
	{
		ProcessInput(data, len);
	});

	const char *banner = "MTEngineSDL Terminal Demo\r\nLocal echo -- type and press Enter\r\n\r\n";
	ProcessInput(reinterpret_cast<const uint8_t *>(banner), strlen(banner));
}
