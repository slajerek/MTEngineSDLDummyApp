#pragma once

#include "CTest.h"

// CTestShaderToyDemo
//
// Drives the Shader Toy example's compile cycle on WHATEVER BACKEND IS
// RUNNING: the shipped preset compiles, a deliberately broken source is
// reported as broken with the driver's own words, and valid source recovers.
//
// SYNCHRONOUS, and that is not an implementation detail. CTest::Run() executes
// inside MT_Render() -- on the render thread, inside the frame -- so a test
// that posted a compile request and polled for it would be waiting on the very
// thread that services it. It calls CViewShaderToyDemo::CompileNow() instead,
// which asserts VID_IsRenderThread(). The imgui_test_engine suite runs on its
// own thread and must do the opposite; see the view's header.
//
// THE FAILURE ASSERTIONS ARE THE POINT. One requires the compiler's
// diagnostics to be RETURNED rather than logged, so a log-only implementation
// fails on Linux, where GLOBAL_DEBUG_OFF turns LOGError into nothing. The
// other requires the previous working shader to survive a failed rebuild, so a
// typo never blanks the preview.
class CTestShaderToyDemo : public CTest
{
public:
	CTestShaderToyDemo();
	virtual ~CTestShaderToyDemo();

	virtual const char *GetName() override { return "ShaderToyDemo"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
