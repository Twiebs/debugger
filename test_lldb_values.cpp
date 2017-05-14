
#define ARRAYCOUNT(a) (sizeof(a) / sizeof(*a))

#define log_debug(...)  printf(__VA_ARGS__); printf("\n")
#define log_error(...)  printf(__VA_ARGS__); printf("\n")
#define log_info(...)   printf(__VA_ARGS__); printf("\n")

#include "lldb_backend.cpp"

int main() {
  DebugContext debugContext = {};

  lldb_initialize(&debugContext, "test");
  lldb_create_breakpoint(&debugContext, "main");
  lldb_run_executable(&debugContext, nullptr);

  {
    static const uint32_t VALUE_COUNT = 1000;
    lldb::SBValue values[VALUE_COUNT];
    for (uint32_t i = 0; i < VALUE_COUNT; i++) {
      values[i] = debugContext.target.EvaluateExpression("structs");
      values[i].SetPreferDynamicValue(lldb::eDynamicCanRunTarget);
    }

    for (uint32_t i = 0; i < VALUE_COUNT; i++) {
    }
  }

  int shouldDestruct = 1;
  return 0;


}
