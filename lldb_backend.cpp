#include <stdio.h>
#include <string.h>

#include <lldb/API/LLDB.h>
#include <lldb/API/SBExpressionOptions.h>
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBProcess.h>
#include <lldb/API/SBBreakpoint.h>

struct DebugContext {
  lldb::SBDebugger debugger;
  lldb::SBTarget target;
  lldb::SBProcess process;
  bool is_executing;
  lldb::StateType current_process_state;
};

static void
lldb_log_callback(const char *log, void *baton)
{
  printf("%s", log);
}

void lldb_initialize(DebugContext *context, const char *executable_file) {
  const char *channel = "lldb";
  const char *categories[] =
  {
    "all",
    NULL
  };

  lldb::SBDebugger::Initialize();
  context->debugger = lldb::SBDebugger::Create(true, lldb_log_callback, NULL);
  //debugger.EnableLog(channel, categories);
  context->debugger.SetAsync(false);
  
  context->target = context->debugger.CreateTarget(executable_file);
  if (!context->target.IsValid()) {
    printf("lldb failed to create target for executable\n");
  }
}

void lldb_terminate(DebugContext *context) {
  context->process.Destroy();
  context->debugger.DeleteTarget(context->target);
  lldb::SBDebugger::Destroy(context->debugger);
}

void lldb_step_over(DebugContext *context) {
  if (context->process.GetState() != lldb::eStateStopped)
    return;
  lldb::SBThread thread = context->process.GetSelectedThread();
  thread.StepOver();
}

void lldb_continue_execution(DebugContext* context) {
  if(context->process.GetState() != lldb::eStateStopped)
    return;
  context->process.Continue();
}

void lldb_step_into(DebugContext *context) {
  lldb::StateType state = context->process.GetState();
  if(state != lldb::eStateStopped) {
    return;
  }
  lldb::SBThread thread = context->process.GetSelectedThread();
  thread.StepInto();
}

void lldb_step_out(DebugContext *context) {
  lldb::StateType state = context->process.GetState();
  if(state != lldb::eStateStopped) {
    return;
  }
  lldb::SBThread thread = context->process.GetSelectedThread();
  thread.StepOut();
}

void lldb_run_executable(DebugContext *context, const char **argv) {
  lldb::SBError error;
  lldb::SBLaunchInfo launch_info(argv);
  context->process = context->target.Launch(launch_info, error);
  if (!context->process.IsValid() || !error.IsValid()) {
    log_error("lldb failed to launch debug process for executable\n");
  }  
}

struct PrintValue {
  const char *name_string;
  const char *type_string;
  const char *value_string;
};

//TODO(Torin) Make this smarter
struct PrintInfo {
  uint32_t value_count;
  PrintValue values[16];
};

//XXX TODO(TORIN) fix hacxz
static char string_test[128];



PrintInfo lldb_print_identifier(DebugContext *context, const char *identifier)  {
  PrintInfo info = {};

  auto AddPrintValue = [](PrintInfo& info, lldb::SBValue& value) {
    PrintValue& printValue = info.values[info.value_count];
    printValue.name_string = value.GetName();
    printValue.type_string = value.GetTypeName();
    printValue.value_string = value.GetValue();
    if (value.GetNumChildren() > 0) {
      printValue.value_string = "...";
    }

    if (printValue.value_string == NULL)
      printValue.value_string = "INVALID VALUE";
    if (printValue.name_string == NULL)
      printValue.name_string = "INVALID NAME";
    if (printValue.type_string == NULL)
      printValue.type_string = "INVALID TYPE";

    info.value_count++;
  };

  static auto AddTypePrintValue = [](PrintInfo& info, const char *name, const char *type){
    PrintValue& printValue = info.values[info.value_count];
    printValue.name_string = name; 
    printValue.type_string = type; 
    printValue.value_string = 0; 
    if (printValue.value_string == NULL)
      printValue.value_string = "INVALID VALUE";
    if (printValue.name_string == NULL)
      printValue.name_string = "INVALID NAME";
    if (printValue.type_string == NULL)
      printValue.type_string = "INVALID TYPE";
    info.value_count++;
  };


#if 1

  lldb::SBThread thread = context->process.GetSelectedThread();
  if(!thread.IsValid()) return info;
  lldb::SBFrame frame = thread.GetSelectedFrame();
  if(!frame.IsValid()) return info;

  lldb::SBValue base_value;
  const char *current_ident = identifier;
  const char *current = identifier;
  size_t current_ident_length = 0;

  auto parse_next_ident = [&current, 
  &current_ident, & current_ident_length]() -> bool 
  {
    assert(isalnum(*current) || *current == '_');
    current_ident = current;
    while(*current != 0 && (isalnum(*current) || *current == '_')) {
      current++;
    }

    current_ident_length = current - current_ident;
    if (*current == '.') {
      current++;
      return true;
    } else if (*current == '-' && *(current+1) == '>') {
      current += 2;
      return true;
    }
    return false;
  };


  char temp[1024];
  bool is_expr_compound = parse_next_ident();
  memcpy(temp, current_ident, current_ident_length);
  temp[current_ident_length] = 0;
  
  if (!is_expr_compound) {
    lldb::SBType type = context->target.FindFirstType(temp);
    if (type.IsValid()) {
      info.values[0].type_string = type.GetName();
      info.values[0].name_string = " ";
      info.values[0].value_string = " ";
      info.value_count = 1;
      
      if(type.GetNumberOfFields() > 0){
        for(size_t i = 0; i < type.GetNumberOfFields(); i++){
          auto typeMember = type.GetFieldAtIndex(i);
          AddTypePrintValue(info, typeMember.GetName(), typeMember.GetType().GetName());
        }
      }
      
      return info;
    } 
  }

  base_value = context->target.FindFirstGlobalVariable(temp);
  if (!base_value.IsValid()) {
    //context->target.FindFirstType(temp);
    base_value = frame.FindVariable(temp);
  }

  while (is_expr_compound) {
    is_expr_compound = parse_next_ident();
    memcpy(temp, current_ident, current_ident_length);
    temp[current_ident_length] = 0;
    base_value = base_value.GetChildMemberWithName(temp);
  }
             
  if (base_value.IsValid()) {
    PrintInfo info = {};	
    uint32_t child_count = base_value.GetNumChildren();
    
    if (strcmp(base_value.GetTypeName(), "const char *") == 0) {
      base_value.SetFormat(lldb::Format::eFormatCString);
      info.values[info.value_count].value_string = base_value.GetValue(); 
      if (info.values[info.value_count].value_string == NULL) {
        info.values[info.value_count].value_string = "NULL";
      }
      info.values[info.value_count].name_string = base_value.GetName(); 
      info.values[info.value_count].type_string = base_value.GetTypeName(); 
      info.value_count++;
    }
#if 0
      if (child_count > 0) {
        for (uint32_t i = 0; i < child_count; i++) {
            lldb::SBValue child = base_value.GetChildAtIndex(i);
            string_test[i] = child.GetValue()[0];
          }
          string_test[child_count] = 0;
          info.values[info.value_count].value_string = string_test;
        }
#endif

        else if (child_count > 0) {

          if (child_count > ARRAYCOUNT(info.values))
            child_count = ARRAYCOUNT(info.values);
          for (uint32_t i = 0; i < child_count; i++) {

            lldb::SBValue child = base_value.GetChildAtIndex(i);
            AddPrintValue(info, child);
#if 0
            info.values[info.value_count].name_string = child.GetName();
            info.values[info.value_count].type_string = child.GetTypeName();
            if (child.GetNumChildren() > 0) {
              info.values[info.value_count].value_string = "...";
            } else {
              info.values[info.value_count].value_string = child.GetValue();
              if (info.values[info.value_count].value_string == NULL) {
                info.values[info.value_count].value_string = "NULL";
              }
            }
          info.value_count++;
#endif
          } 
        } else {
          AddPrintValue(info, base_value);
#if 0
          info.values[info.value_count].name_string = base_value.GetName();
          info.values[info.value_count].type_string = base_value.GetTypeName();
          info.values[info.value_count].value_string = base_value.GetValue();
          if (info.values[info.value_count].value_string == NULL) {
            info.values[info.value_count].value_string = "NULL";
          }
          info.value_count++;
#endif
        }
      return info;
      }     
  return info;
}
#endif


void lldb_create_breakpoint(DebugContext *context, const char *symbol_name) {
  lldb::SBBreakpoint breakpoint = context->target.BreakpointCreateByName(symbol_name);
  if (!breakpoint.IsValid() || !breakpoint.IsEnabled()) {
    printf("failed to create breapoint at symbol: %s\n", symbol_name);
  }
}

#if 0
int main()
{
  lldb_initalize("test");
  lldb_create_breakpoint("main");
  lldb_run_executable(NULL);
 
  auto lldb_thread = std::thread(internal_lldb_thread_proc);
  
  bool is_running = true;
  while (is_running) {
    //internal_lldb_process_events();
  }
  
 
  
  
  
  lldb_terminate();
  return 0;
}
#endif
