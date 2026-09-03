#include "CViewFileDownloaderDemo.h"
#include "RES_ResourceManager.h"
#include "SYS_Main.h"
#include "SYS_Funct.h"
#include "httplib.h"
#include <filesystem>

using namespace ImGui;

CViewFileDownloaderDemo::CViewFileDownloaderDemo(const char *name, float posX, float posY, float posZ,
												  float sizeX, float sizeY)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;
}

CViewFileDownloaderDemo::~CViewFileDownloaderDemo()
{
	downloader.Shutdown();

	if (demoServer != NULL)
	{
		demoServer->stop();
		if (demoServerThread.joinable())
			demoServerThread.join();
		delete demoServer;
		demoServer = NULL;
	}
}

void CViewFileDownloaderDemo::EnsureServerStarted()
{
	if (serverStarted)
		return;

	// Serve the EXISTING assets/fonts/ directory as-is -- set_mount_point
	// handles Range requests for static files on its own, which is what
	// CFileDownloader's resume support needs.
	std::string fontDir = RES_ResolveResourceDir("assets/fonts/", "Roboto-Medium.ttf");

	demoServer = new httplib::Server();
	demoServer->set_mount_point("/", fontDir);

	// A fixed base port, offset the same way every other httplib test built
	// on this engine is. A resume test elsewhere reserves 14904, and this is
	// deliberately distinct from it so the two cannot clash if both ever ran
	// on the same machine.
	demoPort = SYS_ApplyPortOffset(14950);

	httplib::Server *server = demoServer;
	int port = demoPort;
	demoServerThread = std::thread([server, port]()
	{
		server->listen("127.0.0.1", port);
	});

	// Give the listener a moment to bind before the first download attempt,
	// the same margin the reference resume test uses.
	SYS_Sleep(300);

	serverStarted = true;
}

void CViewFileDownloaderDemo::StartDemoDownload()
{
	if (downloader.IsDownloading())
		return;

	EnsureServerStarted();

	demoDstPath = (std::filesystem::temp_directory_path() / "MTEngineSDLDummyApp_download_demo.ttf").string();

	std::string url = "http://127.0.0.1:" + std::to_string(demoPort) + "/Roboto-Medium.ttf";
	downloader.StartDownload(url, demoDstPath);
}

void CViewFileDownloaderDemo::RenderImGui()
{
	PreRenderImGui();

	downloader.Poll();

	TextWrapped("Downloads a copy of this app's own Roboto-Medium.ttf from a "
				"tiny local HTTP server (127.0.0.1) started on demand -- no "
				"real network dependency.");
	Separator();

	BeginDisabled(downloader.IsDownloading());
	if (Button("Start Download"))
	{
		StartDemoDownload();
	}
	EndDisabled();

	if (downloader.IsDownloading() || downloader.GetTotalBytes() > 0)
	{
		uint64_t downloaded = downloader.GetDownloadedBytes();
		uint64_t total = downloader.GetTotalBytes();
		float fraction = total > 0 ? (float)downloaded / (float)total : 0.0f;

		char overlay[64];
		snprintf(overlay, sizeof(overlay), "%llu / %llu bytes",
				 (unsigned long long)downloaded, (unsigned long long)total);
		ProgressBar(fraction, ImVec2(-1.0f, 0.0f), overlay);
	}

	if (!downloader.IsDownloading() && !demoDstPath.empty())
	{
		if (downloader.IsSuccess())
		{
			TextWrapped("Downloaded to: %s", demoDstPath.c_str());
		}
		else if (!downloader.GetLastError().empty())
		{
			TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", downloader.GetLastError().c_str());
		}
	}

	PostRenderImGui();
}
