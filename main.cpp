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

std::unordered_map<Scene::Object*,bool> canJump;
std::unordered_map<Scene::Object*,bool> isJumping;

//with plane
bool checkCollisionWithPlane(Scene::Object* obj1, Scene::Object* obj2){
    if(canJump[obj1]==false){
        if (obj1->transform.position.z - obj1->transform.scale.z<=obj2->transform.position.z) {
            obj1->velocity = glm::vec3(0.0f);
            canJump[obj1] = true;
            return true;
        }
    }
    return false;
}


//cube sphere
inline float squared(float v) { return v * v; }
bool cubeSphereCollision(Scene::Object* cube, Scene::Object* sphere)
{
    if ((sphere->transform.position.z - sphere->transform.scale.z  < cube->transform.position.z + 0.4f) &&
        (sphere->transform.position.z  > cube->transform.position.z - 0.4f) &&
        (cube->transform.position.y+cube->transform.scale.y > sphere->transform.position.y) &&
        (cube->transform.position.y < sphere->transform.position.y + sphere->transform.scale.z)) {
        
        glm::vec3 N = glm::normalize(sphere->transform.position - cube->transform.position);
        glm::vec3 new_vel = sphere->velocity - 2.0f * glm::dot(sphere->velocity, N) * N;
        
        //Like slime volleyball
        if(cube->velocity.z==0){
            sphere->velocity = new_vel;
        }
        else{
            sphere->velocity =  cube->velocity * 1.75f;
        }
        
        return true;
    }
    return false;
}

//Cubes
//AABB Collision
bool checkCollisionCubes(Scene::Object* obj1, Scene::Object* obj2){
    if(std::abs(obj1->transform.position.y - obj2->transform.position.y) < obj1->transform.scale.y + obj2->transform.scale.y)
    {
        if(std::abs(obj1->transform.position.z - obj2->transform.position.z) < obj1->transform.scale.z + obj2->transform.scale.z)
        {
            obj1->velocity = glm::vec3(0.0f);
            canJump[obj1] = true;
            return true;
        }
    }
    canJump[obj1] = false;
    return false;
}

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game2: Scene";
		glm::uvec2 size = glm::uvec2(640, 480);
	} config;
    
    
    bool isLeftPressed = false;
    bool isRightPressed = false;
    
    bool isAPressed = false;
    bool isDPressed = false;

    
    Scene::Object* Player1;
    Scene::Object* Player2;
    
    Scene::Object* Net;
    Scene::Object* Ball;
    
    Scene::Object* Plane;
    
    
    
    glm::vec3 const GRAVITY = glm::vec3(0.0f,0.0f,-9.8f);
    
//    glm::vec3 P2_VEL = glm::vec3(0.0f,0.0f,0.0f);
    const float GRAVITY_SCALE = 2.0f;

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


            
            Player1 = &add_object("p1",  glm::vec3(0.0f, -10.0f, -6.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f));
            canJump[Player1] = false;
            isJumping[Player1] = false;
            
            Player2 = &add_object("p2",  glm::vec3(0.0f, 10.0f, -6.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f));
            canJump[Player2] = false;
            isJumping[Player2] = false;
            
            Net = &add_object("net",  glm::vec3(0.0f, 0.0f, -5.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(10.0f,0.05f,3.0f));
            
            Ball = &add_object("ball",  glm::vec3(0.0f, 10.0f, 5.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(1.0f));
            Ball->velocity = glm::vec3(0.0f,0.0f,-1.0f);
            
            Plane = &add_object("floot",  glm::vec3(0.0f, 0.0f, -7.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(15.0f));
//            Balloon2 = &add_object("p2",  glm::vec3(-0.5f, -2.0f, 1.0f), glm::quat(0.0f, 0.0f, 0.0f, 1.0f), glm::vec3(0.5f));
		}
	}

    


//	glm::vec2 mouse = glm::vec2(0.0f, 0.0f); //mouse position in [-1,1]x[-1,1] coordinates

	struct {
		float radius = 20.0f;
		float elevation = 0.166667f;
		float azimuth =0.0625f;
		glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
	} camera;

	//------------ game loop ------------

	bool should_quit = false;
	while (true) {
		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
            if (evt.type == SDL_KEYDOWN) {
                
                switch(evt.key.keysym.sym){
                    case SDLK_w:
                        if(canJump[Player1]){
                            isJumping[Player1] = true;
                        }
                        break;
                    case SDLK_UP:
                        if(canJump[Player2]){
                            isJumping[Player2] = true;
                        }
                        break;
                    case SDLK_LEFT:
                        isLeftPressed = true;
                        break;
                    case SDLK_a:
                        isAPressed = true;
                        break;
                    case SDLK_d:
                        isDPressed = true;
                        break;
                    case SDLK_RIGHT:
                        isRightPressed = true;
                        break;
                    default:
                        break;
                }
                
            }
            else if (evt.type == SDL_KEYUP) {
                switch(evt.key.keysym.sym){
                    case SDLK_w:
                        isJumping[Player1] = false;
                        break;
                    case SDLK_UP:
                        isJumping[Player2] = false;
                        break;
                    case SDLK_LEFT:
                        isLeftPressed = false;
                        break;
                    case SDLK_a:
                        isAPressed = false;
                        break;
                    case SDLK_d:
                        isDPressed = false;
                        break;
                    case SDLK_RIGHT:
                        isRightPressed = false;
                        break;
                    default:
                        break;
                }
                
            }
			else if (evt.type == SDL_MOUSEMOTION) {
//				glm::vec2 old_mouse = mouse;
//				mouse.x = (evt.motion.x + 0.5f) / float(config.size.x) * 2.0f - 1.0f;
//				mouse.y = (evt.motion.y + 0.5f) / float(config.size.y) *-2.0f + 1.0f;
//				if (evt.motion.state & SDL_BUTTON(SDL_BUTTON_LEFT)) {
//					camera.elevation += -2.0f * (mouse.y - old_mouse.y);
//					camera.azimuth += -2.0f * (mouse.x - old_mouse.x);
//                    
//				}
			}
            else if (evt.type == SDL_MOUSEBUTTONUP) {
                
//                std::cout << camera.elevation << " " << camera.azimuth;
                
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
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;
        
        

		{ //update game state:
            
            
//            std::cout<< checkCollisionCubes(Player1,Player2) << "\n";
            
            bool player1Collision = checkCollisionCubes(Player1,Net);
            bool player2Collision = checkCollisionCubes(Player2,Net);
            
            checkCollisionWithPlane(Player1,Plane);
            checkCollisionWithPlane(Player2,Plane);
            
            cubeSphereCollision(Player1,Ball);
            cubeSphereCollision(Player2,Ball);
            
            
            Ball->velocity +=  GRAVITY*GRAVITY_SCALE * elapsed;
            Ball->transform.position +=  Ball->velocity * elapsed;
            
            if(Ball->transform.position.z<-15.0f){
                Ball->transform.position =  glm::vec3(0.0f, 10.0f, 5.0f);
                Ball->velocity = glm::vec3(0.0f,0.0f,-1.0f);
            }

            
            //player1
            if(canJump[Player1]==false){
                Player1->velocity +=  GRAVITY*GRAVITY_SCALE * elapsed;
            }
            if(isJumping[Player1] ==true){
                Player1->velocity  = glm::vec3(0.0f,0.0f,12.0f);
                isJumping[Player1] = false;
                canJump[Player1]= false;
            }
            if(isAPressed){
                if(!player1Collision)
                    Player1->velocity.y = -7.0f;
                else
                    Player1->transform.position.y += 7.0f;
            }
            else if(isDPressed){
                if(!player1Collision)
                    Player1->velocity.y = 7.0f;
                else
                    Player1->transform.position.y -= 7.0f;
            }
            else{
                Player1->velocity.y = 0;
            }
            Player1->transform.position +=  Player1->velocity * elapsed;
            
            //player2
            if(canJump[Player2]==false){
                Player2->velocity +=  GRAVITY*GRAVITY_SCALE * elapsed;
            }
            if(isJumping[Player2] ==true){
                Player2->velocity  = glm::vec3(0.0f,0.0f,12.0f);
                isJumping[Player2] = false;
                canJump[Player2]= false;
            }
            if(isLeftPressed){
                if(!player2Collision)
                    Player2->velocity.y = -5.0f;
                else
                    Player2->transform.position.y += 5.0f;
                
            }
            else if(isRightPressed){
                if(!player2Collision)
                    Player2->velocity.y = 5.0f;
                else
                    Player2->transform.position.y = -5.0f;
                
                player2Collision = false;
            }
            else{
                Player2->velocity.y = 0;
            }
            
//            {
//                isLeftPressed = false;
//                isRightPressed = false;
//            }

            Player2->transform.position +=  Player2->velocity * elapsed;
            
            // Move objects x/y position based off it's velocity vector
//            Player1->transform.position.x += (gameObject.velocity.x * elapsedSeconds);
//            Player1->transform.position.y += (gameObject.velocity.y * elapsedSeconds);
            
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
