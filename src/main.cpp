#include "renderer.hpp"
#include "text_editor.hpp"
#include "collision_resolver.hpp"
#include "ai.hpp"
#include "physics.hpp"
#include "player.hpp"
#include "inventory.hpp"
#include "engine.hpp"
#include "monsters.hpp"

shared_ptr<Resources> resources = nullptr;
shared_ptr<Renderer> renderer = nullptr;
shared_ptr<TextEditor> text_editor = nullptr;
shared_ptr<Draw2D> draw_2d = nullptr;
shared_ptr<Project4D> project_4d = nullptr;
shared_ptr<Physics> physics = nullptr;
shared_ptr<CollisionResolver> collision_resolver = nullptr;
shared_ptr<AI> ai = nullptr;
shared_ptr<Inventory> inventory = nullptr;
shared_ptr<PlayerInput> player_input = nullptr;
shared_ptr<Engine> engine = nullptr;
shared_ptr<Monsters> monsters = nullptr;

void PressCharCallback(GLFWwindow* window, unsigned int char_code) {
  text_editor->PressCharCallback(string(1, (char) char_code));
}

void PressKeyCallback(GLFWwindow* window, int key, int scancode, int action, 
  int mods) {
  text_editor->PressKeyCallback(key, scancode, action, mods);
}

int window_width_ = WINDOW_WIDTH;
int window_height_ = WINDOW_HEIGHT;
GLFWwindow* window_;

void InitOpenGl() {
  if (!glfwInit()) {
    throw runtime_error("Failed to initialize GLFW");
  }

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // To make MacOS happy; should not be needed.
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 

  // Fullscreen.
  window_ = glfwCreateWindow(window_width_, window_height_, APP_NAME, 
    glfwGetPrimaryMonitor(), NULL);

  // Windowed.
  // window_ = glfwCreateWindow(window_width_, window_height_, APP_NAME, 
  //   NULL, NULL);

  if (window_ == NULL) {
    glfwTerminate();
    throw runtime_error("Failed to open GLFW window");
  }

  // We would expect width and height to be 1024 and 768
  // But on MacOS X with a retina screen it'll be 1024*2 and 768*2, 
  // so we get the actual framebuffer size:
  glfwGetFramebufferSize(window_, &window_width_, &window_height_);
  glfwMakeContextCurrent(window_);

  // Needed for core profile.
  glewExperimental = true; 
  if (glewInit() != GLEW_OK) {
    glfwTerminate();
    throw runtime_error("Failed to initialize GLEW");
  }

  // Hide the mouse and enable unlimited movement.
  glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwPollEvents();
  // glfwSetCursorPos(window_, 0, 0);
}

int main() {
  InitOpenGl();

  const string resources_dir = "resources";
  const string shaders_dir = "shaders";

  resources = make_shared<Resources>(resources_dir, shaders_dir, window_);
  draw_2d = make_shared<Draw2D>(resources, resources_dir, 1440, 900);
  project_4d = make_shared<Project4D>(resources);
  inventory = make_shared<Inventory>(resources, draw_2d);
  renderer = make_shared<Renderer>(resources, draw_2d, project_4d,
    inventory, window_, window_width_, window_height_);
  physics = make_shared<Physics>(resources);
  collision_resolver = make_shared<CollisionResolver>(resources);
  monsters = make_shared<Monsters>(resources);
  ai = make_shared<AI>(resources, monsters);
  text_editor = make_shared<TextEditor>(draw_2d);
  player_input = make_shared<PlayerInput>(resources, project_4d, 
    inventory, renderer->terrain());
  engine = make_shared<Engine>(project_4d, renderer, text_editor, inventory, 
    resources, collision_resolver, ai, physics, player_input,
    window_, window_width_, window_height_);

  glfwSetCharCallback(renderer->window(), PressCharCallback);
  glfwSetKeyCallback(renderer->window(), PressKeyCallback);

  engine->Run();
  return 0;
}
