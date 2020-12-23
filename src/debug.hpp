#ifndef __DEBUG_HPP__
#define __DEBUG_HPP__

#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <queue>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <exception>
#include <tuple>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace std;
using namespace glm;

void DoInOrder() {}

template<typename Lambda0, typename ...Lambdas>
void DoInOrder(Lambda0&& lambda0, Lambdas&&... lambdas) {
  forward<Lambda0>(lambda0)();
  DoInOrder(std::forward<Lambdas>(lambdas)...);
}

template<typename First, typename ...Args>
void ThrowError(First first, Args&& ...args) {
  std::stringstream ss;
  ss << first;
  DoInOrder( [&](){
    ss << std::forward<Args>(args);
  }...);
  throw runtime_error(ss.str());
}

#endif // __DEBUG_HPP__
