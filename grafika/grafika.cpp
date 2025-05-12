#include "framework.h"

//---------------------------
template<class T> struct Dnum { // Dual numbers for automatic derivation
//---------------------------
	float f; // function value
	T d;  // derivatives
	Dnum(float f0 = 0, T d0 = T(0)) { f = f0, d = d0; }
	Dnum operator+(Dnum r) { return Dnum(f + r.f, d + r.d); }
	Dnum operator-(Dnum r) { return Dnum(f - r.f, d - r.d); }
	Dnum operator*(Dnum r) {
		return Dnum(f * r.f, f * r.d + d * r.f);
	}
	Dnum operator/(Dnum r) {
		return Dnum(f / r.f, (r.f * d - r.d * f) / r.f / r.f);
	}
};

// Elementary functions prepared for the chain rule as well
template<class T> Dnum<T> Exp(Dnum<T> g) { return Dnum<T>(expf(g.f), expf(g.f) * g.d); }
template<class T> Dnum<T> Sin(Dnum<T> g) { return  Dnum<T>(sinf(g.f), cosf(g.f) * g.d); }
template<class T> Dnum<T> Cos(Dnum<T>  g) { return  Dnum<T>(cosf(g.f), -sinf(g.f) * g.d); }
template<class T> Dnum<T> Tan(Dnum<T>  g) { return Sin(g) / Cos(g); }
template<class T> Dnum<T> Sinh(Dnum<T> g) { return  Dnum<T>(sinh(g.f), cosh(g.f) * g.d); }
template<class T> Dnum<T> Cosh(Dnum<T> g) { return  Dnum<T>(cosh(g.f), sinh(g.f) * g.d); }
template<class T> Dnum<T> Tanh(Dnum<T> g) { return Sinh(g) / Cosh(g); }
template<class T> Dnum<T> Log(Dnum<T> g) { return  Dnum<T>(logf(g.f), g.d / g.f); }
template<class T> Dnum<T> Pow(Dnum<T> g, float n) {
	return  Dnum<T>(powf(g.f, n), n * powf(g.f, n - 1) * g.d);
}

typedef Dnum<vec2> Dnum2;

const int tessellationLevel = 20;
const int windowWidth = 600, windowHeight = 600; // window size

mat4 RotationMatrix(vec3 axis, float angle) {
	axis = normalize(axis);
	float c = cos(angle), s = sin(angle);
	return mat4(
		c + (1 - c) * axis.x * axis.x,
		(1 - c) * axis.x * axis.y + s * axis.z,
		(1 - c) * axis.x * axis.z - s * axis.y,
		0,

		(1 - c) * axis.x * axis.y - s * axis.z,
		c + (1 - c) * axis.y * axis.y,
		(1 - c) * axis.y * axis.z + s * axis.x,
		0,

		(1 - c) * axis.x * axis.z + s * axis.y,
		(1 - c) * axis.y * axis.z - s * axis.x,
		c + (1 - c) * axis.z * axis.z,
		0,

		0, 0, 0, 1
	);
}

//---------------------------
struct Camera { // 3D camera
//---------------------------
	vec3 wEye, wLookat, wVup;   // extrinsic
	float fov, asp, fp, bp;		// intrinsic
public:
	Camera() {
		asp = (float)windowWidth / windowHeight;
		fov = 75.0f * (float)M_PI / 180.0f;
		fp = 1; bp = 20;
	}
	mat4 V() { // view matrix: translates the center to the origin
		vec3 w = normalize(wEye - wLookat);
		vec3 u = normalize(cross(wVup, w));
		vec3 v = cross(w, u);
		mat4 rotation = mat4(
			u.x, u.y, u.z, 0,
			v.x, v.y, v.z, 0,
			w.x, w.y, w.z, 0,
			0, 0, 0, 1
		);
		mat4 translation = translate(-wEye);
		return rotation * translation;
	}

	mat4 P() { // projection matrix
		return mat4(1 / (tan(fov / 2) * asp), 0, 0, 0,
			0, 1 / tan(fov / 2), 0, 0,
			0, 0, -(fp + bp) / (bp - fp), -1,
			0, 0, -2 * fp * bp / (bp - fp), 0);
	}

	void Spin(const float angle = M_PI_4) {
		vec3 d = wEye - wLookat;

		wEye = vec3(
			d.x * cos(angle) + d.z * sin(angle),
			d.y,
			-d.x * sin(angle) + d.z * cos(angle)
		) + wLookat;
		printf("You spin my head right round, right round... by [45] degrees\n");
	}

	void Turn(const int _dir, const float _angle) {
		vec3 viewDir = normalize(wLookat - wEye);
		vec3 right = normalize(cross(viewDir, wVup));
		vec3 up = normalize(cross(right, viewDir));

		mat4 rot;
		switch (_dir) {
		case 1: // balra
			rot = RotationMatrix(up, _angle);
			break;
		case 2: // jobbra
			rot = RotationMatrix(up, -_angle);
			break;
		case 3: // fel
			rot = RotationMatrix(right, _angle);
			break;
		case 4: // le
			rot = RotationMatrix(right, -_angle);
			break;
		}

		vec4 newViewDir = rot * vec4(viewDir, 0);
		wLookat = wEye + vec3(newViewDir);
		wVup = normalize(cross(right, vec3(newViewDir)));
	}

	void Move(const int _dir) {
		vec3 where;

		switch (_dir) {
		case 1: // balra
			where = wEye + vec3(0.0f, 0.0f, 1.0f);
			break;
		case 2: // jobbra
			where = wEye + vec3(0.0f, 0.0f, -1.0f);
			break;
		case 3: // elore
			where = wEye + vec3(1.0f, 0.0f, 0.0f);
			break;
		case 4: // hatra
			where = wEye + vec3(-1.0f, 0.0f, 1.0f);
			break;
		}

		wEye = where;
	}
};

//---------------------------
struct Material {
//---------------------------
	vec3 kd, ks, ka;
	float shininess;
};

//---------------------------
struct Light {
//---------------------------
	vec3 La, Le;
	vec4 wLightPos; // homogeneous coordinates, can be at ideal point
};

//---------------------------
class CheckerBoardTexture : public Texture {
//---------------------------
public:
	CheckerBoardTexture(const int width, const int height) : Texture(width, height) {
		std::vector<vec4> image(width * height);
		const vec4 white(1.0f, 1.0f, 1.0f, 1.0f), blue(0.0f, 0.0f, 1.0f, 1.0f);
		for (int x = 0; x < width; x++) for (int y = 0; y < height; y++) {
			image[y * width + x] = (x & 1) ^ (y & 1) ? white : blue;
		}
		//create(width, height, image, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, &image[0]); // To GPU
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // sampling
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
};
//---------------------------
class CheckerTexture : public Texture {
//---------------------------
public:
	CheckerTexture(int size = 20) : Texture(size, size) {
		std::vector<vec4> image(size * size);
		const vec4 blue(0.0f, 0.1f, 0.3f, 1.0f);
		const vec4 white(0.3f, 0.3f, 0.3f, 1.0f);

		for (int x = 0; x < size; x++) {
			for (int y = 0; y < size; y++) {
				bool isWhite = ((x + y) % 2 == 1);
				image[y * size + x] = isWhite ? white : blue;
			}
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_FLOAT, &image[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
};

//---------------------------
class RoughTexture : public Texture {
//---------------------------
public:
	RoughTexture(int size = 64) : Texture(size, size) {
		std::vector<vec4> image(size * size);

		for (int y = 0; y < size; ++y) {
			for (int x = 0; x < size; ++x) {
				float noise = float(rand()) / RAND_MAX;
				vec3 color = vec3(0.3f + 0.1f * noise);
				image[y * size + x] = vec4(color, 1.0f);
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_FLOAT, &image[0]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}
};

//---------------------------
class SolidTexture : public Texture {
//---------------------------
public:
	SolidTexture() : Texture(1, 1) {
		std::vector<vec4> image(1, vec4(1.0f, 1.0f, 1.0f, 1.0f));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, &image[0]);
	}
};

//---------------------------
struct RenderState {
//---------------------------
	mat4	           MVP, M, Minv, V, P;
	Material* material;
	std::vector<Light> lights;
	Texture* texture;
	vec3	           wEye;
};

//---------------------------
class Shader : public GPUProgram {
//---------------------------
public:
	virtual void Bind(RenderState state) = 0;

	void setUniformMaterial(const Material& material, const std::string& name) {
		setUniform(material.kd, name + ".kd");
		setUniform(material.ks, name + ".ks");
		setUniform(material.ka, name + ".ka");
		setUniform(material.shininess, name + ".shininess");
	}

	void setUniformLight(const Light& light, const std::string& name) {
		setUniform(light.La, name + ".La");
		setUniform(light.Le, name + ".Le");
		setUniform(light.wLightPos, name + ".wLightPos");
	}
};

//---------------------------
class PhongShader : public Shader {
//---------------------------
	const char* vertexSource = R"(
		#version 330
		precision highp float;

		struct Light {
			vec3 La, Le;
			vec4 wLightPos;
		};

		uniform mat4  MVP, M, Minv; // MVP, Model, Model-inverse
		uniform Light[8] lights;    // light sources 
		uniform int   nLights;
		uniform vec3  wEye;         // pos of eye

		layout(location = 0) in vec3  vtxPos;            // pos in modeling space
		layout(location = 1) in vec3  vtxNorm;      	 // normal in modeling space
		layout(location = 2) in vec2  vtxUV;

		out vec3 wNormal;		    // normal in world space
		out vec3 wView;             // view in world space
		out vec3 wLight[8];		    // light dir in world space
		out vec2 texcoord;

		void main() {
			gl_Position = MVP * vec4(vtxPos, 1); // to NDC
			// vectors for radiance computation
			vec4 wPos = vec4(vtxPos, 1) * M;
			for(int i = 0; i < nLights; i++) {
				wLight[i] = lights[i].wLightPos.xyz * wPos.w - wPos.xyz * lights[i].wLightPos.w;
			}
		    wView  = wEye * wPos.w - wPos.xyz;
		    wNormal = (Minv * vec4(vtxNorm, 0)).xyz;
		    texcoord = vtxUV;
		}
	)";

	// fragment shader in GLSL
	const char* fragmentSource = R"(
		#version 330
		precision highp float;

		struct Light {
			vec3 La, Le;
			vec4 wLightPos;
		};

		struct Material {
			vec3 kd, ks, ka;
			float shininess;
		};

		uniform Material material;
		uniform Light[8] lights;    // light sources 
		uniform int   nLights;
		uniform sampler2D diffuseTexture;

		in  vec3 wNormal;       // interpolated world sp normal
		in  vec3 wView;         // interpolated world sp view
		in  vec3 wLight[8];     // interpolated world sp illum dir
		in  vec2 texcoord;
		
        out vec4 fragmentColor; // output goes to frame buffer

		void main() {
			vec3 N = normalize(wNormal);
			vec3 V = normalize(wView); 
			if (dot(N, V) < 0) N = -N;	// prepare for one-sided surfaces like Mobius or Klein
			vec3 texColor = texture(diffuseTexture, texcoord).rgb;
			vec3 ka = material.ka * texColor;
			vec3 kd = material.kd * texColor;

			vec3 radiance = vec3(0, 0, 0);
			for(int i = 0; i < nLights; i++) {
				vec3 L = normalize(wLight[i]);
				vec3 H = normalize(L + V);
				float cost = max(dot(N,L), 0);
				float cosd = max(dot(N,H), 0);

				radiance += ka * lights[i].La + 
                           (kd * cost + material.ks * pow(cosd, material.shininess)) * lights[i].Le;
			}
			fragmentColor = vec4(radiance, 1);
		}
	)";

	const char* geometrySource = R"(
		#version 330
		in vec4 fragmentColor;
	)";

public:
	PhongShader() { create(vertexSource, fragmentSource); }

	void Bind(RenderState state) {
		Use(); 		// make this program run
		setUniform(state.MVP, "MVP");
		setUniform(state.M, "M");
		setUniform(state.Minv, "Minv");
		setUniform(state.wEye, "wEye");

		// setUniform(*state.texture, "diffuseTexture"); helyett:
		state.texture->Bind(0);
		setUniform(0, "diffuseTexture");

		setUniformMaterial(*state.material, "material");
		setUniform((int)state.lights.size(), "nLights");
		for (unsigned int i = 0; i < state.lights.size(); i++) {
			setUniformLight(state.lights[i], std::string("lights[") + std::to_string(i) + std::string("]"));
		}
	}
};

//---------------------------
class Geomtry {
//---------------------------
protected:
	unsigned int vao, vbo;        // vertex array object
public:
	Geomtry() {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glGenBuffers(1, &vbo); // Generate 1 vertex buffer object
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
	}
	virtual void Draw() = 0;
	~Geomtry() {
		glDeleteBuffers(1, &vbo);
		glDeleteVertexArrays(1, &vao);
	}
};

//---------------------------
class ParamSurface : public Geomtry {
//---------------------------
	struct VertexData {
		vec3 position, normal;
		vec2 texcoord;
	};

	unsigned int nVtxPerStrip, nStrips;
public:
	ParamSurface() { nVtxPerStrip = nStrips = 0; }

	virtual void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) = 0;

	VertexData GenVertexData(float u, float v) {
		VertexData vtxData;
		vtxData.texcoord = vec2(u, v);
		Dnum2 X, Y, Z;
		Dnum2 U(u, vec2(1, 0)), V(v, vec2(0, 1));
		eval(U, V, X, Y, Z);
		vtxData.position = vec3(X.f, Y.f, Z.f);
		vec3 drdU(X.d.x, Y.d.x, Z.d.x), drdV(X.d.y, Y.d.y, Z.d.y);
		vtxData.normal = cross(drdU, drdV);
		return vtxData;
	}

	void create(int N = tessellationLevel, int M = tessellationLevel) {
		nVtxPerStrip = (M + 1) * 2;
		nStrips = N;
		std::vector<VertexData> vtxData;	// vertices on the CPU
		for (int i = 0; i < N; i++) {
			for (int j = 0; j <= M; j++) {
				vtxData.push_back(GenVertexData((float)j / M, (float)i / N));
				vtxData.push_back(GenVertexData((float)j / M, (float)(i + 1) / N));
			}
		}
		glBufferData(GL_ARRAY_BUFFER, nVtxPerStrip * nStrips * sizeof(VertexData), &vtxData[0], GL_STATIC_DRAW);
		// Enable the vertex attribute arrays
		glEnableVertexAttribArray(0);  // attribute array 0 = POSITION
		glEnableVertexAttribArray(1);  // attribute array 1 = NORMAL
		glEnableVertexAttribArray(2);  // attribute array 2 = TEXCOORD0
		// attribute array, components/attribute, component type, normalize?, stride, offset
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texcoord));
	}

	void Draw() {
		glBindVertexArray(vao);
		for (unsigned int i = 0; i < nStrips; i++) glDrawArrays(GL_TRIANGLE_STRIP, i * nVtxPerStrip, nVtxPerStrip);
	}
};

//---------------------------
class Sphere : public ParamSurface {
//---------------------------
public:
	Sphere() { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) {
		U = U * 2.0f * (float)M_PI, V = V * (float)M_PI;
		X = Cos(U) * Sin(V); Y = Sin(U) * Sin(V); Z = Cos(V);
	}
};

//---------------------------
class Cylinder : public ParamSurface {
//---------------------------
public:
	Cylinder() { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) {
		U = U * 2.0f * M_PI, V = V * 2 - 1.0f;
		X = Cos(U); Y = Sin(U); Z = V;
	}
};

class Plane : public ParamSurface {
public:
	Plane() { create(); }
	void eval(Dnum2& U, Dnum2& V, Dnum2& X, Dnum2& Y, Dnum2& Z) {
		U = U * 20.0f - 10.0f;
		V = V * 20.0f - 10.0f;

		X = U;
		Y = Dnum2(-1.0f);
		Z = V;
	}
};

//---------------------------
struct Object {
//---------------------------
	Shader* shader;
	Material* material;
	Texture* texture;
	Geomtry* geometry;
	vec3 scaleVec, translation, rotationAxis;
	float rotationAngle;
public:
	Object(Shader* _shader, Material* _material, Texture* _texture, Geomtry* _geometry) :
		scaleVec(vec3(1, 1, 1)), translation(vec3(0, 0, 0)), rotationAxis(0, 0, 1), rotationAngle(0) {
		shader = _shader;
		texture = _texture;
		material = _material;
		geometry = _geometry;
	}

	virtual void SetModelingTransform(mat4& M, mat4& Minv) {
		M = scale(scaleVec) * rotate(rotationAngle, rotationAxis) * translate(translation);
		Minv = translate(-translation) * rotate(-rotationAngle, rotationAxis) * scale(vec3(1 / scaleVec.x, 1 / scaleVec.y, 1 / scaleVec.z));
	}

	void Draw(RenderState state) {
		mat4 M, Minv;
		SetModelingTransform(M, Minv);
		state.M = M;
		state.Minv = Minv;
		state.MVP = state.P * state.V * state.M;
		state.material = material;
		state.texture = texture;
		shader->Bind(state);
		geometry->Draw();
	}

	virtual void Animate(float tstart, float tend) { rotationAngle = 0.8f * tend; }
};

//---------------------------
class Scene {
//---------------------------
	std::vector<Object*> objects;
	Camera camera; // 3D camera
	std::vector<Light> lights;
public:
	void Build() {
		// Shaders
		Shader* phongShader = new PhongShader();

		// Materials
		Material* material0 = new Material;
		material0->kd = vec3(0.6f, 0.4f, 0.2f);
		material0->ks = vec3(4, 4, 4);
		material0->ka = vec3(0.1f, 0.1f, 0.1f);
		material0->shininess = 100;

		Material* material1 = new Material;
		material1->kd = vec3(0.8f, 0.6f, 0.4f);
		material1->ks = vec3(0.3f, 0.3f, 0.3f);
		material1->ka = vec3(0.2f, 0.2f, 0.2f);
		material1->shininess = 30;

		Material* boardMaterial = new Material;
		boardMaterial->kd = vec3(1.0f, 1.0f, 1.0f);
		boardMaterial->ks = vec3(0.0f);
		boardMaterial->ka = vec3(0.0f);
		boardMaterial->shininess = 1.0f;

		Material* yellowPlastic = new Material;
		yellowPlastic->kd = vec3(0.3f, 0.2f, 0.1f);
		yellowPlastic->ks = vec3(2.0f, 2.0f, 2.0f);
		yellowPlastic->ka = vec3(0.1f, 0.1f, 0.1f);
		yellowPlastic->shininess = 50.0f;

		Material* waterThing = new Material;
		waterThing->kd = vec3(1.3f);
		waterThing->ks = vec3(0.0f);
		waterThing->ka = vec3(0.1f);
		waterThing->shininess = 1.0f;

		// Textures
		Texture* texture4x8 = new CheckerBoardTexture(4, 8);
		Texture* texture15x20 = new CheckerBoardTexture(15, 20);

		Texture* boardTexture = new CheckerTexture(20);
		Texture* roughTexture = new RoughTexture(64);
		Texture* solidTexture = new SolidTexture();


		// Geometries
		Geomtry* sphere = new Sphere();
		Geomtry* cylinder = new Cylinder();

		Geomtry* checkerPlane = new Plane();

		// Create objects by setting up their vertex data on the GPU
		Object* sphereObject1 = new Object(phongShader, material0, texture15x20, sphere);
		sphereObject1->translation = vec3(-9, 3, 0);
		sphereObject1->scaleVec = vec3(0.5f, 1.2f, 0.5f);
		objects.push_back(sphereObject1);

		Object* sphereObject2 = new Object(phongShader, material1, roughTexture, cylinder);
		sphereObject2->translation = vec3(-1.0f, -1.0f, 0.0f);
		sphereObject2->scaleVec = vec3(1.0f, 1.0f, 1.0f);
		sphereObject2->rotationAxis = vec3(-0.2f, 1.0f, -0.1f);
		sphereObject2->rotationAngle = 0.5f;
		objects.push_back(sphereObject2);

		Object* checkerboard = new Object(phongShader, boardMaterial, boardTexture, checkerPlane);
		checkerboard->translation = vec3(0, 0, 0);
		checkerboard->scaleVec = vec3(1.0f);
		objects.push_back(checkerboard);

		Object* yellowCylinder = new Object(phongShader, yellowPlastic, roughTexture, new Cylinder());
		yellowCylinder->scaleVec = vec3(0.3f, 0.6f, 0.1f);
		yellowCylinder->translation = vec3(-1.0f, 0.0f, 0.0f);
		yellowCylinder->rotationAxis = normalize(cross(vec3(0.0f, 0.0f, 1.0f), normalize(vec3(0.0f, 1.0f, 0.1f))));
		yellowCylinder->rotationAngle = acos(dot(vec3(0.0f, 0.0f, 1.0f), normalize(vec3(0.0f, 1.0f, 0.1f))));
		objects.push_back(yellowCylinder);

		Object* waterCylinder = new Object(phongShader, yellowPlastic, solidTexture, new Cylinder());
		waterCylinder->scaleVec = vec3(0.3f, 0.3f, 0.1f);
		waterCylinder->rotationAxis = normalize(vec3(-0.2f, 1.0f, -0.1f));
		waterCylinder->rotationAngle = 0.5f;
		waterCylinder->translation = vec3(0.0f, -1.0f, -0.8f);
		objects.push_back(waterCylinder);

		// Camera
		camera.wEye = vec3(0.0f, 1.0f, 4.0f);
		camera.wLookat = vec3(0.0f, 0.0f, -1.0f);
		camera.wVup = vec3(0.0f, 1.0f, 0.0f);

		// Lights
		lights.resize(3);
		lights[0].wLightPos = vec4(5, 5, 4, 0);	// ideal point -> directional light source
		lights[0].La = vec3(0.1f, 0.1f, 1);
		lights[0].Le = vec3(3.0f, 3.0f, 3.0f);

		lights[1].wLightPos = vec4(5, 10, 20, 0);	// ideal point -> directional light source
		lights[1].La = vec3(0.2f, 0.2f, 0.2f);
		lights[1].Le = vec3(3.0f, 3.0f, 3.0f);

		lights[2].wLightPos = vec4(-5, 5, 5, 0);	// ideal point -> directional light source
		lights[2].La = vec3(0.1f, 0.1f, 0.1f);
		lights[2].Le = vec3(3.0f, 3.0f, 3.0f);
	}

	void Render() {
		RenderState state;
		state.wEye = camera.wEye;
		state.V = camera.V();
		state.P = camera.P();
		state.lights = lights;
		for (Object* obj : objects) obj->Draw(state);
	}

	void Animate(float tstart, float tend) {
		for (Object* obj : objects) obj->Animate(tstart, tend);
	}

	void Spin() {
		camera.Spin();
	}

	void Turn(const int _dir, const float _angle = M_PI / 180.0f * 5.0f) {
		camera.Turn(_dir, _angle);
	}

	void Move(const int _dir) {
		camera.Move(_dir);
	}
};

//---------------------------
class Kepszintezis : public glApp {
//---------------------------
	Scene scene;
public:
	Kepszintezis() : glApp("Kepszintezis") {}

	// Initialization, create an OpenGL context
	void onInitialization() {
		glViewport(0, 0, windowWidth, windowHeight);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		scene.Build();
	}

	// Window has become invalid: Redraw
	void onDisplay() {
		glClearColor(0.5f, 0.5f, 0.8f, 1.0f);// background color 
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
		scene.Render();
	}

	void onKeyboard(int key) override {
		printf("Key pressed: %d\t", key);
		if (key == 'a') {
			scene.Spin();
			refreshScreen();
		}
		else if (key == 'q') {
			scene.Turn(2);
			refreshScreen();
		}
		else if (key == 'e') {
			scene.Turn(1);
			refreshScreen();
		}
		else if (key == 'w') {
			scene.Turn(4);
			refreshScreen();
		}
		else if (key == 's') {
			scene.Turn(3);
			refreshScreen();
		}
		else if (key == 'f') {
			scene.Move(1);
			refreshScreen();
		}
		else if (key == 'h') {
			scene.Move(2);
			refreshScreen();
		}
		else if (key == 't') {
			scene.Move(3);
			refreshScreen();
		}
		else if (key == 'g') {
			scene.Move(4);
			refreshScreen();
		}
	}
};

Kepszintezis app;