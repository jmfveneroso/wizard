#include "scripts.hpp"
#define PY_SSIZE_T_CLEAN
#include <Python.h>

shared_ptr<Resources> gResources = nullptr;

// Embedding Python: https://docs.python.org/3/extending/embedding.html

static PyObject* is_player_inside_region(PyObject *self, PyObject *args) {
  char* c_ptr; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "s", &c_ptr)) {
    return NULL;
  }

  if (!c_ptr) {
    return NULL;
  }

  string buffer = reinterpret_cast<char*>(c_ptr);

  if (!gResources) {
    return NULL;
    // return PyLong_FromLong(numargs);
  }

  return PyBool_FromLong(gResources->IsPlayerInsideRegion(buffer));
  // return PyUnicode_FromString("this is a test");
}

static PyObject* issue_move_order(PyObject *self, PyObject *args) {
  char* c_ptr1;
  char* c_ptr2; 
  if (!PyArg_ParseTuple(args, "ss", &c_ptr1, &c_ptr2)) {
    return NULL;
  }

  if (!c_ptr1 || !c_ptr2) {
    return NULL;
  }

  string buffer1 = reinterpret_cast<char*>(c_ptr1);
  string buffer2 = reinterpret_cast<char*>(c_ptr2);

  if (!gResources) {
    return NULL;
  }

  gResources->IssueMoveOrder(buffer1, buffer2);
  return PyBool_FromLong(1);
}

static PyMethodDef EmbMethods[] = {
 { "is_player_inside_region", is_player_inside_region, METH_VARARGS,
  "Tests if player is inside region" },
 { "issue_move_order", issue_move_order, METH_VARARGS,
  "Issue move order for unit to move to waypoint" },
 { NULL, NULL, 0, NULL }
};

static PyModuleDef EmbModule = {
  PyModuleDef_HEAD_INIT, "wizard", NULL, -1, EmbMethods,
  NULL, NULL, NULL, NULL
};

static PyObject* PyInit_emb(void) {
  return PyModule_Create(&EmbModule);
}




ScriptManager::ScriptManager(shared_ptr<Resources> resources) 
  : resources_(resources) {
  gResources = resources_;
  PyImport_AppendInittab("wizard", &PyInit_emb);
  Py_Initialize();
}

ScriptManager::~ScriptManager() {
  if (Py_FinalizeEx() < 0) {
    cout << "Error in Py_FinalizeEx." << endl;
  }
}

void ScriptManager::ProcessScript(const string& script) {
  PyRun_SimpleString(script.c_str());
}

void ScriptManager::ProcessScripts() {
  for (const auto& [name, script] : resources_->GetScripts()) {
    ProcessScript(script);
  }
}

