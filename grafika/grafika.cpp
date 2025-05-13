#include "framework.h"

template<class T> struct Dnum { // Dual numbers for automatic derivation
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
const int windowWidth = 600, windowHeight = 600; //window size

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

struct Camera { // 3D camera
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
		switch (_dir) {
		case 1: // balra
			wEye = wEye + vec3(0.0f, 0.0f, 0.5f);
			wLookat = wLookat + vec3(0.0f, 0.0f, 0.5f);
			break;
		case 2: // jobbra
			wEye = wEye + vec3(0.0f, 0.0f, -0.5f);
			wLookat = wLookat + vec3(0.0f, 0.0f, -0.5f);
			break;
		case 3: // elore
			wEye = wEye + vec3(0.5f, 0.0f, 0.0f);
			wLookat = wLookat + vec3(0.5f, 0.0f, 0.0f);
			break;
		case 4: // hatra
			wEye = wEye + vec3(-0.5f, 0.0f, 0.0f);
			wLookat = wLookat + vec3(-0.5f, 0.0f, 0.0f);
			break;
		}
	}
};

class Material {
public:
	vec3 kd, ks, ka;
	float shininess;

	Material() {}

	Material(vec3 _kd, vec3 _ks, vec3 _ka, float _shiniess) {
		kd = _kd;
		ks = _ks;
		ka = _ka;
		shininess = _shiniess;
	}
};

struct Light {
	vec3 La, Le;
	vec4 wLightPos; // homogeneous coordinates, can be at ideal point
};

class CheckerTexture : public Texture {
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

class SolidTexture : public Texture {
public:
	SolidTexture() : Texture(1, 1) {
		std::vector<vec4> image(1, vec4(1.0f, 1.0f, 1.0f, 1.0f));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, &image[0]);
	}
};

struct RenderState {
	mat4	           MVP, M, Minv, V, P;
	Material* material;
	std::vector<Light> lights;
	Texture* texture;
	vec3	           wEye;
};

class Shader : public GPUProgram {
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

class PhongShader : public Shader {
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

struct VertexData {
	vec3 position, normal;
	vec2 texcoord;
};

class Object3d {
protected:
	GLuint vao = 0, vbo = 0;
	int vertexCount = 0;
	std::vector<VertexData> vertices;
public:
	Object3d() {
		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
	}

	const std::vector<VertexData>& getVertices() const { return vertices; }

	virtual ~Object3d() {
		if (vbo) glDeleteBuffers(1, &vbo);
		if (vao) glDeleteVertexArrays(1, &vao);
	}

	void uploadVertexData(const std::vector<VertexData>& vertices) {
		vertexCount = vertices.size();
		this->vertices = vertices;
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(VertexData), vertices.data(), GL_STATIC_DRAW);

		glEnableVertexAttribArray(0); // position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));

		glEnableVertexAttribArray(1); // normal
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));

		glEnableVertexAttribArray(2); // texcoord
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texcoord));
	}

	virtual void Draw() {
		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	}
};

class Scene {
	std::vector<Object3d*> objects;
	Camera camera; // 3D camera
	std::vector<Light> lights;
public:
	void Build() {
		// Shaders
		Shader* phongShader = new PhongShader();

		Material* boardMaterial = new Material(vec3(1.0f, 1.0f, 1.0f), vec3(0.0f), vec3(0.0f), 1.0f);

		Material* yellowPlastic = new Material(vec3(0.3f, 0.2f, 0.1f), vec3(2.0f, 2.0f, 2.0f), vec3(0.1f), 50.0f);

		Material* cyanPlastic = new Material(vec3(0.1f, 0.2f, 0.3f), vec3(2.0f, 2.0f, 2.0f), vec3(0.1f, 0.1f, 0.1f), 100.0f);

		Material* magentaPlastic = new Material(vec3(0.3f, 0.0f, 0.2f), vec3(2.0f, 2.0f, 2.0f), vec3(0.1f, 0.1f, 0.1f), 20.0f);

		Material* waterThing = new Material(vec3(1.3f, 1.3f, 1.3f), vec3(0.0f), vec3(0.1f, 0.1f, 0.1f), 1.0f);

		Texture* boardTexture = new CheckerTexture(20);

		// Create objects by setting up their vertex data on the GPU


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
		for (Object3d* obj : objects) obj->Draw();
	}

	/** deprecated
	void Animate(float tstart, float tend) {
		for (Object3d* obj : objects) obj->Animate(tstart, tend);
	}
	*/

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

class Kepszintezis : public glApp {
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
		else if (key == 'g') {
			scene.Move(1);
			refreshScreen();
		}
		else if (key == 't') {
			scene.Move(2);
			refreshScreen();
		}
		else if (key == 'h') {
			scene.Move(3);
			refreshScreen();
		}
		else if (key == 'f') {
			scene.Move(4);
			refreshScreen();
		}
	}
};

Kepszintezis app;