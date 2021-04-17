#ifndef __SCRIPTS_HPP__
#define __SCRIPTS_HPP__

#include <random>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "util.hpp"

class Resources;

class ScriptManager {
  Resources* resources_;
  std::default_random_engine generator_;
  PyObject* module_;

  void LoadScripts();
  void ProcessEvent(shared_ptr<Event> e);

 public:
  ScriptManager(Resources* resources);
  ~ScriptManager();

  void ProcessScripts();
  string CallStrFn(const string& fn_name);
  string CallStrFn(const string& fn_name, const string& arg);
};

#endif // __SCRIPTS_HPP__
