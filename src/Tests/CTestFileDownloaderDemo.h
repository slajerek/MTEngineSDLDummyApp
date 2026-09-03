#pragma once

#include "CTest.h"

// CTestFileDownloaderDemo
//
// Drives CViewFileDownloaderDemo::StartDemoDownload() -- the exact same
// method the "Start Download" button calls -- so this test exercises the
// real production code path: starting a local httplib server, downloading a
// bundled asset from it via CFileDownloader, and verifying the result. All
// over 127.0.0.1, no external network dependency.
class CTestFileDownloaderDemo : public CTest
{
public:
	CTestFileDownloaderDemo();
	virtual ~CTestFileDownloaderDemo();

	virtual const char *GetName() override { return "FileDownloaderDemo"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
