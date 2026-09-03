#ifndef _CViewFileDownloaderDemo_h_
#define _CViewFileDownloaderDemo_h_

#include "CGuiView.h"
#include "CFileDownloader.h"
#include <string>
#include <thread>

namespace httplib { class Server; }

// ===========================================================================
// File Downloader example
// ===========================================================================
//
// Demonstrates the engine's CFileDownloader (Tools/NetGame/, httplib-backed,
// resumable via Range headers) against a tiny local HTTP server -- entirely
// offline, no real network dependency. The server serves an EXISTING small
// repo asset (assets/fonts/Roboto-Medium.ttf) rather than a new one, so
// there is nothing new to add to the asset pipeline on any platform.
//
// The constructor does nothing beyond the base CGuiView -- no port binding
// just because the window exists -- so opening/showing this view (which the
// automated UI test does) stays side-effect free. The server and download
// only start when StartDemoDownload() is called, either from the "Start
// Download" button or directly from a headless test.
// ===========================================================================

class CViewFileDownloaderDemo : public CGuiView
{
public:
	CViewFileDownloaderDemo(const char *name, float posX, float posY, float posZ,
							 float sizeX, float sizeY);
	virtual ~CViewFileDownloaderDemo();

	virtual void RenderImGui() override;

	// Public: the UI button and the headless test both call this, so the
	// test exercises the exact code path the button does.
	void StartDemoDownload();

	CFileDownloader downloader;
	std::string demoDstPath;

private:
	void EnsureServerStarted();

	httplib::Server *demoServer = NULL;
	std::thread demoServerThread;
	bool serverStarted = false;
	int demoPort = 0;
};

#endif
