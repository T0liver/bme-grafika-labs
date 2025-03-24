//=============================================================================================
// Hullámvasút
//=============================================================================================
#include "framework.h"

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330
    precision highp float;

    uniform mat4 mvp;		// modelnézettér projekciós transzformáció

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1); 	// bemenet már normalizált eszközkoordinátákban
	}
)";

// pixel árnyaló
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans szín
	out vec4 fragmentColor;		// pixel szín

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;
const int pointSize = 10, lineSize = 3;



class Hullamvasut : public glApp {
	Geometry<vec2>* triangle;  // geometria
	GPUProgram* gpuProgram;	   // csúcspont és pixel árnyalók
public:
	Hullamvasut() : glApp("Hullamvasut") {}

	// Inicializáció, 
	void onInitialization() {
		glViewport(0, 0, winWidth, winHeight);

		glPointSize(pointSize);
		glLineWidth(lineSize);

		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	void onMousePressed(MouseButton btn, int pX, int pY) {
		if (btn == MOUSE_LEFT) {

		}
	}

	// Ablak újrarajzolás
	void onDisplay() {
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);     // háttér szín
		glClear(GL_COLOR_BUFFER_BIT); // rasztertár törlés
	}

	~Hullamvasut() {
		delete gpuProgram;
	}
};

Hullamvasut app;

