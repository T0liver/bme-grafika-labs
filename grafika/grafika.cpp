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
		gl_Position = vec4(cP.x, cP.y, 0, 1);
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

enum Mode
{
	POINT,
	LINE,
	MOVE,
	INTERSECT
	
};

enum MBtn
{
	DOWN,
	UP
};

GPUProgram* gpuProgram;

// Segédfüggvények
float pointDistance(const vec3 p1, const vec3 p2) {
	float ret;
	ret = sqrtf(powf(p1.x - p2.x, 2) + powf(p1.y - p2.y, 2));
	return ret;
}

bool vecEq(const vec3 lhs, const vec3& rhs) {
	if (fabs(lhs.x - rhs.x) <= 0.01f)
	{
		if (fabs(lhs.y - rhs.y) <= 0.01f)
		{
			if (fabs(lhs.z - rhs.z) <= 0.01f)
			{
				return true;
			}
		}
	}
	return false;
}

// Osztályok
class Object {
protected:
	unsigned int vao, vbo;
	std::vector<vec3> vtx;
public:
	Object() : vao(0), vbo(0) {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(0);
	}

	void setVtxs(const std::vector<vec3> verticles) {
		vtx = verticles;
	}

	void update() {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vtx.size() * sizeof(vec3), &vtx[0], GL_DYNAMIC_DRAW);
	}

	void Draw(GLenum type, vec3 col) {
		if (vtx.size() > 0)
		{
			// drawwwwww
			glBindVertexArray(vao);
			gpuProgram->setUniform(col, "color");
			glDrawArrays(type, 0, vtx.size());
		}
	}

	std::vector<vec3>& getVtxs() {
		return vtx;
	}

	~Object() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}


};



class PointCollection : public Object {
public:
	PointCollection() : Object() {}

	void addPoint(const vec3 point) {
		vtx.push_back(point);
		// printf("=========\nPoint added: %f, %f\n", point.x, point.y);
		update();
	}

	vec3 findNearest(const vec3 p) {
		vec3 nearest{};
		float min = 10000;

		for (size_t i = 0; i < vtx.size(); ++i) {
			if (pointDistance(p, vtx[i]) < min)
			{
				min = pointDistance(p, vtx[i]);
				nearest = vtx[i];
			}
		}

		return nearest;
	}

	void Draw(vec3 color) {
		Object::Draw(GL_POINTS, color);
	}
};

class Line : public Object {
public:
	vec3 p1, p2;

	float a, b, c; // Ax + By + C = 0

	Line() : p1(vec3(0, 0, -1)), p2(vec3(0, 0, -1)) {}


	Line(const vec3 p1, const vec3 p2) : Object(), p1(p1), p2(p2) {
		a = p1.y - p2.y;
		b = p2.x - p1.x;
		c = p1.x * p2.y - p2.x * p1.y;
	}

	static vec3 getIntersect(const Line& l1, const Line& l2) {
		float det = l1.a * l2.b - l2.a * l1.b;
		if (fabs(det) < 1e-6)
		{
			return vec3(-2.0f, -2.0f, -2.0f);
		}
		float mx = (l1.b * l2.c - l2.b * l1.c) / det;
		float my = (l2.a * l1.c - l1.a * l2.c) / det;
		return vec3(mx, my, 1);
	}

	bool isPointOnLine(const vec3 p) {
		float dist = abs(a * p.x + b * p.y + c) / sqrt(a * a + b * b);
		return dist < 0.01;
	}

	void clipToBox() {
		vec3 start(0.0f, 0.0f, -1.0f), end(0.0f, 0.0f, -1.0f);

		bool startSet = false;

		float top = (p2.x - p1.x) * (1 - p1.y) / (p2.y - p1.y) + p1.x;
		vec3 topPoint = vec3(top, 1.0f, 1.0f);

		float bottom = (p2.x - p1.x) * (-1 - p1.y) / (p2.y - p1.y) + p1.x;
		vec3 bottomPoint = vec3(bottom, -1.0f, 1.0f);

		float left = (p2.y - p1.y) * (-1 - p1.x) / (p2.x - p1.x) + p1.y;
		vec3 leftPoint = vec3(-1, left, 1);

		float right = (p2.y - p1.y) * (1 - p1.x) / (p2.x - p1.x) + p1.y;
		vec3 rightPoint = vec3(1, right, 1);

		if (fabs(top) <= 1.0f)
		{
			if (!startSet) {
				start = topPoint;
				startSet = true;
			}
			else {
				end = topPoint;
			}
		}

		if (fabs(bottom) <= 1.0f)
		{
			if (!startSet) {
				start = bottomPoint;
				startSet = true;
			}
			else {
				end = bottomPoint;
			}
		}
		if (fabs(left) <= 1.0f) {
			if (!startSet) {
				start = leftPoint;
				startSet = true;
			}
			else {
				start = leftPoint;
			}
		}

		if (fabs(right) <= 1.0f) {
			if (!startSet) {
				start = rightPoint;
				startSet = true;
			}
			else {
				end = rightPoint;
			}
		}

		p1 = start;
		p2 = end;

	}

	void translateLine(const vec3& p) {
		// mid point: vec3((p1.x + p2.x) / 2, (p1.y + p2.y) / 2, 1);
		vec3 trans = vec3(p.x - p1.x, p.y - p1.y, 0);
		// vec3 trans = vec3(p.x - ((p1.x + p2.x) / 2), p.y - ((p1.y + p2.y) / 2), 0);

		p1.x += trans.x;
		p1.y += trans.y;
		p2.x += trans.x;
		p2.y += trans.y;

		a = p1.y - p2.y;
		b = p2.x - p1.x;
		c = p2.y * p1.x - p1.y * p2.x;
	}
};

/// Random segedfuggveny, ami csak itt fog mukodni
float pointDistToLine(const vec3 point, const Line& line) {
	float ret;
	ret = fabs(line.a * point.x + line.b * point.y + line.c)
		/ sqrt(line.a * line.a + line.b * line.b);
	return ret;
}

class LineCollection : public Object {
public:
	LineCollection() : Object() {}

	void addLine(vec3 p1, vec3 p2) {
		Line line = Line(p1, p2);
		line.clipToBox();
		
		vtx.push_back(line.p1);
		vtx.push_back(line.p2);

		printf("=========\nLine added\n");
		printf("Implicit: %.2fx + %.2fy + %.2f = 0\n", line.a, line.b, line.c);
		printf("Parametric: r(t) = (%.2f, %.2f) + t * (%.2f, %.2f)\n", line.p1.x, line.p2.y, line.p2.x - line.p1.x, line.p2.y - line.p1.y);
		
		update();
	}

	Line* findNearest(vec3 point) {
		vec3 pt1, pt2;
		float mindist = 1000;
		Line* nearest = nullptr;

		for (size_t i = 0; i < vtx.size(); i = i+2)
		{
			pt1 = vtx[i];
			pt2 = vtx[i + 1];

			Line* l = new Line(pt1, pt2);

			float dist = pointDistToLine(point, *l);

			if (dist < mindist)
			{
				mindist = dist;
				nearest = l;
			}
		}

		return nearest;
	}

	void updateLineVertices(Line* line) {
		for (size_t i = 0; i < vtx.size(); i += 2)
		{
			if (vecEq(vtx[i], line->p1) && vecEq(vtx[i + 1], line->p2))
			{
				vtx[i] = line->p1;
				vtx[i + 1] = line->p2;
				update();
				return;
			}
		}
	}

	void Draw(vec3 color) {
		Object::Draw(GL_LINES, color);
	}
};


class PointsAndLines : public glApp {
	Mode mode = POINT;
	MBtn mmode = UP;

	PointCollection* points;
	LineCollection* lines;

	vec3 lastP1 = vec3(0.0f, 0.0f, -1.0f), lastP2 = vec3(0.0f, 0.0f, -1.0f);

	Line *lastL1 = nullptr, *lastL2 = nullptr;
public:
	PointsAndLines() : glApp("Points and lines") {}

	// Inicializacio
	void onInitialization() {
		glViewport(0, 0, winWidth, winHeight);

		points = new PointCollection();
		lines = new LineCollection();

		glPointSize(pointSize);
		glLineWidth(lineSize);

		gpuProgram = new GPUProgram(vertSource, fragSource);
	}

	// Kozosen hasznalt vonal valtozo
	Line* nrst;

	// Billenytuzet
	void onKeyboard(int key) {
		nrst = nullptr;
		switch (key)
		{
			case 'p':
				mode = POINT;
				printf("== Mode: POINT\n");
				break;
			case 'l':
				mode = LINE;
				printf("== Mode: LINE\n");
				break;
			case 'm':
				mode = MOVE;
				printf("== Mode: MOVE\n");
				break;
			case 'i':
				mode = INTERSECT;
				printf("== Mode: INTERSECT\n");
				break;
		}
	}


	// Egergombok
	void onMousePressed(MouseButton btn, int pX, int pY) {
		float nX = 2.0f * pX / winWidth - 1;
		float nY = 1.0f - 2.0f * pY / winHeight;

		vec3* intrsct;
		Line* tmp = nullptr;

		if (btn == MOUSE_LEFT)
		{
			switch (mode)
			{
			case POINT:
				points->addPoint(vec3(nX, nY, 1));
				printf("=========\nPoint added: %f, %f\n", nX, nY);
				break;
			case LINE:
				vec3 selPoint = points->findNearest(vec3(nX, nY, 0));
				if (selPoint.z != -1)
				{
					if (vecEq(lastP1, vec3(0.0f, 0.0f, -1.0))) {
						lastP1 = vec3(selPoint.x, selPoint.y, 1);
					}
					else if (!vecEq(lastP1, vec3(selPoint.x, selPoint.y, 1))) {
						lastP2 = vec3(selPoint.x, selPoint.y, 1);
						lines->addLine(lastP1, lastP2);

						lastP1 = vec3(0.0f, 0.0f, -1.0f), lastP2 = vec3(0.0f, 0.0f, -1.0f);
					}
				}
				break;
			case INTERSECT:
				tmp = lines->findNearest(vec3(nX, nY, 0));

				if (tmp->p2.z != -1.0f)
				{
					if (lastL1 == nullptr)
					{
						lastL1 = tmp;
						tmp = nullptr;
					} else 
					{
						lastL2 = tmp;
						tmp = nullptr;

						intrsct = new vec3(lastL1->getIntersect(*lastL1, *lastL2));
						if (intrsct->z != -2.0f)
						{
							points->addPoint(*intrsct);
							printf("=========\nIntersection point added: %f, %f\n", intrsct->x, intrsct->y);
						}

						delete intrsct;
						delete lastL1;
						delete lastL2;

						intrsct = nullptr;
						lastL1 = nullptr;
						lastL2 = nullptr;
					}
				}
				break;
			case MOVE:
				nrst = lines->findNearest(vec3(nX, nY, 0));
				printf("click: %f, %f\n", nX, nY);
				mmode = DOWN;
				break;
			default:
				break;
			}
		}
		refreshScreen();
	}

	void onMouseReleased(MouseButton but, int pX, int pY) {
		mmode = UP;
	}

	void onMouseMotion(int pX, int pY) {
		if (mode == MOVE && mmode == DOWN)
		{

			float nX = 2.0f * pX / winWidth - 1;
			float nY = 1.0f - 2.0f * pY / winHeight;

			if (nrst != nullptr && nrst->p2.z != -1.0f)
			{

				printf("drag: %f, %f", nX, nY);
				Line* tmpline = new Line(nrst->p1, nrst->p2);
				tmpline->translateLine(vec3(nX, nY, 0));

				std::vector<vec3>* newVtx = &lines->getVtxs();

				for (size_t i = 0; i < newVtx->size() - 1; i++)
				{
					if ((vecEq(newVtx->at(i), nrst->p1) && vecEq(newVtx->at(i + 1), nrst->p2)) ||
						(vecEq(newVtx->at(i), nrst->p2) && vecEq(newVtx->at(i + 1), nrst->p1)))
					{
						printf("\tLine moved\t");
						Line tmp;
						tmp.p1 = tmpline->p1;
						tmp.p2 = tmpline->p2;
						tmp.clipToBox();

						nrst->p1 = tmp.p1;
						nrst->p2 = tmp.p2;
						nrst->clipToBox();

						newVtx->at(i) = nrst->p1;
						newVtx->at(i + 1) = nrst->p2;
					}

				}

				lines->update();
				refreshScreen();
			}
		}
	}

	// Ablak ujrarajzolas
	void onDisplay() {
		glClearColor(0.0f, 0.0f, 0.0f, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		points->Draw(vec3(1.0f, 0.0f, 0.0f));
		lines->Draw(vec3(0.0f, 1.0f, 1.0f));

		refreshScreen();
	}
};

PointsAndLines app;

