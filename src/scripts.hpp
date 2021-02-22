#ifndef __SCRIPTS_HPP__
#define __SCRIPTS_HPP__

#include <random>
#include "resources.hpp"

struct ScriptInputs {
  void set(string msg) { this->msg = msg; }
  string greet()       { return msg;      }
  string msg = "xxxyyyzzz";
};

class ScriptManager {
  shared_ptr<Resources> resources_;
  std::default_random_engine generator_;

  void ProcessScript(const string& script);

 public:
  ScriptManager(shared_ptr<Resources> asset_catalog);
  ~ScriptManager();

  void ProcessScripts();
};

#endif // __SCRIPTS_HPP__
