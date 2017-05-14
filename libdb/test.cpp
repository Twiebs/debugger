#include <stdio.h>
#include <stdint.h>
#include <assert.h>

struct MyData
{
  int x;
  float foo;
};

struct V3
{
  float x, y, z;
};

struct StringTest {
  char *text;
  size_t length;
};

typedef uint32_t color32_t;

struct LargeStruct {
  uint32_t value_count;
  bool is_running;
  bool is_wireframe;
  bool is_debug_mode;

  color32_t panel_color;
  color32_t background_color;
  color32_t foreground_color;

  uint32_t border_size;
  uint32_t pad_left;
  uint32_t pad_right;
  uint32_t pad_top;
  uint32_t pad_bottom;
};

struct StructWithCharArray {
  char foo[32];
  char bar[32];
};
    
struct NestedStructs
{
  V3 position;
  V3 rotation;
  V3 scale;
};

struct Entity {
  V3 position;
  V3 rotation;
  V3 scale;

  uint64_t flags;
};

#define ENTITY_ARRAY_CAPACITY 1024


struct EntityArray {
  Entity entities[ENTITY_ARRAY_CAPACITY];
  uint64_t entityCount;
};

float Dot(const V3& a, const V3& b)
{
  float result = (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
  return result;
}

void test_function()
{
  V3 a { 5.0, 10.0, 20.0 };
  V3 b { 1.75, 2.0, 0.5 };

  float dot_product = Dot(a, b);
  float results[3];
  results[0] = dot_product; 
}

static int flatGlobal;
static NestedStructs deepGlobal; 

static inline
int BigFunctionWithLongNameAndLargeNumberOfArguments(
  uint32_t a, uint32_t b, uint32_t c,
  float d, float f, float g, float h, 
  float j, float k, double l) {
  return 0;
}

int main()
{
  NestedStructs structs;
  structs.position = { 5, 5, 5};
  structs.rotation = { 0.2, 0.2, 0.2};
  structs.scale = {1,1,1};

  int bigFunctionResult = 
    BigFunctionWithLongNameAndLargeNumberOfArguments(
      2, 3, 4, 
      5, 6, 1234324, 2134324, 
      123123, 123213, 9643123964312);


  printf("hello friend\n");
  printf("hello friendoooooo");

  StructWithCharArray charArray = {};

  EntityArray entities = {};

  flatGlobal = 20;
  deepGlobal.scale.y = 2.0f;

  LargeStruct large_struct = {};
  const char *stringLiteral = "foobar";
  
  int x = 7;
  float foo = 1.234f;

  MyData data = {};
  data.x = 1;
  data.foo = 7.0f;

  V3 positions[4096];

  test_function();
  
  printf("%d\n", x); 
  return 0;
}
