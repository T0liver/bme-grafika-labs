//=============================================================================================
// Grafika 1. házifeladat.
//=============================================================================================
#include "framework.h"

///===========
/// 1. Egyenletek
/// 2D egyenes implicit (normálvektoros) egyenlete: Ax + By + C = 0
/// 2D egyenes paraméteres egyenletei: x = x0 + t * dx	;	y = y0 + t * dy
/// paraméteres egyenlet paraméterei => implicit: A = dy, B = -dx, C = - (Ax0 + By0) = dx * y0 - dy * x0
/// implicit egyenlet => paraméteres: (dx, dy) = (B, -A), (x0, y0) = 
/// egyenlet két pontból: P1(x1, y1); P2(x2, y2);	A = y1 - y2, B = x2 - x1, C = y2 * x1 - y1 * x2
/// ======
/// 2. Metszéspont képletek
/// a) x = (B1 * C1 - B2 * C1) / (A1 * B2 - A2 * B1)	;	y = (A2 * C1 - A1 * C2) / (A1 * B2 - A2 * B1)
/// b) t1 = ((x20 - x10) + t2 * a2)/a1 ;	t2 = ((y10 - y20) + ((x20 - x10) * b1)/a1) / (b2 - (a2 * b1)/a1)
/// c) x = x0 + t * dx	;	y = y0 + t * dy
/// ======
/// 3. Ne essen a (-1, -1);(1, 1) négyzetbe az egyenes két pontja
/// P1(x1,y1) legyen a négyzet bal oldalán; P2(x2,y2) legyen a négyzet jobb oldalán
/// jó ha !(1 > x1 > -1 és 1 > y1 > -1 és 1 > x2 > -2 és 1 > y2 > -1)
/// vagyis |x| > 1 && |y| > 1
/// ======
/// 4. Pont és egyenes távolsága
/// implicit egyenlettel:		d = |A * x0 + B * y0 + C| / sqrt(A^2 + B^2)
/// paraméteres egyenlettel:	d = |(py - y0) * dx - (px - x0) * dy| / sqrt(dx^2 + dy^2)
/// ======
/// 5. Teglalap sarkai
/// Adottak tetszoleges P1(x1, y1) és P2(x2, y2) pontok.
/// Elso sarok:		x1, -y1
/// Masodik sarok:	x2, -y2
/// Harmadik sarok:	-x1, y2
/// Negyedik sarok:	-x1, y2
/// ===========

// csúcspont árnyaló
const char* vertSource = R"(
	#version 330				
    precision highp float;

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


GPUProgram* gpuProgram;

// Segédfüggvények
float pointDistance(const vec2 p1, const vec2 p2) {
	float ret;
	ret = sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
	return ret;
}

// Osztályok
class Object {
	unsigned int vao, vbo;
	std::vector<vec2> vtxs;
public:
	Object() : vao(0), vbo(0) {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(0);
	}

	void setVtxs(const std::vector<vec2> verticles) {
		vtxs = verticles;
	}

	void update() {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtxs.size() * sizeof(vec2), &vtxs[0], GL_DYNAMIC_DRAW);
	}

	void Draw(GLenum type, vec3 col) {
		if (vtxs.size() > 0)
		{
			// drawwwwww
			glBindVertexArray(vao);
			gpuProgram->setUniform(col, "color");
			glDrawArrays(type, 0, vtxs.size());
		}
	}

	std::vector<vec2>& getVtxs() {
		return vtxs;
	}

	~Object() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}


};



class PointCollection : public Object {
public:
	PointCollection() : Object() {}

	void addPoint(const vec2 point) {
		this->getVtxs().push_back(point);
		printf("Point added: %f, %f", point.x, point.y);
	}

	vec2 findNearest(const vec2 p) {
		vec2 nearest{};
		float min = 10000;

		for (size_t i = 0; i < this->getVtxs().size(); ++i) {
			if (pointDistance(p, this->getVtxs()[i]) < min)
			{
				nearest = this->getVtxs()[i];
			}
		}

		return nearest;
	}

	void Draw(vec3 color) {
		Object::Draw(GL_POINTS, color);
	}
};

class PointsAndLines : public glApp {
public:
	PointsAndLines() : glApp("Points and lines") {}

	// Inicializacio
	void onInitialization() {
		
	}

	// Ablak ujrarajzolas
	void onDisplay() {
		
	}
};

PointsAndLines app;