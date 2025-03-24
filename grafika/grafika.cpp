//=============================================================================================
// Hull�mvas�t
//=============================================================================================
#include "framework.h"

// cs�cspont �rnyal�
const char* vertSource = R"(
	#version 330
    precision highp float;

    uniform mat4 mvp;		// modeln�zett�r projekci�s transzform�ci�

	layout(location = 0) in vec2 cP;	// 0. bemeneti regiszter

	void main() {
		gl_Position = vec4(cP.x, cP.y, 0, 1); 	// bemenet m�r normaliz�lt eszk�zkoordin�t�kban
	}
)";

// pixel �rnyal�
const char* fragSource = R"(
	#version 330
    precision highp float;

	uniform vec3 color;			// konstans sz�n
	out vec4 fragmentColor;		// pixel sz�n

	void main() {
		fragmentColor = vec4(color, 1); // RGB -> RGBA
	}
)";

const int winWidth = 600, winHeight = 600;
const int pointSize = 10, lineSize = 3;

class Camera {
	vec3 center;
	float width, height;
public:
	Camera(vec3 center, float width, float height) : center(center), width(width), height(height) {}

	mat4 getViewM() const {
		return lookAt(center + vec3(0, 0, -1), center, vec3(0, 1, 0));
	}

	mat4 getProjM() const {
		return ortho(
			center.x - width / 2, center.x + width / 2,
			center.y - height / 2, center.y + height / 2,
			0.1f, 100.0f);
	}

	mat4 getViewIM() const {
		return inverse(getViewM());
	}

	mat4 getProjIM() const {
		return inverse(getProjM());
	}

	vec3 scrToW(const vec2& cords, const vec2& size) const {
		vec4 viewport = vec4(0, 0, size.x, size.y);
		vec3 win = vec3(cords, 1.0f);
		// mat4 viewProjI = getProjIM() * getViewIM();
		return unProject(win, getViewM(), getViewM(), viewport);
	}
};


class Hullamvasut : public glApp {
	Geometry<vec2>* triangle;  // geometria
	GPUProgram* gpuProgram;	   // cs�cspont �s pixel �rnyal�k
public:
	Hullamvasut() : glApp("Hullamvasut") {}

	// Inicializ�ci�, 
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

	// Ablak �jrarajzol�s
	void onDisplay() {
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);     // h�tt�r sz�n
		glClear(GL_COLOR_BUFFER_BIT); // rasztert�r t�rl�s
	}

	~Hullamvasut() {
		delete gpuProgram;
	}
};

Hullamvasut app;

