#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wswitch"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


#include <unistd.h>
#include <fcntl.h>
#include <mcheck.h>

#define ARRAYCOUNT(a) (sizeof(a) / sizeof(*a))

#define log_debug(...) AddConsoleEntryFmt(__VA_ARGS__); printf(__VA_ARGS__); printf("\n")
#define log_error(...) AddConsoleEntryFmt(__VA_ARGS__); printf(__VA_ARGS__); printf("\n")
#define log_info(...)  AddConsoleEntryFmt(__VA_ARGS__); printf(__VA_ARGS__); printf("\n")

static void AddConsoleEntryFmt(const char *fmt, ...);

#define literal_strlen(x) (sizeof(x) - 1)

#include <cmath>
#include <functional>

#if 0
#include "gdb_backend.cpp"
#else
#include "lldb_backend.cpp"
#endif

#define debug_initialize(exe) lldb_initialize(exe)
#define debug_terminate lldb_terminate
#define debug_step_into lldb_step_into
#define debug_step_over lldb_step_over

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_opengl.h>

#define DEFAULT_WINDOW_WIDTH 1792 
#define DEFAULT_WINDOW_HEIGHT 1008

static inline
uint32_t min(uint32_t a, uint32_t b)
{
  uint32_t result = a < b ? a : b; 
  return result;
}

static inline
int max(int a, int b)
{
  int result = a > b ? a : b;
  return result;
}

struct RGB8
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct RGBA8
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct Panel 
{
  uint32_t x, y;
  uint32_t w, h;
};

struct PanelStyle
{
  TTF_Font* font;
  RGBA8 fontColor;
  uint32_t lineSpacing;

  uint32_t fontSize;
  bool drawLineNumbers;

  uint32_t pad_left;
  uint32_t pad_right;
  uint32_t pad_top;
  uint32_t pad_bottom;
  uint32_t border_size;

  RGBA8 color_background;
  RGBA8 color_border; 
  RGBA8 color_syntax_keyword;
  RGBA8 color_syntax_type;
  RGBA8 color_syntax_literal;
};

struct SyntaxStyle {
  RGBA8 keywordColor;
  RGBA8 typeColor;
  RGBA8 identColor;
  RGBA8 valueColor;
  RGBA8 stringColor;
  RGBA8 commentColor;
};

struct InputState
{
  int mouse_x;
  int mouse_y;
};

struct StringBuffer
{
  char* memory;
  size_t size;
  size_t used;
};

enum ConsoleEntryType {
  ConsoleEntryType_APPLICATION,
  ConsoleEntryType_DEBUGGER,
};

struct ConsoleEntry {
  ConsoleEntryType type;
  const char *text;
  size_t length;
};

struct Console {
  static const size_t MAX_ENTRY_COUNT = 1024;

  uint32_t entryCount;
  ConsoleEntry entries[MAX_ENTRY_COUNT];
  StringBuffer buffer;
};

struct Expression {
  bool isExpanded;
  const char *name;
  const char *type;
  const char *value;
  uint32_t childCount;
  uint32_t depth;
  Expression *children;
};

struct BreakpointInfo {
  lldb::SBBreakpoint lldbBreakpoint;
  const char *fileName;
  uint32_t lineNumber;
};

struct ExpressionList {
  Expression *expressions[128]; 
  uint32_t count;
  uint32_t currentLineNumber;
};

struct BreakpointList {
  BreakpointInfo breakpoints[16];
  uint32_t breakpointCount;
};

static Console _console;
Console& GetConsole() {
  return _console;
}

struct Controls {
  bool stepOver;
  bool stepInto;
  bool stepOut;
  
  bool moveCursorDown;
  bool moveCursorUp;
  bool moveCursorLeft;
  bool moveCursorRight;

  bool showFuzzySearch;
  bool continueExecution;
  
  bool isActionDown;
  bool isRemoveDown;
};

enum PanelType {
  PanelType_TextBuffer,
  PanelType_ExpressionList,
  PanelType_Mouseover,
  PanelType_Output,
  PanelType_Count,
};

struct GUIContext
{
  uint32_t screen_width;
  uint32_t screen_height;

  char active_filename[256];
  size_t active_filename_length;
  char *active_filedata;
  size_t active_filesize;

  const char **functionNameList;
  size_t functionNameListCount;
  
  BreakpointList breakpointList;
  bool breakpointWasJustHit;

  ExpressionList expressionList;

  uint32_t line_offsets[10000];
  uint32_t line_count;

  uint32_t cursor_line_number;
  uint32_t cursor_column_number;
  float cursor_time_elapsed_since_last_blink;

  uint32_t top_line_in_panel;
  uint32_t max_lines_in_panel;

  uint32_t line_execution_stopped;

  uint32_t line_spacing;

  union {
    struct { 
      Panel text_panel;
      Panel expressionPanel;
      Panel tooltip_panel;
      Panel output_panel;
    };
    Panel panels[PanelType_Count];
  };
  
  PanelStyle expressionPanelStyle;  
  PanelStyle text_style;
  PanelStyle watch_style;
  PanelStyle output_style;

  SyntaxStyle syntaxStyle;

  char identUnderCursor[1024];
  StringBuffer mouseOverStringBuffer;
  PanelStyle mouseOverPanelStyle;


  char inputBuffer[1024];
  size_t inputBufferCount;
  bool isInputBufferFocused;


  bool tooltip_is_compound;

  uint32_t font_size;


  int32_t linesToScroll;

  bool is_running;
  bool requires_refresh;

  //TODO(Torin) How do you want to do this?
  //Probably move these out into an input state struct?
  //or mabye a command struct or something?

  SDL_Renderer *renderer;
  SDL_Window *window;  
  TTF_Font *font;

  Controls controls;

  //GDBContext gdb;
  
  DebugContext debug_context;
  bool is_debugger_executing;
  
  InputState input;
};

#define memcpy_literal_and_increment_dest(dest, literal) memcpy(dest, literal, literal_strlen(literal)); dest+=literal_strlen(literal)
#define memcpy_and_increment_dest(dest, src, size) memcpy(dest, src, size); dest += size

#define memcpy_and_increment(writeptr, readptr, size) memcpy(writeptr, readptr, size); writeptr += size

#define memcpy_literal_and_increment(dest, literal) memcpy(dest, literal, literal_strlen(literal)); dest+=literal_strlen(literal)
#define memcpy_and_increment_write_ptr(writeptr, readptr, size) memcpy(writeptr, readptr, size); writeptr += size
#define memcpy_literal_and_inceremnt_write_ptr(dest, literal) memcpy(dest, literal, literal_strlen(literal)); dest+=literal_strlen(literal)

//===================================================
static inline 
const char *AppendToStringBuffer(const char *str, 
    const size_t length, StringBuffer *buffer) 
{
  assert(buffer->used + length < buffer->size);
  memcpy(&buffer->memory[buffer->used], str, length);
  const char *result = &buffer->memory[buffer->used];
  buffer->used += length;
  return result;
}

static inline
const char *AppendFmtToStringBuffer(StringBuffer *buffer, size_t *bytesWritten, const char *fmt, va_list args) {
  size_t bytesRemainingInBuffer = buffer->size - buffer->used;
  char *writeLocation = &buffer->memory[buffer->used];
  *bytesWritten = vsnprintf(writeLocation, bytesRemainingInBuffer, fmt, args);
  assert((*bytesWritten < bytesRemainingInBuffer) && *bytesWritten > 0);
  buffer->used += *bytesWritten;
  return writeLocation;
}

static inline 
void AppendCharToStringBuffer(const char c, StringBuffer *buffer) {
  assert(buffer->used + 1 < buffer->size);
  buffer->memory[buffer->used] = c;
  buffer->used += 1;
}

static inline 
void ClearStringBuffer(StringBuffer *buffer) {
  memset(buffer->memory, 0, buffer->size);
  buffer->used = 0;
}

#define AppendLiteralToStringBuffer(literal, buffer) \
  AppendToStringBuffer(literal, literal_strlen(literal), buffer)

//===========================================================
static 
void AddConsoleEntryFmt(const char *fmt, ...) {
  Console& console = GetConsole();
  va_list args;
  va_start(args, fmt);
  ConsoleEntry& entry = console.entries[console.entryCount];
  entry.text = AppendFmtToStringBuffer(&console.buffer, &entry.length, fmt, args);
  entry.type = ConsoleEntryType_DEBUGGER;
  console.entryCount++;
  va_end(args);
}

#include "expression.cpp"
#include "draw.cpp"
#define LiteralAndLength(literal) literal, literal_strlen(literal)

  static inline
int is_point_in_panel(Panel* panel, int x, int y) 
{
  if ((int)panel->x > x || (int)panel->y > y) return 0;
  if ((int)(panel->x + panel->w) < x) return 0;
  if ((int)(panel->y + panel->h) < y) return 0;
  return 1;
}

static inline 
int is_point_in_panel_text_region(Panel *panel, const PanelStyle& style, int x, int y)
{
  if ((int)(panel->x + style.pad_left + style.border_size) > x) return 0;
  if ((int)(panel->y + style.pad_bottom + style.border_size) > y) return 0;
  if ((int)(panel->x + panel->w - style.pad_right - style.border_size) < x) return 0;
  if ((int)(panel->y + panel->h - style.pad_top - style.border_size) < y) return 0;
  return 1;
}

static inline void screen_to_panel_text_region_coordinates(GUIContext * context, Panel *panel, const PanelStyle& style, const int screen_x, const int screen_y, int *out_x, int *out_y)
{
  *out_x = screen_x - (panel->x + style.pad_left + style.border_size);
  *out_y = screen_y - (panel->y + style.pad_right + style.border_size);
  if (style.drawLineNumbers) {
    *out_x -= GetRequiredLineNumberRectWidth(context);
  }
}

static inline void gui_goto_line(GUIContext *context, uint32_t line_number);
static inline void gui_draw_panel(GUIContext* context, Panel *panel, const PanelStyle& style);

//static inline void GUIShowFuzzySearch(GUIContext *context);

void ToggleBreakpointAtLine(GUIContext *context, const uint32_t requestedLineNumber) {
  BreakpointList& bpList = context->breakpointList;
  for (uint32_t i = 0; i < bpList.breakpointCount; i++) {
    BreakpointInfo& bpInfo = bpList.breakpoints[i];
    if (bpInfo.lineNumber == requestedLineNumber) {
      bpInfo.lldbBreakpoint.ClearAllBreakpointSites();
      context->debug_context.target.BreakpointDelete(bpInfo.lldbBreakpoint.GetID());
      log_info("breakpoint-removed at line %u", requestedLineNumber);
      if (bpList.breakpointCount > 1 && (i < (bpList.breakpointCount-1))) {
        bpList.breakpoints[i] = bpList.breakpoints[bpList.breakpointCount - 1];
      } else {
        bpList.breakpoints[i] = {};
      }
      bpList.breakpointCount--;
      return;
    }
  }

  assert(bpList.breakpointCount + 1 < ARRAYCOUNT(BreakpointList::breakpoints));
   lldb::SBBreakpoint lldbBreakpoint = 
     context->debug_context.target.BreakpointCreateByLocation
     (context->active_filename, requestedLineNumber);
  
   if (lldbBreakpoint.IsValid()) {
    BreakpointInfo& bpInfo = bpList.breakpoints[bpList.breakpointCount];
    lldb::SBBreakpointLocation bpLocation = lldbBreakpoint.GetLocationAtIndex(0);
    lldb::SBAddress addr = bpLocation.GetAddress();
    lldb::SBLineEntry line_entry = addr.GetLineEntry();
    lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
    uint32_t line_number = line_entry.GetLine();
    if (line_number != requestedLineNumber) {
      bpInfo.lldbBreakpoint.ClearAllBreakpointSites();
      context->debug_context.target.BreakpointDelete(bpInfo.lldbBreakpoint.GetID());
      log_info("could not created breakpoint at line %u", requestedLineNumber);
    } else {
      bpList.breakpoints[bpList.breakpointCount].lldbBreakpoint = lldbBreakpoint;
      bpList.breakpoints[bpList.breakpointCount].lineNumber = line_number;
      bpList.breakpointCount++;
      log_info("created breakpoint at lineNumber %u", line_number);
    }
  } else {
    log_info("breakpoint is invalid\n");
  }
}

static inline
void SetActiveSourceFile(GUIContext *context, const char *filename)
{
  size_t filenameLength = strlen(filename);
  assert(filenameLength < ARRAYCOUNT(context->active_filename));
  context->active_filename_length = filenameLength;
  memcpy(context->active_filename, filename, filenameLength);
  context->active_filename[filenameLength] = 0;

  //We assume that if the file is not found this is an internal error because all the paths
  //should have been confirmed to be valid before now
  //TODO(Torin) add a more robust error checking on this file
  FILE *file = fopen(context->active_filename, "rb");
  if (file == nullptr) {
    log_error("Could not find file %s."
      "Has the file moved or been deleted since the last time the target executable was compiled?", 
      filename);
  }
  fseek(file, 0, SEEK_END);
  context->active_filesize = ftell(file);
  fseek(file, 0, SEEK_SET);

  //TODO(Torin) Make check to see if the current buffer is big enough to hold the
  //new filedata the we require to avoid calling malloc
  if (context->active_filedata != nullptr) 
    free(context->active_filedata);
  context->active_filedata = (char *)malloc(context->active_filesize + 1);
  context->active_filedata[context->active_filesize] = 0;
  fread(context->active_filedata, 1, context->active_filesize, file);
  fclose(file);

  memset(context->line_offsets, 0, ARRAYCOUNT(context->line_offsets));
  context->line_count = 0;

  char *current = context->active_filedata;
  uint32_t current_offset = 0;
  context->line_offsets[0] = 0;
  context->line_count++;
  while (current[current_offset] != 0) {
    if (current[current_offset] == '\n') {
      context->line_offsets[context->line_count] = current_offset + 1;
      context->line_count++;
    }
    current_offset++;
  }

  //NOTE(Torin) bad if file is empy but it doesnt matter beacause if we break in the file
  //its imposible that its empty //TODO(Torin) unless the file has been modified and the line count
  //no longer coresponds to the line count in the binary in which case the program would crash
  context->line_offsets[context->line_count] = context->line_offsets[context->line_count - 1];
  context->top_line_in_panel = 1;
  log_debug("opened file %s", filename);
}

#define INVALID_INDEX_32 ((uint32_t)((uint64_t)1 << 32) - 1)

uint32_t GetPanelIndexFromPoint(GUIContext *context, uint32_t x, uint32_t y) {
  for(uint32_t i = 0; i < PanelType_Count; i++) {
    Panel& panel = context->panels[i];
    if (panel.x <= x && panel.y <= y && 
        panel.x + panel.w >= x && panel.y + panel.h >= y) {
      return i;
    }
  }
  return INVALID_INDEX_32;
}
static inline
uint32_t GetLineNumberAtScreenPointInPanel(int x, int y, const Panel& panel, const PanelStyle& style) {
  if (y < (int)panel.y || (int)((int)(y > (int)(panel.y + panel.h)))) return 0; 
  int textBufferYPos = y - (panel.y + style.border_size + style.pad_top);
  if (textBufferYPos < 0) return 0;

  uint32_t lineNumber = textBufferYPos / (style.lineSpacing + style.fontSize);
  return lineNumber;
}

#if 1
static Expression * 
GetExpressionAtLineIndex(Expression* expr, uint32_t *currentIndex, uint32_t requestedIndex) {
  if (*currentIndex >= requestedIndex) {
    return expr;
  }

  *currentIndex = *currentIndex + 1;
  if (expr->isExpanded) {
    for(uint32_t i = 0; i < expr->childCount; i++) {
      Expression* child = &expr->children[i];
      auto foundExpr = GetExpressionAtLineIndex(child, currentIndex, requestedIndex);
      if (foundExpr) {
        return foundExpr;
      }
    }
  } 

  return nullptr;
}
#endif

static Expression *
GetExpressionAtLineIndex(ExpressionList *list, 
uint32_t requestedIndex, uint32_t *rootIndex) {
  uint32_t currentIndex = 0;
  for (uint32_t i = 0; i < list->count; i++) {
    Expression* expr = list->expressions[i];
    Expression* result = GetExpressionAtLineIndex(expr, &currentIndex, requestedIndex);
    if (result != nullptr) {
      *rootIndex = currentIndex;
      return result;
    }
  }
  return nullptr;
}

static inline
uint32_t gui_get_line_number_at_mouse(GUIContext *context) {
  int text_x, text_y;
  Panel *panel = &context->text_panel;
  if (!is_point_in_panel_text_region(panel, context->text_style, 
      context->input.mouse_x, context->input.mouse_y)) {
    return 0;
  }
  
  screen_to_panel_text_region_coordinates(context, panel, context->text_style, 
      context->input.mouse_x, context->input.mouse_y, &text_x, &text_y);

  int line_height = context->line_spacing + context->font_size;
  int line_number = text_y / line_height;
  line_number += context->top_line_in_panel;
  return line_number;
}

static inline
void ProcessPanelBasedInput(GUIContext *context) {
  uint32_t panelIndex = GetPanelIndexFromPoint(context, 
      context->input.mouse_x, context->input.mouse_y);
  if (panelIndex == INVALID_INDEX_32) return;
  Panel& panel = context->panels[panelIndex];
  
  switch(panelIndex) {

    case PanelType_ExpressionList: {
      if(context->controls.isActionDown){
        ExpressionList& list = context->expressionList;
        uint32_t lineNumber = GetLineNumberAtScreenPointInPanel(context->input.mouse_x, context->input.mouse_y, 
          context->expressionPanel, context->expressionPanelStyle);

        uint32_t rootExprIndex = 0;
        Expression *expr = GetExpressionAtLineIndex(&list, lineNumber, &rootExprIndex);
        if(expr != nullptr){
          if(context->controls.isRemoveDown){
            DestroyWatchExpression(rootExprIndex, &list);
          } else {
            expr->isExpanded = !expr->isExpanded;
          }
        }
      }

    } break;

    case PanelType_TextBuffer: {
      if (context->controls.isActionDown){
        if(context->controls.isRemoveDown){
          ToggleBreakpointAtLine(context, gui_get_line_number_at_mouse(context));

        } else if(context->identUnderCursor[0] != 0){
          CreateWatchExpression(&context->expressionList, &context->debug_context, context->identUnderCursor);
        }
      }  
    } break;
 };
}

static inline int gui_get_ident_under_point(GUIContext *context, 
  int pointX, int pointY, char *dest, size_t destSize) {
  Panel *panel = &context->text_panel;
  if (!is_point_in_panel_text_region(panel, 
    context->text_style, pointX, pointY)) {
    return 0;
  }

  int text_x, text_y;
  screen_to_panel_text_region_coordinates(context, panel, context->text_style, 
      pointX, pointY, &text_x, &text_y);

  int line_height = context->line_spacing + context->font_size;
  int line_number = text_y / line_height;
  line_number += context->top_line_in_panel - 1;

  if (line_number < (int)context->line_count) {
    int line_length = context->line_offsets[line_number + 1] -
      context->line_offsets[line_number];

    char *line_begin = context->active_filedata + context->line_offsets[line_number];
    char *current = line_begin;
    int total_width = 0;

    char *write_pos = dest;

    while ((current - line_begin) < line_length) {
      if (*current == '.') {
        assert((size_t)((write_pos + 1) - dest) < destSize);
        memcpy(write_pos, current, 1);
        write_pos[1] = 0;
        current++;
        int text_width, text_height;
        TTF_SizeText(context->font, write_pos, &text_width, &text_height);
        write_pos += 1;
        total_width += text_width;
        if (total_width > text_x) {
          memset(dest, 0, destSize);
          return 0;
        }
      }

      else if ((*current == '-' && *(current + 1) == '>')) {
        assert((size_t)((write_pos + 2) - dest) < destSize);
        memcpy(write_pos, current, 2);
        write_pos[2] = 0;
        int text_width, text_height;
        TTF_SizeText(context->font, write_pos, &text_width, &text_height);
        write_pos += 2;
        current += 2;
        total_width += text_width;
        if (total_width > text_x) {
          memset(dest, 0, destSize);
          return 0;
        }
      }

      else if (isalpha(*current) || *current == '_') {
        char *ident_begin = current;
        while (isalnum(*current) || *current == '_' || *current == '[' || *current == ']')
          current++;
        size_t ident_length = current - ident_begin;
        assert((size_t)((write_pos + ident_length) - dest) < destSize);
        memcpy(write_pos, ident_begin, ident_length);
        write_pos[ident_length] = 0;
        int text_width, text_height;
        TTF_SizeText(context->font, write_pos, &text_width, &text_height);
        write_pos += ident_length;
        total_width += text_width;
        if (total_width > text_x) {
          return 1;
        }
      }

      else {
        write_pos = dest;
        char *text_begin = current;
        if (isspace(*current)) {
          while (isspace(*current))
            current++;
        } else if (isdigit(*current)) {
          while (!isspace(*current))
            current++;
        } else {
          current++;
        }

        size_t text_length = current - text_begin;
        memcpy(write_pos, text_begin, text_length);
        write_pos[text_length] = 0;

        int text_width, text_height;
        TTF_SizeText(context->font, dest, &text_width, &text_height);
        total_width += text_width;

        if (total_width > text_x) {
          memset(dest, 0, destSize);
          return 0;
        }
      }
    }
  }
  return 0;
}


  static inline
int copy_line_to_buffer(GUIContext* context, uint32_t lineNumber, char *buffer, size_t bufferLength) 
{
  assert(lineNumber < context->line_count);
  uint32_t lineIndex = lineNumber - 1;

  uint32_t lineOffsetA = context->line_offsets[lineIndex];
  uint32_t lineOffsetB = context->line_offsets[lineIndex+1];
  const char *lineData = context->active_filedata + lineOffsetA;
  uint32_t lineLength = lineOffsetB - lineOffsetA;
  //NOTE(Torin) This is to remove the newline char since we are storing
  //the offset into the filebuffer rather than lineLengths directly
  //TODO(Torin) This will break with non unix line endings
  //consider just storing the length and a pointer upfront with each line entry
  if (lineLength > 0) {
    lineLength--;
  }

  size_t bufferIndex = 0;
  for (uint32_t i = 0; i < lineLength; i++) {
    if (lineData[i] == '\t') {
      buffer[bufferIndex] = ' ';
      buffer[bufferIndex+1] = ' ';
      bufferIndex += 2;
      if (bufferIndex > bufferLength) {
        return 0;
      }
    } else {
      buffer[bufferIndex] = lineData[i];
      bufferIndex++;
      if (bufferIndex > bufferLength) {
        return 0;
      }
    }
  } 
  buffer[bufferIndex] = 0;
  return 1;
}

static inline
void gui_render_buffer(GUIContext *context)
{
  Panel *panel = &context->text_panel;
  const PanelStyle& style = context->text_style;
  gui_draw_panel(context, panel, context->text_style);


  { //Draw breakpoints
    BreakpointList& bpList = context->breakpointList;
    for (uint32_t i = 0; i < bpList.breakpointCount; i++) {
      BreakpointInfo& info = bpList.breakpoints[i];
      if (info.lineNumber > context->top_line_in_panel &&
          info.lineNumber < (context->top_line_in_panel + context->max_lines_in_panel)) {
        gui_highlight_line(context, info.lineNumber, RGBA8 { 200, 20, 20, 30 });
      }
    }
  }

  if (context->is_debugger_executing == false) {
    if (context->breakpointWasJustHit) {
      gui_highlight_line(context, context->line_execution_stopped, 
        RGBA8 { 160, 140, 20, 30 });
    } else {
      gui_highlight_line(context, context->line_execution_stopped, 
        RGBA8 { 20, 140, 20, 30 });
    }
  }
  
  SDL_Color color = { 215, 215, 215, 255 };
  uint32_t line_count_to_render = min(context->max_lines_in_panel, context->line_count);


  uint32_t lineNumberRectWidth = GetRequiredLineNumberRectWidth(context);
  
  for (uint32_t i = 0; i < line_count_to_render; i++) {
    uint32_t lineNumber = context->top_line_in_panel + i;
    char line_buffer[512];
    copy_line_to_buffer(context, lineNumber, line_buffer, ARRAYCOUNT(line_buffer)); 

    uint32_t verticalDisplacementPerLine = context->line_spacing + context->font_size;


    if (style.drawLineNumbers) {
      char lineNumberText[10];
      snprintf(lineNumberText, 10, "%d", (int)lineNumber);
     
      SDL_Surface *lineNumberSurface = TTF_RenderText_Blended(context->font, 
          lineNumberText, color);
      SDL_Texture *lineNumberTexture = SDL_CreateTextureFromSurface(context->renderer, 
          lineNumberSurface);

      SDL_Rect lineNumberRect =  {
        (int)(style.border_size + style.pad_left),
        (int)((i * verticalDisplacementPerLine) + style.border_size + (int)style.pad_top),
        lineNumberSurface->w,
        lineNumberSurface->h,
      };

      SDL_RenderCopy(context->renderer, lineNumberTexture, NULL, &lineNumberRect);
      SDL_DestroyTexture(lineNumberTexture);
      SDL_FreeSurface(lineNumberSurface);
    }

    if (line_buffer[0] != 0) {
      SDL_Surface *text_surface = TTF_RenderText_Blended(context->font, line_buffer, color);
      
      SDL_Rect text_rect = {
          (int)(style.border_size + style.pad_left),
          (int)((i * verticalDisplacementPerLine) + style.border_size + (int)style.pad_top),
          text_surface->w,
          text_surface->h
      };

      if (style.drawLineNumbers) {
        text_rect.x += lineNumberRectWidth; 
      }

          SDL_Texture *text_texture = SDL_CreateTextureFromSurface(context->renderer, text_surface);
      SDL_RenderCopy(context->renderer, text_texture, NULL, &text_rect);
      SDL_DestroyTexture(text_texture);
      SDL_FreeSurface(text_surface);
    }
  }
}

static inline
void gui_update_input(GUIContext *context)
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    ImGui_ImplSdl_ProcessEvent(&event);
    switch(event.type)
    {
        case SDL_QUIT:
        {
            context->is_running = false;
        } break;

        case SDL_KEYDOWN:
        {

            if (event.key.keysym.sym == SDLK_a) {
              context->controls.stepOver = true;
            } else if (event.key.keysym.sym == SDLK_s) {
              context->controls.stepInto = true;
            } else if (event.key.keysym.sym == SDLK_j) {
              context->linesToScroll += 1;
            } else if (event.key.keysym.sym == SDLK_k) { 
              context->linesToScroll -= 1;
            } else if (event.key.keysym.sym == SDLK_c) {
              context->controls.continueExecution = true;
            } else if (event.key.keysym.sym == SDLK_p) {
              if (event.key.keysym.mod & (KMOD_CTRL)) {
                context->controls.showFuzzySearch = true;
              }

            } else if (event.key.keysym.sym == SDLK_LCTRL){
              context->controls.isRemoveDown = true;
            }

        } break;

        case SDL_KEYUP: {
          if (event.key.keysym.sym == SDLK_LCTRL){
            context->controls.isRemoveDown = false;
          }


        } break;

        case SDL_MOUSEMOTION:
        {
          int mouse_x = event.motion.x;
          int mouse_y = event.motion.y;
          context->input.mouse_x = mouse_x;
          context->input.mouse_y = mouse_y;
        } break;

        case SDL_MOUSEWHEEL: 
        {
          context->linesToScroll -= (event.wheel.y * 2);
        } break;

        case SDL_MOUSEBUTTONDOWN: {
          context->controls.isActionDown = true;
          uint32_t keymod = SDL_GetModState(); 
          if (keymod & KMOD_CTRL) {
            
          } else {

          }

        } break;
    }
  }
}

static inline
void gui_goto_line(GUIContext *context, uint32_t line_number)
{
  int realitiveCenterLineNumber = (int)context->max_lines_in_panel / 2;
  int requested_top_line = (int)line_number - realitiveCenterLineNumber;
  int max_top_line_possible = context->line_count - context->max_lines_in_panel;
  if (max_top_line_possible < 1) 
    max_top_line_possible = 1;

  int newTopLine = requested_top_line;
  if (requested_top_line < 1)
    newTopLine = 1;
  else if (requested_top_line > max_top_line_possible)
    newTopLine = max_top_line_possible;  

  context->top_line_in_panel = newTopLine;
  log_debug("set top line to %d, requested line was %d", newTopLine, requested_top_line);
}

static inline
void gui_set_top_line(GUIContext *context, int line_number) {
  if (line_number < 1) {
    line_number = 1;
  } else {
    line_number = min(line_number, 
      context->line_count - context->max_lines_in_panel);    
  }
  context->top_line_in_panel = line_number;
}

static inline
void gui_process_input(GUIContext *context) 
{

  const InputState& input = context->input;
  if (context->linesToScroll) {
    //GetPanelUnderPoint(input.mouse_x, input.mouse_y);

    gui_set_top_line(context, context->top_line_in_panel + context->linesToScroll);
    context->linesToScroll = 0;
  }
  
  if (context->controls.stepOver) {
    // write_literal(context->gdb.output_pipe, "next\n");
    lldb_step_over(&context->debug_context);
    context->is_debugger_executing = true;
    context->breakpointWasJustHit = false;
  }

  if (context->controls.stepInto){
    //write_literal(context->gdb.output_pipe, "step\n");
    lldb_step_into(&context->debug_context);
    context->is_debugger_executing = true;
    context->breakpointWasJustHit = false;
  }

  if (context->controls.continueExecution) {
    lldb_continue_execution(&context->debug_context);
    context->is_debugger_executing = true;
    context->breakpointWasJustHit = false;
  }
}

static inline
void InitStringBuffer(StringBuffer *buffer, size_t size) {
  buffer->used = 0;
  buffer->size = size;
  buffer->memory = (char *)malloc(size);
}

#define FONT_FILE "/usr/share/fonts/TTF/DejaVuSans.ttf"


static inline
const char** CreateTypeNameList(uint32_t mask, size_t *outCount, lldb::SBTarget target) {
  const char** result = 0;
  lldb::SBModule module = target.GetModuleAtIndex(0);
  assert(module.GetNumCompileUnits() >= 1);
  for(size_t i = 0; i < module.GetNumCompileUnits(); i++){
    auto compileUnit = module.GetCompileUnitAtIndex(i);
    auto typeList = compileUnit.GetTypes(mask);//MASK functions
    result = (const char **)malloc(sizeof(char*) * typeList.GetSize()); 
    for(size_t i = 0; i < typeList.GetSize(); i++){
      auto type = typeList.GetTypeAtIndex(i);
      result[i] = type.GetName();
    }
  }
  return result;
}

static inline
void gui_initalize(GUIContext *context)
{
  memset(context, 0, sizeof(GUIContext));
  context->screen_width = DEFAULT_WINDOW_WIDTH;
  context->screen_height = DEFAULT_WINDOW_HEIGHT;

  if(SDL_Init(SDL_INIT_VIDEO)){
    printf("Error: %s\n", SDL_GetError());
  }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

  if (TTF_Init() == -1) {
    log_error("TFF Failed to initalize, %s", TTF_GetError());
  }

  context->font = TTF_OpenFont(FONT_FILE, 16);
  if (context->font == NULL) {
    log_error("Failed to open font: %s", TTF_GetError());
  }

  int oglIdx = -1;
  int nRD = SDL_GetNumRenderDrivers();
  for(int i=0; i<nRD; i++) {
    SDL_RendererInfo info;
    if(!SDL_GetRenderDriverInfo(i, &info)){
      if(!strcmp(info.name, "opengl")){
        oglIdx = i;
      }
    }
  }

  context->window = SDL_CreateWindow("GDBFrontend",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, context->screen_width, context->screen_height, SDL_WINDOW_OPENGL);
  context->renderer = SDL_CreateRenderer(context->window, oglIdx, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); 
  SDL_GLContext glContext = SDL_GL_CreateContext(context->window);
  ImGui_ImplSdl_Init(context->window);

  //TODO(Torin) Revert mouseOver size back to somthing sane
  InitStringBuffer(&context->mouseOverStringBuffer, 1024*1024*4);
  InitStringBuffer(&GetConsole().buffer, 1024*1024);
  
  context->font_size = 16;
  context->line_spacing = 4;

  Panel *panel = &context->text_panel;
  context->text_panel.w = context->screen_width / 2;
  context->text_panel.h = context->screen_height;

  context->output_panel.x = context->screen_width / 2;
  context->output_panel.y = 0;
  context->output_panel.w = context->screen_width / 2;
  context->output_panel.h = context->screen_height /2;

  context->expressionPanel.x = context->screen_width / 2;
  context->expressionPanel.y = context->screen_height / 2;
  context->expressionPanel.w = context->screen_width / 2;
  context->expressionPanel.h = context->screen_height / 2;
  
  { //@Syntax @Style
    SyntaxStyle& style = context->syntaxStyle;
    style.typeColor  = RGBA8 { 139, 191, 239, 255 };
    style.identColor = RGBA8 { 236, 236, 236, 255 };
    style.valueColor = RGBA8 { 217, 206, 132, 255 }; 
  }

  PanelStyle commonStyle = {};
  commonStyle.font = context->font;
  commonStyle.fontSize = 14;
  commonStyle.fontColor = { 255, 255, 255, 255 };
  commonStyle.border_size = 2;
  commonStyle.pad_left = 2;
  commonStyle.pad_right = 2;
  commonStyle.pad_top = 2;
  commonStyle.pad_bottom = 2;
  commonStyle.color_background = { 22, 22, 22, 255 };
  commonStyle.color_border = { 200, 200, 200, 255 };

  context->text_style = commonStyle;
  context->output_style = commonStyle;
  context->mouseOverPanelStyle = commonStyle;
  context->expressionPanelStyle = commonStyle;


  context->expressionPanelStyle.lineSpacing = 2;
  context->expressionPanelStyle.fontSize = 18;
  context->expressionPanelStyle.pad_left = 6;

  context->text_style.drawLineNumbers = true;
  context->text_style.border_size = 0;

  context->max_lines_in_panel = panel->h / (context->font_size + context->line_spacing);
  context->is_running = true;


}

static void 
internal_lldb_process_events(GUIContext *context) {
  DebugContext *debug_context = &context->debug_context;
  
  lldb::StateType process_state = debug_context->process.GetState();
  if (process_state != debug_context->current_process_state 
    || debug_context->is_executing != context->is_debugger_executing) {
      
    switch(process_state) {
      //@Running @Process
      case lldb::eStateRunning: {
        log_info("state: running");
        debug_context->is_executing = true;
      } break;
      
      //@Stopped @Process 
      case lldb::eStateStopped: {
        lldb::SBThread thread = debug_context->process.GetSelectedThread();
        lldb::StopReason stop_reason = thread.GetStopReason();

        static auto updateStopInfo = [](GUIContext *context, const lldb::SBLineEntry lineEntry, const lldb::SBFileSpec fileSpec) {
          uint32_t line_number = lineEntry.GetLine();
          char temp[1024];
          uint32_t bytesRequired = fileSpec.GetPath(temp, ARRAYCOUNT(context->active_filename));
          if (bytesRequired > ARRAYCOUNT(context->active_filename)) {
            assert(false);
          }

          if (temp[0] != 0) {
            if (strcmp(temp, context->active_filename)) {
              SetActiveSourceFile(context, temp);
            }
            if (context->line_execution_stopped != line_number) {
              gui_goto_line(context, line_number);
              context->line_execution_stopped = line_number;
              context->requires_refresh = true;
            }
          }
        };
        
        if (stop_reason == lldb::eStopReasonBreakpoint) {
          if (thread.GetStopReasonDataCount() > 0) {
            debug_context->current_process_state = process_state;
            context->is_debugger_executing = false;
            
            uint64_t bp_id = thread.GetStopReasonDataAtIndex(0);
            lldb::SBBreakpoint breakpoint = debug_context->target.FindBreakpointByID(bp_id);
            lldb::SBBreakpointLocation location = breakpoint.GetLocationAtIndex(0);
            lldb::SBAddress addr = location.GetAddress();
            lldb::SBFunction func = addr.GetFunction();
            
            lldb::SBLineEntry line_entry = addr.GetLineEntry();
            lldb::SBFileSpec fileSpec = line_entry.GetFileSpec();
            updateStopInfo(context, line_entry, fileSpec);
            context->breakpointWasJustHit = true;
          }
        }
        
        else { //End step range
          debug_context->current_process_state = process_state;
          context->is_debugger_executing = false;
          lldb::SBFrame frame = thread.GetSelectedFrame();

          lldb::SBLineEntry line_entry = frame.GetLineEntry();
          lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
          updateStopInfo(context, line_entry, file_spec);
        }
        
      } break;
      
      case lldb::eStateExited: {
        debug_context->current_process_state = process_state;
        context->is_debugger_executing = false;
        lldb::SBThread thread = debug_context->process.GetSelectedThread();  
        lldb::SBFrame frame = thread.GetSelectedFrame();
        
        log_debug("stopped-exited");
        log_info("exited");
        context->line_execution_stopped = 0;
        context->requires_refresh = true;
      } break;
    }
  }
}

void CreateBreakpoint(GUIContext *context, const char *expr) {
  BreakpointList& bpList = context->breakpointList;
  assert(bpList.breakpointCount + 1 < ARRAYCOUNT(BreakpointList::breakpoints));
  lldb::SBBreakpoint lldbBreakpoint;
  bool breakpointWasCreated = false;
  uint32_t requestedLineNumber = 0;

  char temp[256];
  const char *current = expr;
  while (*current != 0) {
    if (*current == ':') {
      size_t filenameLength = current - expr;
      assert(filenameLength + 1< ARRAYCOUNT(temp));
      memcpy(temp, expr, filenameLength);
      temp[filenameLength] = 0;
      const char *filename = temp;
      uint32_t lineNumber = std::stoi(current + 1);
      lldbBreakpoint = context->debug_context.target.
        BreakpointCreateByLocation(filename, lineNumber);
      breakpointWasCreated = true;
      requestedLineNumber = lineNumber;
      break;
    }
    muntrace();
    current++;
  }

  if (!breakpointWasCreated) {
    assert(false);
    //lldbBreakpoint = context->debug_context.target.BreakpointCreateByName
     //(context->active_filename, requestedLineNumber);
  } 

   if (lldbBreakpoint.IsValid()) {
    BreakpointInfo& bpInfo = bpList.breakpoints[bpList.breakpointCount];
    lldb::SBBreakpointLocation bpLocation = lldbBreakpoint.GetLocationAtIndex(0);
    lldb::SBAddress addr = bpLocation.GetAddress();
    lldb::SBLineEntry line_entry = addr.GetLineEntry();
    lldb::SBFileSpec file_spec = line_entry.GetFileSpec();
    uint32_t line_number = line_entry.GetLine();
    if (line_number != requestedLineNumber) {
      bpInfo.lldbBreakpoint.ClearAllBreakpointSites();
      context->debug_context.target.BreakpointDelete(bpInfo.lldbBreakpoint.GetID());
      log_info("could not created breakpoint at line %u", requestedLineNumber);
    } else {
      bpList.breakpoints[bpList.breakpointCount].lldbBreakpoint = lldbBreakpoint;
      bpList.breakpoints[bpList.breakpointCount].lineNumber = line_number;
      bpList.breakpointCount++;
      log_info("created breakpoint at lineNumber %u", line_number);
    }
  } else {
    log_info("breakpoint is invalid\n");
  }
}

static inline
bool isNewline(const char *str) { 
  if (*str == '\n') return true;
  return false;
}

static void
DrawConsole(GUIContext *context, const Panel& panel, const PanelStyle& style) {
  const Console& console = GetConsole();
  uint32_t verticalDisplacementPerLine = style.lineSpacing + style.fontSize; 
  uint32_t estimatedMaxCharsPerLine = (panel.w / style.fontSize) + 10;
  char bufferToDraw[estimatedMaxCharsPerLine];
  for (uint32_t i = 0; i < console.entryCount; i++) {
    const ConsoleEntry& entry = console.entries[i];
    char *write = bufferToDraw;
    memset(bufferToDraw, 0, estimatedMaxCharsPerLine);
    if (entry.type == ConsoleEntryType_DEBUGGER) {
      memcpy_literal_and_increment_dest(write, "[UDB] ");
    } else if (entry.type == ConsoleEntryType_APPLICATION) {
      memcpy_literal_and_increment_dest(write, "[App] ");
    }

    //TODO(Torin) Add filtering of console entry types
    //Draw each line of the current entry
    //TODO(Torin) Consider newlines inside of the entry 
    const char *entryText = console.entries[i].text;
    uint32_t entryLength = console.entries[i].length;
    memcpy_and_increment_dest(write, entryText, entryLength);
    
    SDL_Color sdlColor = { style.fontColor.r, style.fontColor.g, style.fontColor.b, style.fontColor.a };
    SDL_Surface *text_surface = TTF_RenderText_Blended(style.font, bufferToDraw, sdlColor); 
    
    SDL_Rect text_rect = {
      (int)(style.border_size + style.pad_left + panel.x),
      (int)((i * verticalDisplacementPerLine) + 
          style.border_size + (int)style.pad_top + panel.y),
      text_surface->w,
      text_surface->h
    };

    SDL_Texture *text_texture = 
      SDL_CreateTextureFromSurface(context->renderer, text_surface);
    SDL_RenderCopy(context->renderer, text_texture, NULL, &text_rect);
    SDL_DestroyTexture(text_texture);
    SDL_FreeSurface(text_surface);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
      printf("debugger must be run with an argument\n");
      return 1;
  }


  enum CommandType {
    CommandType_SET_BREAKPOINT 
  };

  struct Command {
    CommandType type;
    const char *arg;
  };

  Command commands[argc];
  uint32_t commandCount = 0;
  uint32_t executableNameIndex = 0;
  for (int i = 1; i < argc; i++) {
    const char *current = argv[i];
    if (*current == '-') {
      current++;
      if (*current == 'b') {
        current++;
        while(isspace(*current)) {
          current++;
        }
        commands[commandCount].arg = current;
        commands[commandCount].type = CommandType_SET_BREAKPOINT;
        commandCount++;
      } else {
        log_error("Invalid command line paramater");
        return 1;
      }
    } else {
      executableNameIndex = i;
      break;
    }
  }
  
   
  const char *executable_path = argv[executableNameIndex]; 
  FILE *executableFileHandle = fopen(executable_path, "rb");
  if (executableFileHandle == NULL) {
    printf("[ERROR] Could not find the provide executable filepath\n");
    return 1;
  }
 
  const char *executable_arguments = NULL;
  if (executableNameIndex + 1 < (uint32_t)argc) {
    executable_arguments = argv[executableNameIndex + 1];
  }

  GUIContext* ctx = (GUIContext *)malloc(sizeof(GUIContext));
  GUIContext& context = *ctx;

  gui_initalize(&context);
  lldb_initialize(&context.debug_context, executable_path);
  ctx->functionNameList = CreateTypeNameList(lldb::eTypeClassFunction, 
    &ctx->functionNameListCount, ctx->debug_context.target);

  bool wasBreakpointSet = false;
  for (uint32_t i = 0; i < commandCount; i++) {
    Command& command = commands[i];
    if (command.type == CommandType_SET_BREAKPOINT) {
      CreateBreakpoint(&context, command.arg);
      wasBreakpointSet = true;
    }
  } 

  if (!wasBreakpointSet) {
    lldb_create_breakpoint(&context.debug_context, "main");
  }

  lldb_run_executable(&context.debug_context, &executable_arguments);
  
  while (context.is_running) {
    gui_update_input(&context);
    internal_lldb_process_events(&context);
    gui_process_input(&context);

    ProcessPanelBasedInput(&context);

    if (context.requires_refresh) {
      ExpressionList* list = &context.expressionList;
      for (uint32_t i = 0; i < list->count; i++) {
        Expression* expr = list->expressions[i];
        UpdateRootExpression(expr, &context.debug_context, true);
      }
    }

#if 0
    static char test[1024];
    size_t bytesWritten = context.debug_context.process.GetSTDOUT(test, 1024);
    if (bytesWritten) {
      printf("bytesWritten %lu\n", bytesWritten);
    }
    
    if (test[0]) {
      Console& console = GetConsole();
      const char *textWritten = AppendToStringBuffer(test, bytesWritten, &console.buffer);
      const char *current = textWritten;
      while ((current - textWritten) < bytesWritten) {
        const char *entryText = current;
        while (*current != 0) {
          current++;
        }
        size_t entryLength = current - entryText;
        ConsoleEntry& entry = console.entries[console.entryCount];
        entry.text = entryText;
        entry.length = entryLength;
        console.entryCount++;
        current++;
      }
    }
#endif

            
        InputState *input = &context.input;
        char temp[1024];
        memcpy(temp, context.identUnderCursor, 1024);

        if (gui_get_ident_under_point(&context, input->mouse_x, input->mouse_y,
              context.identUnderCursor, ARRAYCOUNT(context.identUnderCursor))) {
          if (strcmp(temp, context.identUnderCursor) || context.requires_refresh) {
            ClearStringBuffer(&context.mouseOverStringBuffer);
            PrintInfo info = lldb_print_identifier(&context.debug_context, context.identUnderCursor);
            context.tooltip_is_compound = info.value_count > 1;
            for (uint32_t i = 0; i < info.value_count; i++) {
              PrintValue& value = info.values[i];
              AppendToStringBuffer(value.type_string, strlen(value.type_string), &context.mouseOverStringBuffer);
              AppendLiteralToStringBuffer(" ", &context.mouseOverStringBuffer);
              AppendToStringBuffer(value.name_string, strlen(value.name_string), &context.mouseOverStringBuffer);
              AppendLiteralToStringBuffer(" = ", &context.mouseOverStringBuffer);
              AppendToStringBuffer(value.value_string, strlen(value.value_string), &context.mouseOverStringBuffer);
              AppendCharToStringBuffer('\0', &context.mouseOverStringBuffer);
            }
            AppendCharToStringBuffer('\0', &context.mouseOverStringBuffer);
          }


        } else if (context.mouseOverStringBuffer.used != 0) {
          ClearStringBuffer(&context.mouseOverStringBuffer);
        }

        { //Draw the main buffer
          SDL_Rect screen_rect = { 0, 0, (int)context.screen_width, (int)context.screen_height };
          SDL_RenderSetViewport(context.renderer, &screen_rect);
          SDL_SetRenderDrawColor(context.renderer, 0, 0, 0, 0);
          SDL_RenderClear(context.renderer);
          if (context.active_filedata)
            gui_render_buffer(&context);
        }

            gui_draw_panel(&context, &context.output_panel, context.output_style);
            DrawConsole(&context, context.output_panel, context.output_style);

            gui_draw_panel(&context, &context.expressionPanel, 
                context.expressionPanelStyle);
            DrawExpressionList(&context, &context.expressionList, context.expressionPanel, 
                context.expressionPanelStyle, context.syntaxStyle);

            //@Draw the @mouseOver buffer
            if (context.mouseOverStringBuffer.memory[0] != 0) {

              //Determine the required size of the panel
              uint32_t totalHeight = 0;
              uint32_t maxWidth = 0;
              {
                const char *read_pos = context.mouseOverStringBuffer.memory;
                while (read_pos[0] != 0) {
                  const char *text = read_pos;
                  while (*read_pos != 0) { 
                    read_pos++;
                  }
                  int width, height;
                  TTF_SizeText(context.font, text, &width, &height);
                  maxWidth = max(width, maxWidth);
                  totalHeight += context.line_spacing + context.font_size; 
                  read_pos++;
                }
              }


              const PanelStyle& style = context.mouseOverPanelStyle;
              Panel *panel = &context.tooltip_panel;
              
              panel->x = input->mouse_x + 20;
              panel->y = input->mouse_y + 20;
              panel->w = maxWidth + (style.border_size * 4) + style.pad_left + style.pad_right; //TODO(Torin) Why is this slightly too small
              panel->h = totalHeight + (style.border_size * 2) + style.pad_top + style.pad_bottom;
              gui_draw_panel(&context, panel, context.mouseOverPanelStyle);

              char *read_pos = context.mouseOverStringBuffer.memory;
              int ypos = (int)(panel->y + style.border_size + style.pad_top);
              int xoffset = 4;
              while(read_pos[0] != 0) {
                char *text = read_pos;
                while(*read_pos != 0) 
                  read_pos++;
                size_t text_length = read_pos - text;
                
                SDL_Color color = { 255, 255, 255, 255 };
                SDL_Surface *text_surface = TTF_RenderText_Blended(context.font, text, color);
                SDL_Texture *text_texture = SDL_CreateTextureFromSurface(context.renderer, text_surface);
                SDL_Rect dest_rect = { (int)(panel->x + style.border_size + style.pad_left) + xoffset, ypos, text_surface->w, text_surface->h }; 
                SDL_RenderCopy(context.renderer, text_texture, NULL, &dest_rect);
                SDL_DestroyTexture(text_texture);
                SDL_FreeSurface(text_surface);
                ypos += context.line_spacing + context.font_size;
                read_pos++;
              }
            }

#if 1 
            if (context.identUnderCursor[0] != 0) {
              SDL_Color color = { 255, 255, 255, 255 };
              SDL_Surface *text_surface = TTF_RenderText_Blended(context.font, context.identUnderCursor, color);
              SDL_Texture *text_texture = SDL_CreateTextureFromSurface(context.renderer, text_surface);
              SDL_Rect dest_rect = { 640, 360, text_surface->w, text_surface->h }; 
              SDL_RenderCopy(context.renderer, text_texture, NULL, &dest_rect);
              SDL_DestroyTexture(text_texture);
              SDL_FreeSurface(text_surface);
            }
#endif



            static bool isSearchVisible = false;
            if (context.controls.showFuzzySearch) {
              isSearchVisible = true;
            }

            if (isSearchVisible) {
              //GUIShowFuzzySearch(&context);
              isSearchVisible = false;
            }
            
            //glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
            //glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            //glClear(GL_COLOR_BUFFER_BIT);




            //ImGui_ImplSdl_NewFrame(ctx->window);
            //ImGui::ShowTestWindow();
            //ImGui::Render();
            SDL_RenderPresent(context.renderer);
            


            bool isRemoveDown = context.controls.isRemoveDown;
            memset(&context.controls, 0, sizeof(Controls));
            context.controls.isRemoveDown = isRemoveDown;

            
            

            context.requires_refresh = false;
      }

      TTF_Quit();
      SDL_Quit();
      return 0;
  }
