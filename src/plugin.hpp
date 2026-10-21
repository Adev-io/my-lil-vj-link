// plugin.hpp: the plugin's model and the panel-colour lights.
// Author: Ari Klopfer / A_Development · MIT
#pragma once
#include <rack.hpp>
using namespace rack;
extern Plugin* pluginInstance;
extern Model* modelVjLink;

// The panel colours as lights (tools/panel.py uses the same two).
struct LimeLight : GrayModuleLightWidget { LimeLight() { addBaseColor (nvgRGB (0xc8, 0xff, 0x3a)); } };
struct MagentaLight : GrayModuleLightWidget { MagentaLight() { addBaseColor (nvgRGB (0xff, 0x3d, 0x9a)); } };
