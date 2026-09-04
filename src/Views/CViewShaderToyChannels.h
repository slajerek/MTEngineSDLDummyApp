#ifndef _CViewShaderToyChannels_h_
#define _CViewShaderToyChannels_h_

#include "CGuiView.h"
#include "SYS_Defs.h"
#include "CSystemFileDialogCallback.h"
#include "CRenderShaderCustomFragment.h"

#include <list>
#include <string>

class CSlrImage;

// Everything the shader needs to know about one channel, in one value.
//
// NOT a CSlrImage*, and the reason is channel 0: it defaults to ImGui's font
// atlas, which is an ImTextureRef and not a CSlrImage at all, so a
// CSlrImage-shaped accessor could not express the default the example ships
// with. A native handle plus its metadata covers both, and covers a render
// target's texture the day a multipass buffer channel exists.
struct SShaderChannelBinding
{
	void *texture = NULL;          // native handle, as CSlrImage::TexturePtr() returns it
	float width = 0.0f;            // the IMAGE size -- what iChannelResolution reports
	float height = 0.0f;
	// CSlrImage::defaultTexEndX/Y: the fraction of the texture the image
	// actually occupies, because CSlrImage pads every texture up to a power of
	// two. 1.0 for anything unpadded, and 1.0 for everything once NextPow2 is
	// removed -- at which point this half of iChannelUvTransform quietly
	// becomes a no-op rather than needing to be taken back out.
	float uvScaleX = 1.0f;
	float uvScaleY = 1.0f;
	// ON BY DEFAULT, exactly as shadertoy.com defaults vflip for a texture.
	// The engine stores images top-down (which is why the thumbnail beside
	// this is upright) while ShaderToy's fragCoord counts from the BOTTOM, so
	// an unflipped channel samples the picture upside down.
	bool flipY = true;
	EShaderChannelFilter filter = SHADER_CHANNEL_LINEAR;
	EShaderChannelWrap wrap = SHADER_CHANNEL_REPEAT;
};

// The Shader Toy example's channel panel: four texture slots, exactly as
// shadertoy.com has, minus the sources this engine has no equivalent for.
//
// WHAT IS HERE: a still image per slot, from any file the platform's image
// loader reads, with a filter and a wrap mode each. WHAT IS DELIBERATELY NOT:
// buffers (they need multipass render targets), cubemaps, video, webcam, audio
// and the virtual keyboard texture. Each of those is a feature in its own
// right; none of them changes the uniform block or the binding rules this
// panel establishes.
//
// IT OWNS THE IMAGES. The editor view reads bindings and never holds one --
// the two views have different lifetimes only in principle, but an owner that
// is also a reader is how a double free starts.
class CViewShaderToyChannels : public CGuiView, public CSystemFileDialogCallback
{
public:
	CViewShaderToyChannels(const char *name, float posX, float posY, float posZ,
						   float sizeX, float sizeY);
	virtual ~CViewShaderToyChannels();
	virtual void RenderImGui() override;

	// Never fails: an empty slot returns a default-constructed binding, whose
	// NULL texture every backend renders as black.
	SShaderChannelBinding GetChannelBinding(int channel);

	// RENDER THREAD ONLY -- it creates a GPU texture. False when the file could
	// not be read, leaving the slot exactly as it was. A test calls this
	// directly; the UI goes through the file dialog, which defers to
	// ServicePendingLoad() for this reason.
	bool SetChannelImage(int channel, const char *path);
	void ClearChannel(int channel);

	// Bind ImGui's font atlas to a slot -- what channel 0 holds by default,
	// and the one texture guaranteed to exist on every backend with nothing
	// to load.
	void SetChannelFontAtlas(int channel);

	// "" for an empty slot, kFontAtlasPath for the default channel 0.
	const char *GetChannelPath(int channel);
	void SetChannelFilter(int channel, EShaderChannelFilter filter);
	void SetChannelWrap(int channel, EShaderChannelWrap wrap);
	void SetChannelFlipY(int channel, bool flipY);
	bool GetChannelFlipY(int channel);

	virtual void SystemDialogFileOpenSelected(CSlrString *path) override;

	// The sentinel path meaning "ImGui's font atlas", which is what channel 0
	// holds until the user puts something else there. A real path, a sentinel
	// and empty are the three states a slot can be in, and they persist as
	// written.
	static const char *kFontAtlasPath;

private:
	// Creates the texture for whatever the dialog picked. Runs at the top of
	// RenderImGui, on the render thread; the dialog callback only records the
	// path, because it may arrive on any thread and binding a texture off the
	// render thread does not fail, it crashes.
	void ServicePendingLoad();

	// Opens the platform file dialog for one slot, remembering which asked --
	// one callback serves four thumbnails.
	void OpenLoadDialogFor(int channel);

	void LoadPersisted();
	void SavePersisted();

	// The CSlrImages this view loaded, and therefore owns. Channel 0's default
	// is the font atlas, which is owned by ImGui and never appears here.
	CSlrImage *ownedImage[kShaderChannelCount] = {};
	std::string channelPath[kShaderChannelCount];
	EShaderChannelFilter channelFilter[kShaderChannelCount] =
		{ SHADER_CHANNEL_LINEAR, SHADER_CHANNEL_LINEAR, SHADER_CHANNEL_LINEAR, SHADER_CHANNEL_LINEAR };
	EShaderChannelWrap channelWrap[kShaderChannelCount] =
		{ SHADER_CHANNEL_REPEAT, SHADER_CHANNEL_REPEAT, SHADER_CHANNEL_REPEAT, SHADER_CHANNEL_REPEAT };
	bool channelFlipY[kShaderChannelCount] = { true, true, true, true };

	// WHICH SLOT ASKED. One callback serves four Load… buttons, so the slot has
	// to be remembered across the dialog; -1 means no dialog is outstanding.
	int dialogSlot = -1;
	// Handed over by SystemDialogFileOpenSelected, consumed by
	// ServicePendingLoad on the next frame. Empty means nothing pending.
	std::string pendingPath;
	int pendingSlot = -1;
	// Set by LoadPersisted, acted on by the first ServicePendingLoad: the
	// constructor knows the paths but is in no position to make textures.
	bool initialLoadPending = false;

	std::list<CSlrString *> imageFileExtensions;
};

#endif
