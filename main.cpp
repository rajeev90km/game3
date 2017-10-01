#include "load_save_png.hpp"
#include "GL.hpp"
#include "Meshes.hpp"
#include "Scene.hpp"
#include "read_chunk.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <unordered_map>

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game2: Scene";
		glm::uvec2 size = glm::uvec2(640, 480);
	} config;
    
    
    bool isZPressed = false;
    bool isXPressed = false;
    bool isAPressed = false;
    bool isSPressed = false;
    bool isCommaPressed = false;
    bool isColnPressed = false;
    bool isPeriodPressed = false;
    bool isSlashPressed = false;
    
    Scene::Object* Balloon1;
    Scene::Object* Balloon2;
    Scene::Object* Balloon3;

	//------------  initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	//SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_Normal = 0;
	GLuint program_mvp = 0;
	GLuint program_itmv = 0;
	GLuint program_to_light = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"uniform mat3 itmv;\n"
			"in vec4 Position;\n"
			"in vec3 Normal;\n"
			"out vec3 normal;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	normal = itmv * Normal;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform vec3 to_light;\n"
			"in vec3 normal;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	float light = max(0.0, dot(normalize(normal), to_light));\n"
			"	fragColor = vec4(light * vec3(1.0, 1.0, 1.0), 1.0);\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
		program_Normal = glGetAttribLocation(program, "Normal");
		if (program_Normal == -1U) throw std::runtime_error("no attribute named Normal");

		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
		program_itmv = glGetUniformLocation(program, "itmv");
		if (program_itmv == -1U) throw std::runtime_error("no uniform named itmv");

		program_to_light = glGetUniformLocation(program, "to_light");
		if (program_to_light == -1U) throw std::runtime_error("no uniform named to_light");
	}

	//------------ meshes ------------

	Meshes meshes;

	{ //add meshes to database:
		Meshes::Attributes attributes;
		attributes.Position = program_Position;
		attributes.Normal = program_Normal;

		meshes.load("meshes.blob", attributes);
	}
	
	//------------ scene ------------

	Scene scene;
	//set up camera parameters based on window:
	scene.camera.fovy = glm::radians(60.0f);
	scene.camera.aspect = float(config.size.x) / float(config.size.y);
	scene.camera.near = 0.01f;
	//(transform will be handled in the update function below)

	//add some objects from the mesh library:
	auto add_object = [&](std::string const &name, glm::vec3 const &position, glm::quat const &rotation, glm::vec3 const &scale) -> Scene::Object & {
		Mesh const &mesh = meshes.get(name);
		scene.objects.emplace_back();
		Scene::Object &object = scene.objects.back();
		object.transform.position = position;
		object.transform.rotation = rotation;
		object.transform.scale = scale;
		object.vao = mesh.vao;
		object.start = mesh.start;
		object.count = mesh.count;
		object.program = program;
		object.program_mvp = program_mvp;
		object.program_itmv = program_itmv;
		return object;
	};
    
	std::vector< Scene::Object * > craneObj;

	{ //read objects to add from "scene.blob":
		std::ifstream file("scene.blob", std::ios::binary);

		std::vector< char > strings;
		//read strings chunk:
		read_chunk(file, "str0", &strings);

		{ //read scene chunk, add meshes to scene:
			struct SceneEntry {
				uint32_t name_begin, name_end;
				glm::vec3 position;
				glm::quat rotation;
				glm::vec3 scale;
			};
			static_assert(sizeof(SceneEntry) == 48, "Scene entry should be packed");

			std::vector< SceneEntry > data;
			read_chunk(file, "scn0", &data);


            add_object("Plane",  glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f));
            add_object("Stand",  glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f));
            Balloon1 = &add_object("Balloon1.001",  glm::vec3(0.9f, 2.2f, 1.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f));
            Balloon2 = &add_object("Balloon1.001",  glm::vec3(-0.5f, -2.0f, 1.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.5f));
            Balloon3 = &add_object("Balloon1.001",  glm::vec3(-2.5f, 0.0f, 1.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.5f));
            craneObj.emplace_back(&add_object("Base",  glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f)));
            craneObj.emplace_back(&add_object("Link1",  glm::vec3(0.0f, 0.0f, 0.3f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f)));
            craneObj.emplace_back(&add_object("Link2",  glm::vec3(0.0f, 0.0f, 1.2f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f)));
            craneObj.emplace_back(&add_object("Link3",  glm::vec3(0.0f, 0.0f, 1.2f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f)));
		}
	}

    
    //Parent Crane Entities
    for (uint32_t i = 1; i < craneObj.size(); ++i) {
		craneObj[i]->transform.set_parent(&craneObj[i-1]->transform);
	}


	std::vector< float > wave_acc(craneObj.size(), 0.0f);

	glm::vec2 mouse = glm::vec2(0.0f, 0.0f); //mouse position in [-1,1]x[-1,1] coordinates

	struct {
		float radius = 7.5f;
		float elevation = -3.75834f;
		float azimuth =-6.13125f;
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;

	//------------ game loop ------------

	bool should_quit = false;
	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
            if (evt.type == SDL_KEYDOWN) {
                
                switch(evt.key.keysym.sym){
                    case SDLK_z:
                        isZPressed = true;
                        break;
                    case SDLK_x:
                        isXPressed = true;
                        break;
                    case SDLK_a:
                        isAPressed = true;
                        break;
                    case SDLK_s:
                        isSPressed = true;
                        break;
                    case SDLK_SEMICOLON:
                        isCommaPressed = true;
                        break;
                    case SDLK_QUOTE:
                        isColnPressed = true;
                        break;
                    case SDLK_PERIOD:
                        isPeriodPressed = true;
                        break;
                    case SDLK_SLASH:
                        isSlashPressed = true;
                        break;
                    default:
                        break;
                }
                
            }
            else if (evt.type == SDL_KEYUP) {
                switch(evt.key.keysym.sym){
                    case SDLK_z:
                        isZPressed = false;
                        break;
                    case SDLK_x:
                        isXPressed = false;
                        break;
                    case SDLK_a:
                        isAPressed = false;
                        break;
                    case SDLK_s:
                        isSPressed = false;
                        break;
                    case SDLK_SEMICOLON:
                        isCommaPressed = false;
                        break;
                    case SDLK_QUOTE:
                        isColnPressed = false;
                        break;
                    case SDLK_PERIOD:
                        isPeriodPressed = false;
                        break;
                    case SDLK_SLASH:
                        isSlashPressed = false;
                        break;
                    default:
                        break;
                }
                
            }
			else if (evt.type == SDL_MOUSEMOTION) {
				glm::vec2 old_mouse = mouse;
				mouse.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
				mouse.y = (evt.motion.y + 0.5f) / float(config.size.y) *-2.0f + 1.0f;
				if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
					camera.elevation += -2.0f * (mouse.y - old_mouse.y);
					camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
                    
				}
			}
            else if (evt.type == SDL_MOUSEBUTTONUP) {
                
                std::cout << camera.elevation << " " << camera.azimuth;
                
            }
            else if (evt.type == SDL_MOUSEBUTTONDOWN) {
			} else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE) {
				should_quit = true;
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;

		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
//		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;
        
        float speed = 0.01f;

		{ //update game state:
            static float axes1 = 0.0f;
            static float axes2 = 0.0f;
            static float axes3 = 0.0f;
            static float axes4 = 0.0f;
            
            if(isZPressed)
                axes1+=speed;
            if(isXPressed)
                axes1-=speed;
            if(isAPressed){
                if(axes2>-0.29f)
                    axes2-=speed;
            }
            if(isSPressed){
                if(axes2<0.29f)
                    axes2+=speed;
            }
            if(isCommaPressed){
                if(axes3>-0.39f)
                    axes3-=speed;
            }
            if(isColnPressed){
                if(axes3<0.39f)
                    axes3+=speed;
            }
            if(isPeriodPressed){
                if(axes4>-0.39f)
                    axes4-=speed;
            }
            if(isSlashPressed){
                if(axes4<0.39f)
                    axes4+=speed;
            }
            
            
            
			
            craneObj[0]->transform.rotation = glm::angleAxis(
                                                             axes1 * 2.0f * float(M_PI) * (0.8f),
                                                             glm::vec3(0.0f,0.0f,1.0f)
                                                             );

            craneObj[1]->transform.rotation = glm::angleAxis(
                                                             axes2 * 2.0f * float(M_PI) * (0.8f),
                                                             glm::vec3(1.0f,0.0f,0.0f)
                                                             );

            craneObj[2]->transform.rotation = glm::angleAxis(
                                                             axes3 * 2.0f * float(M_PI) * (0.8f),
                                                             glm::vec3(1.0f,0.0f,0.0f)
                                                             );
            
            craneObj[3]->transform.rotation = glm::angleAxis(
                                                             axes4 * 2.0f * float(M_PI) * (0.8f),
                                                             glm::vec3(1.0f,0.0f,0.0f)
                                                             );
            
            
            static float ctr = 0.0f;
            ctr+=speed;
            //Balloon Up Down
            Balloon1->transform.position.z +=  0.03f *std::sin(ctr*2);
            Balloon2->transform.position.z +=  0.04f *std::sin(ctr*3);
            Balloon3->transform.position.z +=  0.07f *std::sin(ctr*5);
            
            glm::mat4 tr = craneObj[3]->transform.make_world_to_local();
            
            glm::vec3 localPostion;
            localPostion.x =tr[3][0];
            localPostion.y =tr[3][1];
            localPostion.z =tr[3][2];
            
          
//            std::cout<<localPostion.x << " " << localPostion.y << " " << localPostion.z << "\n";
//            
            std::cout<< glm::distance(Balloon1->transform.position,localPostion) << "\n";
            
            
            //camera:
			scene.camera.transform.position = camera.radius * glm::vec3(
				std::cos(camera.elevation) * std::cos(camera.azimuth),
				std::cos(camera.elevation) * std::sin(camera.azimuth),
				std::sin(camera.elevation)) + camera.target;

			glm::vec3 out = -glm::normalize(camera.target - scene.camera.transform.position);
			glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
			up = glm::normalize(up - glm::dot(up, out) * out);
			glm::vec3 right = glm::cross(up, out);
			
			scene.camera.transform.rotation = glm::quat_cast(
				glm::mat3(right, up, out)
			);
			scene.camera.transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
		}

		//draw output:
		glClearColor(0.5, 0.5, 0.5, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		{ //draw game state:
			glUseProgram(program);
			glUniform3fv(program_to_light, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 1.0f, 10.0f))));
			scene.render();
		}


		SDL_GL_SwapWindow(window);
	}


	//------------  teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}



static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
