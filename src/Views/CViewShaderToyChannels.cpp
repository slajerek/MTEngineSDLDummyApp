#include "CViewShaderToyChannels.h"

#include "CSlrImage.h"
#include "CSlrString.h"
#include "CGuiMain.h"
#include "DBG_Log.h"
#include "SYS_FileSystem.h"
#include "SYS_DefaultConfig.h"
#include "CConfigStorageHjson.h"
#include "Core/Render/VID_ImageBinding.h"

#include <cstdio>
#include <cstring>

using namespace ImGui;

const char *CViewShaderToyChannels::kFontAtlasPath = "@font-atlas";

CViewShaderToyChannels::CViewShaderToyChannels(const char *name, float posX, float posY, float posZ,
											   float sizeX, float sizeY)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	// EVERY FORMAT THE ENGINE DECODES, taken from CImageData::Load's own
	// extension dispatch (CImageData.cpp:3323 onward) rather than guessed:
	// the dedicated decoders first, then the stb_image chain that everything
	// else falls through to. None of them is capability-gated -- they are all
	// compiled in -- so this list is static.
	static const char *kImageExtensions[] =
	{
		// dedicated decoders
		"ktx2", "tiff", "tif", "webp", "heic", "heif", "avif", "png",
		// raw camera files, via the embedded preview
		"cr2", "cr3", "nef", "arw", "dng", "raf", "rw2", "orf", "pef",
		// stb_image
		"jpg", "jpeg", "bmp", "tga", "psd", "gif", "hdr", "pic", "pnm", "ppm", "pgm",
		NULL
	};
	for (int i = 0; kImageExtensions[i] != NULL; i++)
		imageFileExtensions.push_back(new CSlrString(kImageExtensions[i]));

	LoadPersisted();
}

CViewShaderToyChannels::~CViewShaderToyChannels()
{
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		if (ownedImage[i] != NULL)
		{
			VID_PostImageDealloc(ownedImage[i]);
			ownedImage[i] = NULL;
		}
	}
	while (!imageFileExtensions.empty())
	{
		delete imageFileExtensions.front();
		imageFileExtensions.pop_front();
	}
}

// --- bindings ---------------------------------------------------------------

SShaderChannelBinding CViewShaderToyChannels::GetChannelBinding(int channel)
{
	SShaderChannelBinding binding;
	if (channel < 0 || channel >= kShaderChannelCount)
		return binding;

	binding.filter = channelFilter[channel];
	binding.wrap = channelWrap[channel];
	binding.flipY = channelFlipY[channel];

	if (ownedImage[channel] != NULL)
	{
		binding.texture = ownedImage[channel]->TexturePtr();
		// width/height, NOT rasterWidth/rasterHeight: iChannelResolution is
		// the IMAGE's size, and the raster pair is the padded texture it sits
		// in. Handing the padded size to a shader makes every uv it computes
		// from iChannelResolution wrong by the padding fraction.
		binding.width = ownedImage[channel]->width;
		binding.height = ownedImage[channel]->height;
		binding.uvScaleX = ownedImage[channel]->defaultTexEndX;
		binding.uvScaleY = ownedImage[channel]->defaultTexEndY;
		return binding;
	}

	if (channelPath[channel] == kFontAtlasPath)
	{
		ImFontAtlas *atlas = GetIO().Fonts;
		if (atlas != NULL && atlas->TexData != NULL)
		{
			binding.texture = (void *)(intptr_t)atlas->TexRef.GetTexID();
			binding.width = (float)atlas->TexData->Width;
			binding.height = (float)atlas->TexData->Height;
		}
		return binding;
	}

	return binding;
}

const char *CViewShaderToyChannels::GetChannelPath(int channel)
{
	if (channel < 0 || channel >= kShaderChannelCount)
		return "";
	return channelPath[channel].c_str();
}

void CViewShaderToyChannels::SetChannelFilter(int channel, EShaderChannelFilter filter)
{
	if (channel < 0 || channel >= kShaderChannelCount)
		return;
	channelFilter[channel] = filter;
	SavePersisted();
}

void CViewShaderToyChannels::SetChannelWrap(int channel, EShaderChannelWrap wrap)
{
	if (channel < 0 || channel >= kShaderChannelCount)
		return;
	channelWrap[channel] = wrap;
	SavePersisted();
}

void CViewShaderToyChannels::SetChannelFlipY(int channel, bool flipY)
{
	if (channel < 0 || channel >= kShaderChannelCount)
		return;
	channelFlipY[channel] = flipY;
	SavePersisted();
}

bool CViewShaderToyChannels::GetChannelFlipY(int channel)
{
	if (channel < 0 || channel >= kShaderChannelCount)
		return false;
	return channelFlipY[channel];
}

void CViewShaderToyChannels::ClearChannel(int channel)
{
	if (channel < 0 || channel >= kShaderChannelCount)
		return;

	if (ownedImage[channel] != NULL)
	{
		VID_PostImageDealloc(ownedImage[channel]);
		ownedImage[channel] = NULL;
	}
	// Empty, not back to the font atlas -- Clear on channel 0 has to be able
	// to mean "nothing", or the default could never be got rid of.
	channelPath[channel].clear();
	SavePersisted();
}

void CViewShaderToyChannels::SetChannelFontAtlas(int channel)
{
	if (channel < 0 || channel >= kShaderChannelCount)
		return;

	if (ownedImage[channel] != NULL)
	{
		VID_PostImageDealloc(ownedImage[channel]);
		ownedImage[channel] = NULL;
	}
	channelPath[channel] = kFontAtlasPath;
	SavePersisted();
}

bool CViewShaderToyChannels::SetChannelImage(int channel, const char *path)
{
	if (channel < 0 || channel >= kShaderChannelCount || path == NULL || path[0] == '\0')
		return false;

	// linearScaling true, fromResources false: an absolute path, filtered the
	// way a photograph wants. The per-channel filter combo overrides this at
	// sample time through our own sampler objects, so this only decides what
	// the rest of the engine sees.
	CSlrImage *image = new CSlrImage(path, true, false);
	// A ZERO-SIZED IMAGE IS A FAILURE, and it is the failure a user will
	// actually hit: CImageData::Load LOGErrors and returns false on a file it
	// cannot decode, but nothing above it treats that as fatal, so a corrupt
	// or unsupported file arrives here as a 0x0 texture rather than as an
	// error state.
	if (image->resourceState == RESOURCE_STATE_ERROR || image->TexturePtr() == NULL
		|| image->width < 1.0f || image->height < 1.0f)
	{
		LOGError("CViewShaderToyChannels: failed to load %s", path);
		delete image;
		return false;
	}

	if (ownedImage[channel] != NULL)
		VID_PostImageDealloc(ownedImage[channel]);
	ownedImage[channel] = image;
	channelPath[channel] = path;
	SavePersisted();
	return true;
}

// --- the file dialog --------------------------------------------------------

void CViewShaderToyChannels::SystemDialogFileOpenSelected(CSlrString *path)
{
	if (path == NULL || dialogSlot < 0)
	{
		dialogSlot = -1;
		return;
	}

	char *cPath = path->GetStdASCII();
	// RECORD ONLY. This runs from the platform's dialog, which is not
	// guaranteed to be the render thread, and CSlrImage's constructor binds a
	// GPU texture on the spot.
	pendingPath = cPath;
	pendingSlot = dialogSlot;
	delete [] cPath;

	dialogSlot = -1;
}

void CViewShaderToyChannels::ServicePendingLoad()
{
	if (initialLoadPending)
	{
		initialLoadPending = false;
		int failed = 0;
		for (int i = 0; i < kShaderChannelCount; i++)
		{
			if (channelPath[i].empty() || channelPath[i] == kFontAtlasPath)
				continue;
			// A remembered file that has since been moved or deleted CLEARS
			// the slot. Leaving the path in place would fail again on every
			// launch, silently, and leave the panel claiming an image it does
			// not have.
			if (!SetChannelImage(i, channelPath[i].c_str()))
			{
				channelPath[i].clear();
				failed++;
			}
		}
		if (failed > 0)
		{
			SavePersisted();
			guiMain->ShowNotification("Shader Toy",
									  "A remembered channel image is gone -- that slot is empty");
		}
	}

	if (pendingSlot < 0 || pendingPath.empty())
		return;

	int slot = pendingSlot;
	std::string path = pendingPath;
	pendingSlot = -1;
	pendingPath.clear();

	if (!SetChannelImage(slot, path.c_str()))
		guiMain->ShowNotification("Shader Toy", "Could not load that image");
}

// --- persistence ------------------------------------------------------------

void CViewShaderToyChannels::LoadPersisted()
{
	// Channel 0 defaults to the font atlas so the Texture preset shows
	// something on a first run; the other three start empty, which samples
	// black exactly as an unset ShaderToy channel does.
	channelPath[0] = kFontAtlasPath;

	if (gApplicationDefaultConfig == NULL)
		return;

	char key[64];
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		// E_x_i_s_t_s FIRST, because GetStdString DOES NOT APPLY ITS DEFAULT:
		// on a missing key it clears the string and returns (GetInt, two calls
		// below, does honour its default). Passing channelPath[i] as the
		// default and trusting it is how channel 0's font-atlas default was
		// wiped on every first run.
		snprintf(key, sizeof(key), "shadertoy.channel%d.path", i);
		std::string path = channelPath[i];
		if (gApplicationDefaultConfig->E_x_i_s_t_s(key))
			gApplicationDefaultConfig->GetStdString(key, &path, channelPath[i]);

		int filter = (int)channelFilter[i];
		snprintf(key, sizeof(key), "shadertoy.channel%d.filter", i);
		gApplicationDefaultConfig->GetInt(key, &filter, filter);
		channelFilter[i] = (filter == (int)SHADER_CHANNEL_NEAREST) ? SHADER_CHANNEL_NEAREST
																   : SHADER_CHANNEL_LINEAR;

		int wrap = (int)channelWrap[i];
		snprintf(key, sizeof(key), "shadertoy.channel%d.wrap", i);
		gApplicationDefaultConfig->GetInt(key, &wrap, wrap);
		channelWrap[i] = (wrap == (int)SHADER_CHANNEL_CLAMP) ? SHADER_CHANNEL_CLAMP
															 : SHADER_CHANNEL_REPEAT;

		bool flipY = channelFlipY[i];
		snprintf(key, sizeof(key), "shadertoy.channel%d.flipy", i);
		gApplicationDefaultConfig->GetBool(key, &flipY, flipY);
		channelFlipY[i] = flipY;

		channelPath[i] = path;
	}

	// The IMAGES load on the first frame, not here: this runs from the
	// constructor, and creating a texture is a render-thread operation.
	initialLoadPending = true;
}

void CViewShaderToyChannels::SavePersisted()
{
	if (gApplicationDefaultConfig == NULL)
		return;

	char key[64];
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		snprintf(key, sizeof(key), "shadertoy.channel%d.path", i);
		gApplicationDefaultConfig->SetStdString(key, channelPath[i]);

		int filter = (int)channelFilter[i];
		snprintf(key, sizeof(key), "shadertoy.channel%d.filter", i);
		gApplicationDefaultConfig->SetInt(key, &filter);

		int wrap = (int)channelWrap[i];
		snprintf(key, sizeof(key), "shadertoy.channel%d.wrap", i);
		gApplicationDefaultConfig->SetInt(key, &wrap);

		bool flipY = channelFlipY[i];
		snprintf(key, sizeof(key), "shadertoy.channel%d.flipy", i);
		gApplicationDefaultConfig->SetBool(key, &flipY);
	}
}

// --- the panel --------------------------------------------------------------

void CViewShaderToyChannels::OpenLoadDialogFor(int channel)
{
	dialogSlot = channel;
	CSlrString *title = new CSlrString("Load channel image");
	SYS_DialogOpenFile(this, &imageFileExtensions, NULL, title);
}

void CViewShaderToyChannels::RenderImGui()
{
	PreRenderImGui();

	ServicePendingLoad();

	// FOUR COLUMNS, one per slot, sized to the window rather than fixed: this
	// window's whole job is to sit under the editor and show what the shader
	// is sampling, so the thumbnails are the content and everything else is a
	// caption.
	float spacing = GetStyle().ItemSpacing.x;
	float columnWidth = (GetContentRegionAvail().x - spacing * (float)(kShaderChannelCount - 1))
					  / (float)kShaderChannelCount;
	if (columnWidth < 60.0f)
		columnWidth = 60.0f;
	float thumbSize = columnWidth - GetStyle().FramePadding.x * 2.0f;
	if (thumbSize > 128.0f)
		thumbSize = 128.0f;

	for (int i = 0; i < kShaderChannelCount; i++)
	{
		PushID(i);
		BeginGroup();

		SShaderChannelBinding binding = GetChannelBinding(i);

		// THE THUMBNAIL IS THE BUTTON. Clicking the picture to replace it is
		// what shadertoy.com does and what anyone tries first; a separate
		// "Load..." button beside it was one control too many for a window
		// that is meant to be glanced at.
		bool clicked = false;
		if (binding.texture != NULL)
		{
			// uv1 is the USED FRACTION, not (1,1): CSlrImage pads to a power
			// of two, and a thumbnail drawn to (1,1) shows the padding as a
			// black band down two edges.
			clicked = ImageButton("##thumb", (ImTextureID)(intptr_t)binding.texture,
								  ImVec2(thumbSize, thumbSize),
								  ImVec2(0.0f, 0.0f), ImVec2(binding.uvScaleX, binding.uvScaleY));
		}
		else
		{
			clicked = Button("Click to\nload", ImVec2(thumbSize, thumbSize));
		}

		if (clicked)
			OpenLoadDialogFor(i);

		if (IsItemHovered())
		{
			SetTooltip("iChannel%d -- click to load an image\n"
					   "png jpg ktx2 heic tiff webp avif raw ...\n"
					   "right-click for more", i);
		}

		// Right-click for the two things that are not "load a file".
		if (BeginPopupContextItem("##channelMenu"))
		{
			if (MenuItem("Load image..."))
				OpenLoadDialogFor(i);
			if (MenuItem("ImGui font atlas"))
				SetChannelFontAtlas(i);
			if (MenuItem("Clear"))
				ClearChannel(i);
			EndPopup();
		}

		Text("iChannel%d", i);

		// The caption is CLIPPED to the column, with the full path in the
		// tooltip: a file name is routinely longer than a quarter of this
		// window, and letting it push the columns apart would break the row.
		char caption[512];
		if (channelPath[i].empty())
			snprintf(caption, sizeof(caption), "(empty -- black)");
		else if (channelPath[i] == kFontAtlasPath)
			snprintf(caption, sizeof(caption), "font atlas %.0fx%.0f", binding.width, binding.height);
		else
		{
			const char *slash = strrchr(channelPath[i].c_str(), '/');
			const char *backslash = strrchr(channelPath[i].c_str(), '\\');
			if (backslash != NULL && (slash == NULL || backslash > slash))
				slash = backslash;
			const char *fileName = (slash != NULL) ? slash + 1 : channelPath[i].c_str();
			snprintf(caption, sizeof(caption), "%s %.0fx%.0f", fileName, binding.width, binding.height);
		}
		PushTextWrapPos(GetCursorPosX() + columnWidth);
		TextDisabled("%s", caption);
		PopTextWrapPos();
		if (IsItemHovered() && !channelPath[i].empty() && channelPath[i] != kFontAtlasPath)
			SetTooltip("%s", channelPath[i].c_str());

		SetNextItemWidth(columnWidth);
		int filter = (channelFilter[i] == SHADER_CHANNEL_NEAREST) ? 1 : 0;
		if (Combo("##filter", &filter, "Linear\0Nearest\0"))
			SetChannelFilter(i, filter == 1 ? SHADER_CHANNEL_NEAREST : SHADER_CHANNEL_LINEAR);

		SetNextItemWidth(columnWidth);
		int wrap = (channelWrap[i] == SHADER_CHANNEL_CLAMP) ? 1 : 0;
		if (Combo("##wrap", &wrap, "Repeat\0Clamp\0"))
			SetChannelWrap(i, wrap == 1 ? SHADER_CHANNEL_CLAMP : SHADER_CHANNEL_REPEAT);

		// ON BY DEFAULT, and shown rather than hidden because this is the one
		// setting whose wrong value looks like a bug in the engine rather than
		// a setting: ShaderToy's fragCoord counts from the bottom, textures
		// are stored top-down, and shadertoy.com defaults the same toggle on.
		bool flipY = channelFlipY[i];
		if (Checkbox("Flip Y", &flipY))
			SetChannelFlipY(i, flipY);

		EndGroup();

		if (i != kShaderChannelCount - 1)
			SameLine();

		PopID();
	}

	TextDisabled("Sample them with texChannel0(uv) .. texChannel3(uv). "
				 "Wrapping is done in the shader, not by the sampler: the engine pads every "
				 "texture up to a power of two, so hardware repeat would tile the padding.");

	PostRenderImGui();
}
