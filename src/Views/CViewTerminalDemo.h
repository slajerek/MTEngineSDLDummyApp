#ifndef _CViewTerminalDemo_h_
#define _CViewTerminalDemo_h_

#include "CGuiViewTerminal.h"

// ===========================================================================
// Terminal example
// ===========================================================================
//
// A local-echo demo of MTEngineSDL's CGuiViewTerminal (GUI/Controls/,
// libtmt-backed VT100 emulator). c64d wires the same base class to a real
// telnet transport (CViewC64UTerminal: SetWriteCallback -> send over the
// network, incoming network data -> ProcessInput). This demo has no
// network/subprocess/PTY -- SetWriteCallback loops straight back into
// ProcessInput, so whatever you type is echoed into the terminal itself.
// ===========================================================================

class CViewTerminalDemo : public CGuiViewTerminal
{
public:
	CViewTerminalDemo(const char *name, float posX, float posY, float posZ,
					   float sizeX, float sizeY, int cols = 80, int rows = 25);
};

#endif
