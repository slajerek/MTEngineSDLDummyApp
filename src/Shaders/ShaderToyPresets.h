#ifndef _ShaderToyPresets_h_
#define _ShaderToyPresets_h_

// The Shader Toy example's starting shaders, each written out in all three
// languages the engine's backends speak.
//
// WHY THREE COPIES RATHER THAN ONE AND A TRANSLATOR. A GLSL-to-MSL/HLSL
// transpiler means glslang plus SPIRV-Cross -- two substantial new engine
// dependencies -- to buy "paste from shadertoy.com and it runs everywhere".
// Writing the same effect three times instead costs a few dozen lines and
// turns the difference into the example's subject: the bodies are the same
// arithmetic, and what varies is exactly what a reader needs to know.
//
// WHAT ACTUALLY DIFFERS between the three, and it is a short list:
//   vec2/vec3/vec4  ->  float2/float3/float4
//   mod             ->  fmod
//   atan(y, x)      ->  atan2(y, x)
//   fract           ->  frac            (HLSL only)
//   mix             ->  lerp            (unused here, but the pattern holds)
//   Texture sampling is texChannel0(uv) in all three, because the spelling
//   differs everywhere (texture / .Sample / .sample) and hiding that one
//   difference behind a macro is what lets the BODIES stay identical.
//
// EVERY BODY OPENS WITH MAIN_IMAGE, and that macro earns its keep on Metal.
// GLSL uniforms and HLSL cbuffer members are global scope, and so are HLSL's
// Texture2D/SamplerState; MSL has neither -- a constant buffer and a texture
// both arrive as function arguments and cannot be made global. With four
// channels, MSL's mainImage takes ELEVEN parameters while the other two take
// two. MAIN_IMAGE is defined by each backend's preamble to whatever that
// backend's signature is, so a preset writes one word and never sees the list,
// and the three variants below now differ ONLY in the arithmetic spellings
// above. The long-form signature still compiles on every backend -- the macro
// is a convenience, not a requirement.
//
// The host writes ONLY mainImage. Everything else -- uniform declarations,
// the entry point, the vertex stage on Metal -- is supplied by
// CRenderShaderCustomFragment's per-backend preamble.

// ---------------------------------------------------------------------------
// Tunnel -- the flagship. A kaleidoscopically folded tunnel with a cosine
// palette cycling per depth step: three overlaid copies at different phase,
// each folded into a 60-degree wedge, lit by a sharp power curve so the bands
// read as glowing rather than as a gradient.
//
// smoothstep(0.2, 2.5, depth) then INVERTED, never smoothstep(2.5, 0.2, ...):
// GLSL leaves edge0 >= edge1 undefined, and it would differ between drivers.
// ---------------------------------------------------------------------------

static const char *kShaderToyTunnelGLSL = R"GLSL(MAIN_IMAGE
{
	vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	vec3 col = vec3(0.0);
	float t = iTime * 0.5;

	for (float i = 0.0; i < 3.0; i += 1.0)
	{
		float r = length(uv);
		float a = atan(uv.y, uv.x) + t * 0.3 + i * 2.094;
		a = mod(a, 1.0472) - 0.5236;          // fold into a 60-degree wedge
		vec2 p = vec2(cos(a), sin(a)) * r;

		float depth = 1.0 / (r + 0.15);
		vec2 tun = vec2(p.x * depth * 0.5, depth + t * 1.5);

		float bands = sin(tun.y * 3.0 + sin(tun.x * 4.0 + t)) * 0.5 + 0.5;
		float glow = pow(bands, 6.0) * (1.0 - smoothstep(0.2, 2.5, depth));

		col += (0.5 + 0.5 * cos(6.2831 * (vec3(0.0, 0.33, 0.67)
										  + depth * 0.15 + t * 0.2 + i * 0.15)))
			   * glow * 0.9;
	}

	fragColor = vec4(pow(col, vec3(0.85)), 1.0);
}
)GLSL";

static const char *kShaderToyTunnelMSL = R"MSL(MAIN_IMAGE
{
	float2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float3 col = float3(0.0);
	float t = iTime * 0.5;

	for (float i = 0.0; i < 3.0; i += 1.0)
	{
		float r = length(uv);
		float a = atan2(uv.y, uv.x) + t * 0.3 + i * 2.094;
		a = fmod(a, 1.0472) - 0.5236;         // fold into a 60-degree wedge
		float2 p = float2(cos(a), sin(a)) * r;

		float depth = 1.0 / (r + 0.15);
		float2 tun = float2(p.x * depth * 0.5, depth + t * 1.5);

		float bands = sin(tun.y * 3.0 + sin(tun.x * 4.0 + t)) * 0.5 + 0.5;
		float glow = pow(bands, 6.0) * (1.0 - smoothstep(0.2, 2.5, depth));

		col += (0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.33, 0.67)
										  + depth * 0.15 + t * 0.2 + i * 0.15)))
			   * glow * 0.9;
	}

	fragColor = float4(pow(col, float3(0.85)), 1.0);
}
)MSL";

static const char *kShaderToyTunnelHLSL = R"HLSL(MAIN_IMAGE
{
	float2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float3 col = float3(0.0, 0.0, 0.0);
	float t = iTime * 0.5;

	for (float i = 0.0; i < 3.0; i += 1.0)
	{
		float r = length(uv);
		float a = atan2(uv.y, uv.x) + t * 0.3 + i * 2.094;
		a = fmod(a, 1.0472) - 0.5236;         // fold into a 60-degree wedge
		float2 p = float2(cos(a), sin(a)) * r;

		float depth = 1.0 / (r + 0.15);
		float2 tun = float2(p.x * depth * 0.5, depth + t * 1.5);

		float bands = sin(tun.y * 3.0 + sin(tun.x * 4.0 + t)) * 0.5 + 0.5;
		float glow = pow(bands, 6.0) * (1.0 - smoothstep(0.2, 2.5, depth));

		col += (0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.33, 0.67)
										  + depth * 0.15 + t * 0.2 + i * 0.15)))
			   * glow * 0.9;
	}

	fragColor = float4(pow(col, float3(0.85, 0.85, 0.85)), 1.0);
}
)HLSL";

// ---------------------------------------------------------------------------
// Hello UV -- ten lines, and its whole purpose is that the three versions fit
// on one screen together, so the language differences are legible at a glance
// before anyone reads the tunnel.
// ---------------------------------------------------------------------------

static const char *kShaderToyHelloGLSL = R"GLSL(MAIN_IMAGE
{
	vec2 uv = fragCoord / iResolution.xy;
	fragColor = vec4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
)GLSL";

static const char *kShaderToyHelloMSL = R"MSL(MAIN_IMAGE
{
	float2 uv = fragCoord / iResolution.xy;
	fragColor = float4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
)MSL";

static const char *kShaderToyHelloHLSL = R"HLSL(MAIN_IMAGE
{
	float2 uv = fragCoord / iResolution.xy;
	fragColor = float4(uv.x, uv.y, 0.5 + 0.5 * sin(iTime), 1.0);
}
)HLSL";


// ---------------------------------------------------------------------------
// Plasma -- the oldest trick in the demo, and still the clearest introduction
// to what a fragment shader is: four sine fields summed and fed through a
// cosine palette. No geometry, no raymarching, just arithmetic on a
// coordinate.
// ---------------------------------------------------------------------------

static const char *kShaderToyPlasmaGLSL = R"GLSL(MAIN_IMAGE
{
	vec2 uv = fragCoord / iResolution.xy * 6.0;
	float t = iTime * 0.7;

	float v = sin(uv.x + t)
			+ sin(uv.y * 0.9 - t * 0.8)
			+ sin((uv.x + uv.y) * 0.7 + t * 1.3)
			+ sin(length(uv - 3.0) * 1.4 - t * 1.7);
	v *= 0.25;

	vec3 col = 0.5 + 0.5 * cos(6.2831 * (vec3(0.0, 0.33, 0.67) + v + t * 0.05));
	fragColor = vec4(col, 1.0);
}
)GLSL";

static const char *kShaderToyPlasmaMSL = R"MSL(MAIN_IMAGE
{
	float2 uv = fragCoord / iResolution.xy * 6.0;
	float t = iTime * 0.7;

	float v = sin(uv.x + t)
			+ sin(uv.y * 0.9 - t * 0.8)
			+ sin((uv.x + uv.y) * 0.7 + t * 1.3)
			+ sin(length(uv - 3.0) * 1.4 - t * 1.7);
	v *= 0.25;

	float3 col = 0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.33, 0.67) + v + t * 0.05));
	fragColor = float4(col, 1.0);
}
)MSL";

static const char *kShaderToyPlasmaHLSL = R"HLSL(MAIN_IMAGE
{
	float2 uv = fragCoord / iResolution.xy * 6.0;
	float t = iTime * 0.7;

	float v = sin(uv.x + t)
			+ sin(uv.y * 0.9 - t * 0.8)
			+ sin((uv.x + uv.y) * 0.7 + t * 1.3)
			+ sin(length(uv - float2(3.0, 3.0)) * 1.4 - t * 1.7);
	v *= 0.25;

	float3 col = 0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.33, 0.67) + v + t * 0.05));
	fragColor = float4(col, 1.0);
}
)HLSL";

// ---------------------------------------------------------------------------
// Kaleidoscope -- polar coordinates and a fold, which is the whole idea behind
// most symmetric demo effects. Change kSlices and the symmetry changes with
// it; that one line is the effect.
// ---------------------------------------------------------------------------

static const char *kShaderToyKaleidoGLSL = R"GLSL(MAIN_IMAGE
{
	vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float t = iTime * 0.4;

	float slices = 8.0;
	float wedge = 6.2831 / slices;
	float a = atan(uv.y, uv.x) + t;
	float r = length(uv);

	// Fold the angle into one wedge, then mirror it -- the mirror is what
	// makes the seams meet instead of jumping.
	a = mod(a, wedge);
	a = abs(a - wedge * 0.5);

	vec2 p = vec2(cos(a), sin(a)) * r;
	float rings = sin(r * 12.0 - t * 3.0) * 0.5 + 0.5;
	float spokes = sin(p.x * 14.0) * sin(p.y * 14.0);
	float v = rings * 0.7 + spokes * 0.3;

	vec3 col = 0.5 + 0.5 * cos(6.2831 * (vec3(0.0, 0.4, 0.7) + v + r * 0.3 - t * 0.2));
	col *= smoothstep(1.4, 0.1, r);
	fragColor = vec4(col, 1.0);
}
)GLSL";

static const char *kShaderToyKaleidoMSL = R"MSL(MAIN_IMAGE
{
	float2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float t = iTime * 0.4;

	float slices = 8.0;
	float wedge = 6.2831 / slices;
	float a = atan2(uv.y, uv.x) + t;
	float r = length(uv);

	a = fmod(a, wedge);
	a = abs(a - wedge * 0.5);

	float2 p = float2(cos(a), sin(a)) * r;
	float rings = sin(r * 12.0 - t * 3.0) * 0.5 + 0.5;
	float spokes = sin(p.x * 14.0) * sin(p.y * 14.0);
	float v = rings * 0.7 + spokes * 0.3;

	float3 col = 0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.4, 0.7) + v + r * 0.3 - t * 0.2));
	col *= smoothstep(1.4, 0.1, r);
	fragColor = float4(col, 1.0);
}
)MSL";

static const char *kShaderToyKaleidoHLSL = R"HLSL(MAIN_IMAGE
{
	float2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float t = iTime * 0.4;

	float slices = 8.0;
	float wedge = 6.2831 / slices;
	float a = atan2(uv.y, uv.x) + t;
	float r = length(uv);

	a = fmod(a, wedge);
	a = abs(a - wedge * 0.5);

	float2 p = float2(cos(a), sin(a)) * r;
	float rings = sin(r * 12.0 - t * 3.0) * 0.5 + 0.5;
	float spokes = sin(p.x * 14.0) * sin(p.y * 14.0);
	float v = rings * 0.7 + spokes * 0.3;

	float3 col = 0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.4, 0.7) + v + r * 0.3 - t * 0.2));
	col *= smoothstep(1.4, 0.1, r);
	fragColor = float4(col, 1.0);
}
)HLSL";

// ---------------------------------------------------------------------------
// Texture -- the one preset that SAMPLES ONE CHANNEL.
//
// WHAT IT SAMPLES: iChannel0, reached as texChannel0(uv), which the Channels
// window fills. Out of the box channel 0 holds ImGui's FONT ATLAS -- a real
// texture that exists on every backend with nothing to load -- so this warps a
// sheet of glyphs until you point the slot at a picture of your own.
//
// TWO THINGS HERE ARE THE POINT, and both are new with the channels:
//
//   * NO fract() ON THE COORDINATE. The wrap combo does it: texChannelN
//     expands to a macro that wraps or clamps according to the setting, and
//     scales for the padding CSlrImage adds. Set channel 0 to Clamp and the
//     tiling stops, with nothing here changed.
//
//   * iChannelResolution[0] CORRECTS THE ASPECT. A 3:2 photograph sampled
//     with a square coordinate comes out squashed; one divide by the
//     channel's own aspect ratio fixes it, and that is what the uniform is
//     for.
//
// The rotozoomer is the classic reason to sample in the first place: rotate
// and scale the coordinate, not the image.
// ---------------------------------------------------------------------------

static const char *kShaderToyTextureGLSL = R"GLSL(MAIN_IMAGE
{
	vec2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float t = iTime * 0.5;

	// Rotozoom: rotate and scale the COORDINATE, and the image follows.
	float zoom = 1.5 + sin(t * 0.7) * 0.7;
	float c = cos(t), s = sin(t);
	vec2 rot = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c) * zoom;

	vec2 st = rot * 0.5 + 0.5;
	float chAspect = iChannelResolution[0].x / max(iChannelResolution[0].y, 1.0);
	st.x = (st.x - 0.5) / max(chAspect, 0.001) + 0.5;

	vec4 tex = texChannel0(st);

	// The atlas is white glyphs on transparent black, so tint by a moving
	// palette to make the sampling visible rather than a grey smear.
	vec3 tint = 0.5 + 0.5 * cos(6.2831 * (vec3(0.0, 0.33, 0.67) + length(rot) * 0.2 + t * 0.3));
	fragColor = vec4(tex.rgb * tint + tex.a * tint * 0.6, 1.0);
}
)GLSL";

static const char *kShaderToyTextureMSL = R"MSL(MAIN_IMAGE
{
	float2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float t = iTime * 0.5;

	float zoom = 1.5 + sin(t * 0.7) * 0.7;
	float c = cos(t), s = sin(t);
	float2 rot = float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c) * zoom;

	float2 st = rot * 0.5 + 0.5;
	float chAspect = iChannelResolution[0].x / max(iChannelResolution[0].y, 1.0);
	st.x = (st.x - 0.5) / max(chAspect, 0.001) + 0.5;

	float4 tex = texChannel0(st);

	float3 tint = 0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.33, 0.67) + length(rot) * 0.2 + t * 0.3));
	fragColor = float4(tex.rgb * tint + tex.a * tint * 0.6, 1.0);
}
)MSL";

static const char *kShaderToyTextureHLSL = R"HLSL(MAIN_IMAGE
{
	float2 uv = (2.0 * fragCoord - iResolution.xy) / iResolution.y;
	float t = iTime * 0.5;

	float zoom = 1.5 + sin(t * 0.7) * 0.7;
	float c = cos(t), s = sin(t);
	float2 rot = float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c) * zoom;

	float2 st = rot * 0.5 + 0.5;
	float chAspect = iChannelResolution[0].x / max(iChannelResolution[0].y, 1.0);
	st.x = (st.x - 0.5) / max(chAspect, 0.001) + 0.5;

	float4 tex = texChannel0(st);

	float3 tint = 0.5 + 0.5 * cos(6.2831 * (float3(0.0, 0.33, 0.67) + length(rot) * 0.2 + t * 0.3));
	fragColor = float4(tex.rgb * tint + tex.a * tint * 0.6, 1.0);
}
)HLSL";

// ---------------------------------------------------------------------------
// Two Channels -- what four slots are actually for.
//
// iChannel1 DISPLACES iChannel0: the second texture's red and green are read
// as an offset applied to the first one's coordinate, which is how heat haze,
// water and glass are done in every demo that has them. A one-channel example
// cannot show it, which is the only reason this preset exists.
//
// Load two pictures into channels 0 and 1 from the Channels window. With
// channel 1 empty it samples black -- the offset is then a constant, and the
// effect is a still shift rather than a warp, which is itself worth seeing
// once.
// ---------------------------------------------------------------------------

static const char *kShaderToyTwoChanGLSL = R"GLSL(MAIN_IMAGE
{
	vec2 uv = fragCoord / iResolution.xy;
	float t = iTime * 0.2;

	// Scroll the displacement map so the warp moves without the image doing so.
	vec4 disp = texChannel1(uv * 0.75 + vec2(t, t * 0.5));
	vec2 warped = uv + (disp.rg - 0.5) * 0.2;

	vec4 base = texChannel0(warped);
	// A hint of the displacement map itself, so it is visible what is doing
	// the warping.
	fragColor = vec4(mix(base.rgb, disp.rgb, 0.25) + base.a * 0.4, 1.0);
}
)GLSL";

static const char *kShaderToyTwoChanMSL = R"MSL(MAIN_IMAGE
{
	float2 uv = fragCoord / iResolution.xy;
	float t = iTime * 0.2;

	float4 disp = texChannel1(uv * 0.75 + float2(t, t * 0.5));
	float2 warped = uv + (disp.rg - 0.5) * 0.2;

	float4 base = texChannel0(warped);
	fragColor = float4(mix(base.rgb, disp.rgb, 0.25) + base.a * 0.4, 1.0);
}
)MSL";

static const char *kShaderToyTwoChanHLSL = R"HLSL(MAIN_IMAGE
{
	float2 uv = fragCoord / iResolution.xy;
	float t = iTime * 0.2;

	float4 disp = texChannel1(uv * 0.75 + float2(t, t * 0.5));
	float2 warped = uv + (disp.rg - 0.5) * 0.2;

	float4 base = texChannel0(warped);
	fragColor = float4(lerp(base.rgb, disp.rgb, 0.25) + base.a * 0.4, 1.0);
}
)HLSL";

struct SShaderToyPreset
{
	const char *name;
	const char *glsl;
	const char *msl;
	const char *hlsl;
};

static const SShaderToyPreset kShaderToyPresets[] =
{
	{ "Tunnel",   kShaderToyTunnelGLSL, kShaderToyTunnelMSL, kShaderToyTunnelHLSL },
	{ "Hello UV", kShaderToyHelloGLSL,  kShaderToyHelloMSL,  kShaderToyHelloHLSL  },
	{ "Plasma",   kShaderToyPlasmaGLSL, kShaderToyPlasmaMSL, kShaderToyPlasmaHLSL },
	{ "Kaleidoscope", kShaderToyKaleidoGLSL, kShaderToyKaleidoMSL, kShaderToyKaleidoHLSL },
	{ "Texture",  kShaderToyTextureGLSL, kShaderToyTextureMSL, kShaderToyTextureHLSL },
	{ "Two Channels", kShaderToyTwoChanGLSL, kShaderToyTwoChanMSL, kShaderToyTwoChanHLSL },
};

// "Not one of the presets." The combo shows Custom for it, and LoadPreset
// refuses it -- there is nothing to load.
static const int kShaderToyPresetCustom = -1;

static const int kShaderToyPresetCount = (int)(sizeof(kShaderToyPresets) / sizeof(kShaderToyPresets[0]));

#endif
