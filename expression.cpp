static void 
UpdateRootExpression(Expression* expr, DebugContext* debug, bool forceUpdate = false) 
{
  lldb::SBExpressionOptions options; 
  options.SetFetchDynamicValue(lldb::eNoDynamicValues);
  options.SetUnwindOnError(true);
  assert(expr->depth == 0 && expr->name != NULL);
  const char *expressionString = expr->name;

  static void 
  (*UpdateSubWatchExpression)(Expression* expr, lldb::SBValue& value) = 
  [](Expression* expr, lldb::SBValue& value)
  {
    expr->name = value.GetName();

    if (value.GetType().IsArrayType()) {
      lldb::SBType arrayType = value.GetType().GetArrayElementType();
      if (strcmp(arrayType.GetName(), "const char *") == 0 ||
            strcmp(arrayType.GetName(), "char *") == 0 ) {
        value.SetFormat(lldb::eFormatCharArray);
      }
    }

    expr->type = value.GetTypeName();
    expr->value = value.GetValue();

    if (expr->name == NULL)
      expr->name = "INVALID NAME";
    if (expr->type == NULL)
      expr->type = "INVALID TYPE";
    if (expr->value == NULL)
      expr->value = "INVALID VALUE";

    for (uint32_t i = 0; i < expr->childCount; i++) {
      Expression *childExpr = &expr->children[i];
      lldb::SBValue childValue = value.GetChildAtIndex(i);
      UpdateSubWatchExpression(childExpr, childValue);
      childValue.Clear();
    }
  };


  lldb::SBValue value = debug->target.EvaluateExpression(expr->name, options);
  UpdateSubWatchExpression(expr, value);
  expr->name = expressionString;
}

static uint32_t 
GetExpressionEffectiveChildCount(lldb::SBValue value) {
  uint32_t result = 0;
  if (value.GetType().IsArrayType()) {
    lldb::SBType arrayType = value.GetType().GetArrayElementType();
    if (strcmp(arrayType.GetName(), "const char *") == 0 ||
          strcmp(arrayType.GetName(), "char *") == 0 ) {
      return result;
    }
    return result;
  }

  result = value.GetNumChildren();
  return result;
}

static void GetExpressionRecursiveEffectiveChildCount(
uint32_t *result, lldb::SBValue& value) 
{
  for (uint32_t i = 0; i < value.GetNumChildren(); i++) {
    *result += GetExpressionEffectiveChildCount(value.GetChildAtIndex(i));
  }
}

static void
RecursivelyInitChildExpressions(Expression *expr, lldb::SBValue value, 
uint32_t currentIndex, uint32_t depth, Expression* rootExpression) 
{
  expr->childCount = GetExpressionEffectiveChildCount(value);
  expr->depth = depth;
  expr->isExpanded = true;
  
  uint32_t childFillIndex = currentIndex + 1;
  uint32_t subChildrenHandled = 0;
  for (uint32_t i = 0; i < expr->childCount; i++) {
    lldb::SBValue childValue = value.GetChildAtIndex(i); 
    Expression& childExpr = expr->children[i];
    childFillIndex = childFillIndex + expr->childCount;
    subChildrenHandled += childValue.GetNumChildren();
    childExpr.children = rootExpression + childFillIndex;
    RecursivelyInitChildExpressions(&childExpr, childValue, 
      currentIndex + 1, depth + 1, rootExpression);
  }
}

static void 
CreateWatchExpression(ExpressionList *exprList, 
DebugContext* debug, const char *exprString) 
{
  assert(exprString != NULL && strlen(exprString) > 0);
  assert(exprList->count + 1 < ARRAYCOUNT(exprList->expressions));

  lldb::SBValue rootValue = debug->target.EvaluateExpression(exprString);
  uint32_t totalChildCount = rootValue.GetNumChildren();
  GetExpressionRecursiveEffectiveChildCount(&totalChildCount, rootValue);

  size_t totalExpressionMemorySize = (totalChildCount + 1) * sizeof(Expression);
  size_t expressionStringLength = strlen(exprString);
  size_t requiredExpressionStringSize = expressionStringLength + 1;
  
  Expression *rootExpression = (Expression *)
    malloc(totalExpressionMemorySize + requiredExpressionStringSize);
  memset(rootExpression, 0, totalExpressionMemorySize + requiredExpressionStringSize);
  char *rootExpressionString = (char *)(rootExpression + (totalChildCount + 1));
  memcpy(rootExpressionString, exprString, expressionStringLength);
 
  rootExpression->children = rootExpression + 1;
  RecursivelyInitChildExpressions(rootExpression, rootValue, 0, 0, rootExpression);
  rootExpression->name = rootExpressionString;
  UpdateRootExpression(rootExpression, debug, true);

  assert(exprList->count + 1 < ARRAYCOUNT(exprList->expressions));
  exprList->expressions[exprList->count] = rootExpression;
  exprList->count++;
  log_debug("watch-expression added: %s", exprString);
}

static void
DestroyWatchExpression(uint32_t expressionIndex, ExpressionList *expressionList)
{
  assert(expressionList->count > expressionIndex);
  Expression *expressionToFree = expressionList->expressions[expressionIndex];
  free(expressionToFree);
  for (uint32_t i = expressionIndex; i < expressionList->count - 1; i++)
    expressionList->expressions[i] = expressionList->expressions[i + 1];
  expressionList->expressions[expressionList->count - 1] = nullptr;
  expressionList->count--;
}

static inline
uint32_t GetTotalExpressionCount(const Expression* expr) 
{
  uint32_t result = 1;
  for (uint32_t i = 0; i < expr->childCount; i++) {
    Expression *child = &expr->children[i];
    result += GetTotalExpressionCount(child);
  }
  return result;
}



