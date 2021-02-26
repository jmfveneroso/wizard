#ifndef __SCRIPTS_HPP__
#define __SCRIPTS_HPP__

#include <random>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "resources.hpp"

class ScriptManager {
  shared_ptr<Resources> resources_;
  std::default_random_engine generator_;
  PyObject* module_;

  void LoadScripts();
  void ProcessEvent(shared_ptr<Event> e);

 public:
  ScriptManager(shared_ptr<Resources> asset_catalog);
  ~ScriptManager();

  void ProcessScripts();
};

#endif // __SCRIPTS_HPP__
