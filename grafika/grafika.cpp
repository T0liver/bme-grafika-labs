//=============================================================================================
// Grafika 1. h�zifeladat.
//=============================================================================================
#include "framework.h"

///===========
/// 1. Egyenletek
/// 2D egyenes implicit (norm�lvektoros) egyenlete: Ax + By + C = 0
/// 2D egyenes param�teres egyenletei: x = x0 + t * dx	;	y = y0 + t * dy
/// param�teres egyenlet param�terei => implicit: A = dy, B = -dx, C = - (Ax0 + By0) = dx * y0 - dy * x0
/// implicit egyenlet => param�teres: (dx, dy) = (B, -A), (x0, y0) = 
/// egyenlet k�t pontb�l: P1(x1, y1); P2(x2, y2);	A = y1 - y2, B = x2 - x1, C = y2 * x1 - y1 * x2
/// ======
/// 2. Metsz�spont k�pletek
/// a) x = (B1 * C1 - B2 * C1) / (A1 * B2 - A2 * B1)	;	y = (A2 * C1 - A1 * C2) / (A1 * B2 - A2 * B1)
/// b) t1 = ((x20 - x10) + t2 * a2)/a1 ;	t2 = ((y10 - y20) + ((x20 - x10) * b1)/a1) / (b2 - (a2 * b1)/a1)
/// c) x = x0 + t * dx	;	y = y0 + t * dy
/// ======
/// 3. Ne essen a (-1, -1);(1, 1) n�gyzetbe az egyenes k�t pontja
/// P1(x1,y1) legyen a n�gyzet bal oldal�n; P2(x2,y2) legyen a n�gyzet jobb oldal�n
/// j� ha !(1 > x1 > -1 �s 1 > y1 > -1 �s 1 > x2 > -2 �s 1 > y2 > -1)
/// ======
/// 4. Pont �s egyenes t�vols�ga
/// implicit egyenlettel:		d = |A * x0 + B * y0 + C| / sqrt(A^2 + B^2)
/// param�teres egyenlettel:	d = |(py - y0) * dx - (px - x0) * dy| / sqrt(dx^2 + dy^2)
/// ======
/// 5. Teglalap sarkai
/// Adottak tetszoleges P1(x1, y1) �s P2(x2, y2) pontok.
/// Elso sarok:		x1, -y1
/// Masodik sarok:	x2, -y2
/// Harmadik sarok:	-x1, y2
/// Negyedik sarok:	-x1, y2
/// ===========

// cs�cspont �rnyal�
const char* vertSource = R"(
	#version 330				
    precision highp float;

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


GPUProgram* gpuProgram;


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