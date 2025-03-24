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
	vec2 wCenter, wSize;	// ablak k�z�ppontja �s m�rete

public:
	Camera() : wCenter(0, 0), wSize(30, 30) {}

	mat4 View() { return translate(vec3(-wCenter, 0)); }
	mat4 Proj() { return scale(vec3((2/wSize.x), (2/wSize.y), 0)); }

	mat4 ViewI() { return translate(vec3(wCenter, 0)); }
	mat4 ProjI() { return scale(vec3((2 / wSize.x), (2 / wSize.y), 0)); }

	void zoom(float s) { wSize *= s; }
	void move(vec2 t) { wCenter += t; }
};

Camera camera;

class Spline {
	std::vector<vec2> cPs; // controll points
	unsigned int vao, vbo;
	std::vector<vec2> vtcs; // verticles

public:
	Spline() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	~Spline() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}

	void addPoint(vec2 point) {
		vec4 p = camera.ProjI() * camera.ViewI() * vec4(point, 0, 1);
		cPs.push_back(p);
		// updateGPU();
	}

	void genSpline(int res = 100) {
		vtcs.clear();
		for (size_t i = 0; i < cPs.size() - 1; i++) {
			for (int j = 0; j < res; j++) {
				float t = (float)j / (res - 1);
				vec2 p = (1 - t) * cPs[i] + t * cPs[i + 1];
				vtcs.push_back(p);
			}
		}
		updateGPU();
	}

	void Draw(GPUProgram* gpuProg) {
		if (cPs.size() > 0)
		{
			mat4 mvpTrans = camera.Proj() * camera.View();
			gpuProg->setUniform(mvpTrans, "mvp");

			glBindVertexArray(vao);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);

			gpuProg->setUniform(vec3(1.0f, 1.0f, 0.0f), "color");
			glBufferData(GL_ARRAY_BUFFER, vtcs.size() * sizeof(vec2), &vtcs[0], GL_DYNAMIC_DRAW);
			glDrawArrays(GL_LINE_STRIP, 0, vtcs.size());

			gpuProg->setUniform(vec3(1.0f, 0.0f, 0.0f), "color");
			glBufferData(GL_ARRAY_BUFFER, cPs.size() * sizeof(vec2), &cPs[0], GL_DYNAMIC_DRAW);
			glDrawArrays(GL_POINTS, 0, cPs.size());
		}
	}

	void updateGPU() {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtcs.size() * sizeof(vec2), &vtcs[0], GL_DYNAMIC_DRAW);
	}
};

class Hullamvasut : public glApp {
	Geometry<vec2>* triangle;  // geometria
	GPUProgram* gpuProgram;	   // cs�cspont �s pixel �rnyal�k
	Spline* spline;
public:
	Hullamvasut() : glApp("Hullamvasut") {}

	// Inicializ�ci�, 
	void onInitialization() {
		glViewport(0, 0, winWidth, winHeight);

		glPointSize(pointSize);
		glLineWidth(lineSize);

		gpuProgram = new GPUProgram(vertSource, fragSource);
		spline = new Spline();
	}

	void onMousePressed(MouseButton btn, int pX, int pY) {
		if (btn == MOUSE_LEFT) {
			spline->addPoint(vec2(pX, pY));
			spline->genSpline();
			spline->Draw(gpuProgram);
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

