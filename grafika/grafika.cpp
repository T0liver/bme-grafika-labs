//=============================================================================================
// Grafika 1. házifeladat.
//=============================================================================================
#include "framework.h"

///===========
/// 1. Egyenletek
/// 2D egyenes implicit (normálvektoros) egyenlete: Ax + By + C = 0
/// 2D egyenes paraméteres egyenletei: x = x0 + t * dx	;	y = y0 + t * dy vagy r(t) = r0 + t * v
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
		printf("=========\nPoint added: %f, %f\n", point.x, point.y);
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

class Line : public Object {
	vec2 p1, p2;
	float a, b, c; // Ax + By + C = 0

public:
	Line(const vec2 p1, const vec2 p2) : Object(), p1(p1), p2(p2) {
		a = p1.y - p2.y;
		b = p2.x - p1.x;
		c = p2.y * p1.x - p1.y * p2.x;
		printf("=========\nLine added\n");
		printf("Implicit: %fx + %fy + %f = 0\n", a, b, c);
		printf("Parametric: r(t) = (%f, %f) + t * (%f, %f)\n", p1.x, p2.y, p2.x - p1.x, p2.y - p1.y);
	}

	vec2 getIntersect(const Line& l2) {
		float det = this->a * l2.b - l2.a * this->b;
		if (fabs(det) < 1e-6)
		{
			throw "Lines are parallel!";
		}
		float mx = (this->b * this->c - l2.b * this->c) / det;
		float my = (l2.a * this->c -this->a * l2.c) / det;
		return vec2(mx, my);
	}

	bool isPointOnLine(const vec2 p) {
		float dist = abs(a * p.x + b * p.y + c) / sqrt(a * a + b * b);
		return dist < 0.01;
	}

	void clipToBox(vec2 start, vec2 end) {

		/// minX, maxY      maxX, maxY
		///		 +-----------+
		///		 |start      |
		///	     |           |
		///		 |           |
		///		 |        end|
		///		 +-----------+
		///  minX, minY      maxX, minY
		/// 

		float minX = -1.0f, maxX = 1.0f, minY = -1.0f, maxY = 1.0f;
		
		vec2 left;
		try { left = getIntersect(Line(vec2(minX, minY), vec2(minX, maxY))); }
		catch (const std::exception&) { left = vec2(-2.0f -2.0f); }
		vec2 right;
		try { right = getIntersect(Line(vec2(maxX, minY), vec2(maxX, maxY))); }
		catch (const std::exception&) { right = vec2(-2.0f - 2.0f); }
		vec2 top;
		try { top = getIntersect(Line(vec2(minX, maxY), vec2(maxX, maxY))); }
		catch (const std::exception&) { top = vec2(-2.0f - 2.0f); }
		vec2 bottom;
		try { bottom = getIntersect(Line(vec2(minX, maxY), vec2(maxX, maxY))); }
		catch (const std::exception&) { bottom = vec2(-2.0f - 2.0f); }

		if (fabs(left.y) <= 1)
		{
			start = left;
		}
		else if (fabs(top.x) <= 1)
		{
			start = top;
		}
		else {
			start = bottom;
		}

		if (fabs(bottom.x) <= 1 && equal(start, bottom).y)
		{
			end = bottom;
		}
		else if (fabs(right.y) <= 1) {
			end = right;
		}
		else {
			end = top;
		}

	}

	void translateLine(const vec2& p) {
		vec2 trans = vec2(p.x - p1.x, p.y - p1.y);

		p1.x += trans.x;
		p1.y += trans.y;
		p2.x += trans.x;
		p2.y += trans.y;

		a = p1.y - p2.y;
		b = p2.x - p1.x;
		c = p2.y * p1.x - p1.y * p2.x;
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