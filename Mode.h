#pragma once
#include "ProgramMode.h"
#include "WindowManager_win.h"
#include "VulkanSystem.h"
#include "chrono"
#include "Events.h"
#include "RTSystem.h"
struct MoveStatus {
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool forward = false;
	bool backward = false;
	std::pair<int, int> mouseDelta;
};
class Mode : ProgramMode
{
public:
	void handleinputEvent(InputEvent inputEvent);
	void listPhysicalDevices() {
		vulkanSystem.listPhysicalDevices();
	}
	int modeMain();

	MasterWindow* mainWindow;
	Platform platform;
	float speed = 10.f;
	MovementMode movementMode;
	std::pair<int, int> defaultCursorPos;

	//Pre run time user input
	bool useInstancing = false;
	bool useRT = false;
	bool verbose = false;
	bool culling = true;
	int poolSize = MAX_POOL;
	bool animate = true;
	int numSamples = 1;
	int numBounces = 1;
	int doReflect = 0;
	std::string sceneName;
	std::string cameraName;
	std::string deviceName;
	std::string eventName;
	std::string shaderDir;
	
	HeadlessEvents events;
private:
	VulkanSystem vulkanSystem;
	RTSystem rtSystem;
	void mainLoop(SceneGraph* graph, bool rt = false);
	MoveStatus moveStatus;
	std::chrono::high_resolution_clock::time_point lastFrame;
	std::pair<int, int> lastMousePos;
	bool mouseEventActive = false;

	void handleMouseMove(int x, int y);
	void handleScrollEvent(int delta);
	void handleButtonEvent(ButtonEvent event);
	void handleCharEvent(wchar_t inChar);
};

