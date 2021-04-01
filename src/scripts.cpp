#include "scripts.hpp"
#include "resources.hpp"

EventType StrToEventType(const std::string& s) {
  static unordered_map<string, EventType> str_to_event_type ({
    { "on_interact_with_sector", EVENT_ON_INTERACT_WITH_SECTOR },
  });

  if (str_to_event_type.find(s) == str_to_event_type.end()) {
    return EVENT_NONE;
  }
  return str_to_event_type[s];
}

Resources* gResources = nullptr;

// Embedding Python: https://docs.python.org/3/extending/embedding.html

static PyObject* get_game_flag(PyObject *self, PyObject *args) {
  char* c_ptr; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "s", &c_ptr)) return NULL;
  if (!c_ptr) return NULL;

  string buffer = reinterpret_cast<char*>(c_ptr);
  int game_flag = gResources->GetGameFlag(buffer);
  return PyBool_FromLong(game_flag);
}

static PyObject* set_game_flag(PyObject *self, PyObject *args) {
  char* c_ptr1; 
  char* c_ptr2; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "ss", &c_ptr1, &c_ptr2)) return NULL;
  if (!c_ptr1 || !c_ptr2) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  gResources->SetGameFlag(s1, boost::lexical_cast<int>(s2));
  return PyBool_FromLong(0);
}

// Change to move or set position if too far.
static PyObject* set_position(PyObject *self, PyObject *args) {
  char* c_ptr1; 
  char* c_ptr2; 
  char* c_ptr3; 
  char* c_ptr4; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "ssss", &c_ptr1, &c_ptr2, &c_ptr3, &c_ptr4)) return NULL;
  if (!c_ptr1 || !c_ptr2 || !c_ptr3 || !c_ptr4) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  string s3 = reinterpret_cast<char*>(c_ptr3);
  string s4 = reinterpret_cast<char*>(c_ptr4);
  float x = boost::lexical_cast<float>(s2);
  float y = boost::lexical_cast<float>(s3);
  float z = boost::lexical_cast<float>(s4);

  ObjPtr obj = gResources->GetObjectByName(s1);
  if (!obj) {
    return PyBool_FromLong(0);
  }

  vec3 new_pos = vec3(x, y, z);
  if (length(obj->position - new_pos) > 50.0f) {
    cout << "Changing pos: " << obj->position << endl;
    obj->ChangePosition(new_pos);
    cout << "After: " << obj->position << endl;
  }

  return PyBool_FromLong(0);
}

static PyObject* print_message(PyObject *self, PyObject *args) {
  char* c_ptr; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "s", &c_ptr)) return NULL;
  if (!c_ptr) return NULL;

  string buffer = reinterpret_cast<char*>(c_ptr);
  cout << "PYTHON MSG: " << buffer << endl;
  return PyBool_FromLong(0);
}

static PyObject* register_onenter_event(PyObject *self, PyObject *args) {
  char* c_ptr1; 
  char* c_ptr2; 
  char* c_ptr3; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "sss", &c_ptr1, &c_ptr2, &c_ptr3)) return NULL;

  if (!c_ptr1 || !c_ptr2 || !c_ptr3) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  string s3 = reinterpret_cast<char*>(c_ptr3);
  if (!gResources) {
    return NULL;
  }

  gResources->RegisterOnEnterEvent(s1, s2, s3);
  return PyBool_FromLong(0);
}

static PyObject* register_onleave_event(PyObject *self, PyObject *args) {
  char* c_ptr1; 
  char* c_ptr2; 
  char* c_ptr3; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "sss", &c_ptr1, &c_ptr2, &c_ptr3)) return NULL;

  if (!c_ptr1 || !c_ptr2 || !c_ptr3) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  string s3 = reinterpret_cast<char*>(c_ptr3);
  if (!gResources) {
    return NULL;
  }

  gResources->RegisterOnLeaveEvent(s1, s2, s3);
  return PyBool_FromLong(0);
}

static PyObject* register_onopen_event(PyObject *self, PyObject *args) {
  cout << "<<<<<<<<<<<< ON OPEN" << endl;
  char* c_ptr1; 
  char* c_ptr2; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "ss", &c_ptr1, &c_ptr2)) return NULL;

  if (!c_ptr1 || !c_ptr2) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  if (!gResources) {
    return NULL;
  }

  gResources->RegisterOnOpenEvent(s1, s2);
  return PyBool_FromLong(0);
}

static PyObject* register_on_finish_dialog_event(PyObject *self, PyObject *args) {
  cout << "<<<<<<<<<<<<<<<<<< CAll" << endl;
  char* c_ptr1; 
  char* c_ptr2; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "ss", &c_ptr1, &c_ptr2)) return NULL;

  cout << "xxx" << endl;

  if (!c_ptr1 || !c_ptr2) return NULL;

  cout << "yyy" << endl;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  if (!gResources) {
    return NULL;
  }

  cout << "zzz" << endl;

  gResources->RegisterOnFinishDialogEvent(s1, s2);
  return PyBool_FromLong(0);
}

static PyObject* push_action(PyObject *self, PyObject *args) {
  char* c_ptr1; 
  char* c_ptr2; 
  char* c_ptr3; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "sss", &c_ptr1, &c_ptr2, &c_ptr3)) return NULL;

  if (!c_ptr1 || !c_ptr2 || !c_ptr3) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  string s3 = reinterpret_cast<char*>(c_ptr3);
  if (!gResources) {
    return NULL;
  }

  ObjPtr obj = gResources->GetObjectByName(s1);
  if (!obj) {
    return PyBool_FromLong(0);
  }

  if (s2 == "move_to_unit") {
    ObjPtr target_obj = gResources->GetObjectByName(s3);
    if (!target_obj) {
      return PyBool_FromLong(0);
    }
    obj->actions.push(make_shared<MoveAction>(target_obj->position));
  } else if (s2 == "talk_to") {
    ObjPtr target_obj = gResources->GetObjectByName(s3);
    if (!target_obj) {
      return PyBool_FromLong(0);
    }
    obj->actions.push(make_shared<TalkAction>(target_obj->name));
  } else if (s2 == "wait") {
    float current_time = glfwGetTime();
    float seconds = boost::lexical_cast<float>(s3);
    obj->actions.push(make_shared<WaitAction>(current_time + seconds));
  }

  return PyBool_FromLong(1);
}

static PyObject* change_object_animation(PyObject *self, PyObject *args) {
  char* c_ptr1; 
  char* c_ptr2; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "ss", &c_ptr1, &c_ptr2)) return NULL;
  if (!c_ptr1 || !c_ptr2) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);
  if (!gResources) {
    return NULL;
  }

  ObjPtr obj = gResources->GetObjectByName(s1);
  if (!obj) return NULL;

  gResources->ChangeObjectAnimation(obj, s2);
  return PyBool_FromLong(0);
}

static PyObject* turn_on_actionable(PyObject *self, PyObject *args) {
  char* c_ptr1; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "s", &c_ptr1)) return NULL;
  if (!c_ptr1) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  if (!gResources) {
    return NULL;
  }

  gResources->TurnOnActionable(s1);
  return PyBool_FromLong(0);

}

static PyObject* turn_off_actionable(PyObject *self, PyObject *args) {
  char* c_ptr1; 

  // How to parse tuple: http://web.mit.edu/people/amliu/vrut/python/ext/parseTuple.html
  if (!PyArg_ParseTuple(args, "s", &c_ptr1)) return NULL;
  if (!c_ptr1) return NULL;

  string s1 = reinterpret_cast<char*>(c_ptr1);
  if (!gResources) {
    return NULL;
  }

  gResources->TurnOffActionable(s1);
  return PyBool_FromLong(0);
}

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

static PyObject* register_callback(PyObject *self, PyObject *args) {
  char* c_ptr1;
  char* c_ptr2; 
  if (!PyArg_ParseTuple(args, "ss", &c_ptr1, &c_ptr2)) {
    return NULL;
  }

  if (!c_ptr1 || !c_ptr2) {
    return NULL;
  }

  string s1 = reinterpret_cast<char*>(c_ptr1);
  string s2 = reinterpret_cast<char*>(c_ptr2);

  if (!gResources) {
    return NULL;
  }

  float seconds = boost::lexical_cast<float>(s2);
  gResources->SetPeriodicCallback(s1, seconds);

  return PyBool_FromLong(0);
}

static PyObject* fade_out(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, "")) {
    return NULL;
  }

  if (!gResources) {
    return NULL;
  }

  gResources->GetConfigs()->fading_out = 60.0f;
  return PyBool_FromLong(0);
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
 { "push_action", push_action, METH_VARARGS,
  "Push action" },
 { "register_onenter_event", register_onenter_event, METH_VARARGS,
  "Register an on enter event" },
 { "register_onleave_event", register_onleave_event, METH_VARARGS,
  "Register an on leave event" },
 { "register_onopen_event", register_onopen_event, METH_VARARGS,
  "Register on open event" },
 { "register_on_finish_dialog_event", register_on_finish_dialog_event, 
  METH_VARARGS, "Register on finish dialog event" },
 { "print_message", print_message, METH_VARARGS,
  "Prints a message" },
 { "change_object_animation", change_object_animation, METH_VARARGS,
  "Changes object animation" },
 { "turn_on_actionable", turn_on_actionable, METH_VARARGS,
  "Turn on actionable" },
 { "turn_off_actionable", turn_off_actionable, METH_VARARGS,
  "Turn off actionable" },
 { "get_game_flag", get_game_flag, METH_VARARGS, "Get game flag" },
 { "set_game_flag", set_game_flag, METH_VARARGS, "Set game flag" },
 { "set_position", set_position, METH_VARARGS, "Set object position" },
 { "register_callback", register_callback, METH_VARARGS, "Register callback" },
 { "fade_out", fade_out, METH_VARARGS, "Fade out" },
 { NULL, NULL, 0, NULL }
};

static PyModuleDef EmbModule = {
  PyModuleDef_HEAD_INIT, "wizard", NULL, -1, EmbMethods,
  NULL, NULL, NULL, NULL
};

static PyObject* PyInit_emb(void) {
  return PyModule_Create(&EmbModule);
}

ScriptManager::ScriptManager(Resources* resources) 
  : resources_(resources) {
  gResources = resources_;
  PyImport_AppendInittab("wizard", &PyInit_emb);
  Py_Initialize();
  LoadScripts();
}

ScriptManager::~ScriptManager() {
  if (Py_FinalizeEx() < 0) {
    cout << "Error in Py_FinalizeEx." << endl;
  }
}

void ScriptManager::LoadScripts() {
  cout << "Running script..." << endl;

  PyObject *pName, *pFunc;

  PySys_SetPath(L"resources/scripts");
  pName = PyUnicode_DecodeFSDefault("beginning");
  module_ = PyImport_Import(pName);
  Py_DECREF(pName);

  if (module_ == NULL) return;

  pFunc = PyObject_GetAttrString(module_, "main");

  if (!pFunc || !PyCallable_Check(pFunc)) return;

  PyObject *pArgs, *pValue;
  pArgs = PyTuple_New(0);

  pValue = PyObject_CallObject(pFunc, pArgs);
}

string ScriptManager::CallStrFn(const string& fn_name) {
  PyObject *pFunc;
  pFunc = PyObject_GetAttrString(module_, fn_name.c_str());
  if (!pFunc || !PyCallable_Check(pFunc)) return "";

  PyObject *pArgs, *pValue;
  pArgs = PyTuple_New(0);
  pValue = PyObject_CallObject(pFunc, pArgs);

  const char* c_string = PyUnicode_AsUTF8(pValue);
  return string(c_string);
}

void ScriptManager::ProcessEvent(shared_ptr<Event> e) {
  string callback;
  switch (e->type) {
    case EVENT_ON_INTERACT_WITH_SECTOR: {
      shared_ptr<RegionEvent> region_event = static_pointer_cast<RegionEvent>(e);
      callback = region_event->callback; 
      break;
    }
    case EVENT_ON_INTERACT_WITH_DOOR: {
      shared_ptr<DoorEvent> door_event = static_pointer_cast<DoorEvent>(e);
      callback = door_event->callback; 
      break;
    }
    default: {
      return;
    }
  } 

  PyObject *pFunc;
  pFunc = PyObject_GetAttrString(module_, callback.c_str());

  if (!pFunc || !PyCallable_Check(pFunc)) return;

  PyObject *pArgs, *pValue;
  pArgs = PyTuple_New(0);

  pValue = PyObject_CallObject(pFunc, pArgs);
}

void ScriptManager::ProcessScripts() {
  vector<shared_ptr<Event>>& events = resources_->GetEvents();
  for (shared_ptr<Event> event : events) {
    cout << "Processing event" << endl; 
    ProcessEvent(event);
  }
  events.clear();
}
