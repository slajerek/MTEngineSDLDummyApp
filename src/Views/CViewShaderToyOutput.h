#ifndef _CViewShaderToyOutput_h_
#define _CViewShaderToyOutput_h_

#include "CGuiView.h"
#include "SYS_Defs.h"

class CViewShaderToyDemo;

// The Shader Toy example's OUTPUT window: nothing but the running shader,
// filling it edge to edge.
//
// Separate from the editor window on purpose. One window holding both meant
// the shader was always a strip under a text box, could never be resized
// without shrinking the editor, and could not go fullscreen at all -- which is
// the one thing you want once a shader is doing something.
//
// It owns no shader state. The editor view compiles and holds the shader, its
// uniforms and its error; this window asks it to draw, and shows a line of
// text when there is nothing to draw yet.
class CViewShaderToyOutput : public CGuiView
{
public:
	CViewShaderToyOutput(const char *name, float posX, float posY, float posZ,
						 float sizeX, float sizeY, CViewShaderToyDemo *editorView);
	virtual void RenderImGui() override;

private:
	// Not owned. The editor view outlives this one -- both are created and
	// destroyed together by CViewDummyAppMain.
	CViewShaderToyDemo *editorView;
};

#endif
