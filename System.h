#pragma once

#include "SceneGraph.h"
#include "platform.h"

#include "WindowManager_win.h"

class System {
public:
	MasterWindow* mainWindow;
	virtual void initVulkan(DrawList drawList, std::string cameraName) = 0;
	virtual void cleanup() = 0 ;
	virtual void drawFrame() = 0;
};