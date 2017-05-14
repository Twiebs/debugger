#include "imgui.cpp"
#include "imgui_draw.cpp"
#include "imgui_impl_sdl.cpp"
#include "imgui_demo.cpp"

struct TextDrawState {
  uint32_t currentXOffset;
  uint32_t currentYOffset;
};


//TODO(Torin) Make a new TextBuffer struct to seperated out the 
//line information from the rest of the guicontext

static inline
uint32_t GetRequiredLineNumberRectWidth(GUIContext *context) {
  uint32_t currentNumber = 1;
  uint32_t digitCount = 0;
  while ((currentNumber - 1) < context->line_count) {
    currentNumber *= 10;
    digitCount += 1;
  }

  uint32_t result = digitCount * context->text_style.fontSize; 
  return result;
}

static inline
SDL_Color RGBA8ToSDLColor(const RGBA8& color) {
  SDL_Color result;
  result.r = color.r;
  result.g = color.g;
  result.b = color.b;
  result.a = color.a;
  return result;
}

static inline
void gui_highlight_line(GUIContext* context, uint32_t lineNumber, RGBA8 color) {
  if (lineNumber < context->top_line_in_panel || 
      lineNumber > context->top_line_in_panel + context->max_lines_in_panel) {
    return;
  }

  Panel *panel = &context->text_panel;
  const PanelStyle& style = context->text_style;
  int line_height = context->line_spacing + context->font_size;
  int screenPosY = (line_height * (lineNumber - context->top_line_in_panel)) + 
    style.pad_top + style.border_size + 2;

  SDL_Rect highlight_rect = {
    (int)(panel->x + style.border_size),
    screenPosY,
    (int)(panel->w - style.border_size),
    line_height
  };

  SDL_SetRenderDrawColor(context->renderer, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(context->renderer, &highlight_rect);
}



void DrawCStrText(GUIContext *ctx, const char *text, const Panel& panel, 
const PanelStyle& style, const RGBA8& color, TextDrawState *state) 
{
  assert(text != NULL);
  SDL_Color textColor = RGBA8ToSDLColor(color); 
  SDL_Color backgroundColor = RGBA8ToSDLColor(style.color_background);

  SDL_Surface *textSurface = TTF_RenderText_Blended(style.font, text, textColor);
  SDL_Texture *textTexture = SDL_CreateTextureFromSurface(ctx->renderer, textSurface);

  SDL_Rect textRect = {
    (int)(panel.x + style.pad_left + state->currentXOffset),
    (int)(panel.y + style.pad_top + state->currentYOffset),
    textSurface->w,
    textSurface->h,
  };

  SDL_RenderCopy(ctx->renderer, textTexture, NULL, &textRect);
  SDL_DestroyTexture(textTexture);
  SDL_FreeSurface(textSurface);
  state->currentXOffset += textSurface->w;
}

inline void 
DrawCStrText(GUIContext* ctx, const char *text, const Panel& panel, 
const PanelStyle& style,  TextDrawState *state) 
{
  DrawCStrText(ctx, text, panel, style, style.fontColor, state);
}

void NextRow(TextDrawState *drawState, const PanelStyle& style) {
  drawState->currentYOffset += style.fontSize + style.lineSpacing;
  drawState->currentXOffset = 0;
}

static inline
void gui_draw_panel(GUIContext* context, Panel *panel, const PanelStyle& style)
{
  SDL_Rect panel_border_rect = {
    (int)(panel->x),
    (int)(panel->y),
    (int)(panel->w),
    (int)(panel->h),
  };

  SDL_Rect panel_background_rect = {
    (int)(panel->x + style.border_size),
    (int)(panel->y + style.border_size),
    (int)(panel->w - (style.border_size * 2)),
    (int)(panel->h - (style.border_size * 2)),
  };

#if 0 
  SDL_Rect panel_content_rect = {
    (int)(panel->x + style.pad_left + style.border_size),
    (int)(panel->y + style.pad_bottom + style.border_size),
    (int)(panel->w - style.pad_right - style.pad_left - (style.border_size * 2)),
    (int)(panel->h - style.pad_top   - style.pad_bottom - (style.border_size * 2)),
  };
#endif


  SDL_SetRenderDrawColor(context->renderer,
   style.color_border.r,
   style.color_border.g,
   style.color_border.b,
   style.color_border.a
  );
  SDL_RenderFillRect(context->renderer, &panel_border_rect);
    
  if (style.drawLineNumbers) {
    SDL_Rect lineNumberContainer = {
      (int)(panel->x + style.border_size), 
      (int)(panel->y + style.border_size),
      (int)(GetRequiredLineNumberRectWidth(context)),
      (int)(panel->h - (style.border_size * 2)),
    };

    panel_background_rect.x += lineNumberContainer.w;
    panel_background_rect.w -= lineNumberContainer.w;

    //TODO(Torin) This will be bad with lightColors
    SDL_SetRenderDrawColor(context->renderer,
       style.color_background.r + 35,
       style.color_background.g + 35,
       style.color_background.b + 35,
       255
   );
    SDL_RenderFillRect(context->renderer, &lineNumberContainer);
  }

    SDL_SetRenderDrawColor(context->renderer,
     style.color_background.r,
     style.color_background.g,
     style.color_background.b,
     255
   );
    SDL_RenderFillRect(context->renderer, &panel_background_rect);
}

static void 
DrawExpression(GUIContext *ctx, Expression* expr, 
const Panel& panel, const PanelStyle& panelStyle, 
const SyntaxStyle& syntaxStyle, TextDrawState *drawState)
{
  for (uint32_t i = 0; i < expr->depth; i++) {
    DrawCStrText(ctx, "  ", panel, panelStyle, syntaxStyle.identColor, drawState);
  }

  DrawCStrText(ctx, expr->type, panel, panelStyle, syntaxStyle.typeColor, drawState);
  DrawCStrText(ctx," ", panel, panelStyle, syntaxStyle.identColor, drawState);
  DrawCStrText(ctx, expr->name, panel, panelStyle, syntaxStyle.identColor, drawState);

  if (expr->childCount > 0) {
    NextRow(drawState, panelStyle);
    if (expr->isExpanded) {
      for (uint32_t i = 0; i < expr->childCount; i++) {
        Expression* child = &expr->children[i];
        DrawExpression(ctx, child, panel, panelStyle, syntaxStyle, drawState);
      }
    }
  } else {
    DrawCStrText(ctx, " ", panel, panelStyle, syntaxStyle.identColor, drawState);
    DrawCStrText(ctx, expr->value, panel, panelStyle, syntaxStyle.valueColor, drawState);
    NextRow(drawState, panelStyle);
  }
}

static inline
void DrawExpressionList(GUIContext* ctx, ExpressionList *list, const Panel& panel, 
const PanelStyle& panelStyle, const SyntaxStyle& syntaxStyle)
{
  TextDrawState drawState = {};
  for (uint32_t i = 0; i < list->count; i++) {
    Expression *expr = list->expressions[i];
    DrawExpression(ctx, expr, panel, panelStyle, syntaxStyle, &drawState);
  }
}


