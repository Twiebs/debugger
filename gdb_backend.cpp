#define literal_strlen(s) (sizeof(s) - 1)
#define write_literal(fd,s) write(fd, s, literal_strlen(s))
#define check_errors(proc) if (proc) printf("ERROR: call failed " #proc "\n")

int string_equals(const char *stringA, size_t lengthA, const char *stringB, size_t lengthB)
{
    if (lengthA != lengthB) return 0;
    for (size_t i = 0; i < lengthA; i++)
    {
        if (stringA[i] != stringB[i]) return 0;
    }
    return 1;
}

#define matches_literal(source, literal) string_matches(literal, literal_strlen(literal), source)

int string_matches(const char *match, size_t match_length, const char *target) 
{
    for (size_t i = 0; i < match_length; i++)
    {
        if (match[i] != target[i]) return 0;
    }
    return 1;
}

static inline
int string_to_int(const char *str, size_t length)
{
  int result = 0;
  for (size_t i = 0; i < length; i++)
  {
    char digit_value = str[i] - '0';
    result += pow10((length-1)-i) * digit_value; 
  }
  return result;
}

struct GDBParser
{
  char *current;
};

struct GDBContext
{
    int output_pipe;
    int input_pipe;
    char input_buffer[1024*1024];
    ssize_t input_buffer_size;
    GDBParser parser;
};

enum GDBEventType
{
  GDB_EVENT_NONE,
  GDB_EVENT_STOPPED,
  GDB_EXECUTION_FINISHED,
  GDB_PRINT_INFO,
  GDB_EVENT_UNKNOWN
};

struct GDBStoppedInfo
{
    char *filename;
    size_t filename_length;
    uint32_t line_number;
};

enum GDBValueType
{
    GDB_VARIABLE,
    GDB_VALUE,
};

struct GDBValue
{
    GDBValueType type;
    union
    {
        int64_t int_value;
        double float_value;
        uint64_t subvalue_count;
        struct
        {
            char *string_value;
            size_t string_length;
        };
    };
};

struct GDBPrintValue
{
    
    GDBValueType type;
    char *text;
    uint32_t text_length;
};

struct GDBPrintInfo
{
    GDBPrintValue values[1024];
    uint32_t value_count;
};

struct GDBEvent
{
    GDBEventType type;
    union
    {
        GDBStoppedInfo stopped_info;
        GDBPrintInfo print_info;
    };
};

static inline
void gdb_initalize(GDBContext *context)
{
    int to_child[2], from_child[2]; 
    pipe2(to_child,   O_NONBLOCK);
    pipe2(from_child, O_NONBLOCK);
    pid_t cpid = fork();
    if (cpid == 0)
    {
        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        if (execl("/usr/bin/gdb", "/usr/bin/gdb", "--interpreter=mi2", 0))
        {
            printf("failed to spawn gdb process!\n");
        }
    }
    else
    {
        close(to_child[0]);
        close(from_child[1]);
        context->output_pipe = to_child[1];
        context->input_pipe = from_child[0];
        write_literal(to_child[1], "file test\n");
        write_literal(to_child[1], "start\n"); 
    }
}

#define gdb_extract_uint(variable_name, output)                                \
{                                                                              \
    while (!(matches_literal(parser->current, variable_name)))                 \
      parser->current++;                                                       \
    parser->current += literal_strlen(variable_name "=\"");                    \
    char *value_text = parser->current;                                        \
    while (*parser->current != '"')                                            \
      parser->current++;                                                       \
    size_t value_length = (parser->current - value_text);                      \
    output = string_to_int(value_text, value_length);                          \
    parser->current += literal_strlen("\"");                                   \
}

static GDBStoppedInfo
gdb_parse_stopped_event(GDBParser *parser)
{
    GDBStoppedInfo info = {}; 
    
#if 0 
    assert(matches_literal(parser->current, "stopped"));
    parser->current += literal_strlen("stopped");
#endif

    while (!(matches_literal(parser->current, "file")))
        parser->current++;

    parser->current += literal_strlen("file=\"");
    info.filename = parser->current;
    while (*parser->current != '"') parser->current++;
    info.filename_length= (parser->current - info.filename);
    parser->current += literal_strlen("\"");

    gdb_extract_uint("line", info.line_number);

    return info;
}
static int 
gdb_parse_output(GDBContext *gdb, GDBEvent *event)
{
    GDBParser *parser = &gdb->parser;
    
    *event = {};
    
    if (gdb->input_buffer_size > 0)
    {
        if (*gdb->parser.current == 0)
        {
            gdb->input_buffer_size = 0;
            return 0;
        }
        
        else if (*gdb->parser.current == '*')
        {
            gdb->parser.current++;           
            
	    if (matches_literal(gdb->parser.current, "stopped")) 
	    {
	      gdb->parser.current += literal_strlen("stopped,reason=\"");
	      if (matches_literal(gdb->parser.current, "exited"))
	      {
		event->type = GDB_EXECUTION_FINISHED;
	      }
	      else
	      {
		event->stopped_info = gdb_parse_stopped_event(&gdb->parser);
                event->type = GDB_EVENT_STOPPED;
	      }
            }
        }

        else if (*gdb->parser.current == '~')
        {
	    parser->current++;
            if (matches_literal(gdb->parser.current, "\"$"))
            {
                event->type = GDB_PRINT_INFO;
                GDBPrintInfo& info = event->print_info;
                parser->current += 2;
                while(isdigit(*parser->current)) parser->current++;
                assert(matches_literal(parser->current, " = "));
                parser->current += 3;

                if(*parser->current == '{')
                {
                    std::function<void()> parse_print_value =
                        [&parse_print_value, &info, &parser]()
                    {
                        GDBPrintValue& value = info.values[info.value_count++];
                        value.type = GDB_VARIABLE;
                        value.text = parser->current;
                        while(!isspace(*parser->current)) parser->current++;
                        value.text_length = parser->current - value.text;
                        assert(matches_literal(parser->current, " = "));
                        parser->current += 3;

                        if (*parser->current == '{')
                        {
                            parser->current++;
                            parse_print_value();
                            while(1)
                            {
                                while(*parser->current != ',' &&
                                      *parser->current != '}')
                                    parser->current++;

                                if (*parser->current == '}') break;
				assert(matches_literal(parser->current, ", "));
				parser->current+=2;
				
                                parse_print_value();
                            }
                        }
                        else
                        {
                            GDBPrintValue& value = info.values[info.value_count++];
                            value.type = GDB_VALUE;
                            value.text = parser->current;
                            while ((!isspace(*parser->current)) && *parser->current != '"')parser->current++;
                            value.text_length = parser->current - value.text;
                        }

                    };
                    parser->current++;
                    parse_print_value();
                }
                
                else
                {
                    GDBPrintValue& value = info.values[info.value_count++];
                    value.type = GDB_VALUE;
                    value.text = parser->current;
                    while (!isspace(*parser->current) && *parser->current != '"') parser->current++;
                    value.text_length = parser->current - value.text;
                }
            }
        }

        
        gdb->parser.current++;
        return 1;       
    }
    else
    {   
      gdb->input_buffer_size = read(gdb->input_pipe, gdb->input_buffer, ARRAYCOUNT(gdb->input_buffer) - 1);
      if (gdb->input_buffer_size == -1) gdb->input_buffer_size = 0;
      assert((size_t)gdb->input_buffer_size < ARRAYCOUNT(gdb->input_buffer));
      gdb->input_buffer[gdb->input_buffer_size] = 0;
      gdb->parser.current = gdb->input_buffer;
      if (gdb->input_buffer_size > 0) return 1;
    }
    return 0;
}



#if 0
  static GDBMIValue
      parse_value(GDBMIParser *parser)
  {
      if (*parser->current == '[')
      {
          parser->current++;
          parse_value(parser);
          while (*parser->current == ',')
          {
              parser->current++;
              parse_value(parser);
          }
        
          assert(*parser->current == ']');
          parser->current++;
      }
      else if (*parser->current == '{')
      {
          parser->current++;
          parse_value(parser);
          assert(*parser->current == '}');
          parser->current++;
#if 0
          while(*parser->current == ',')
          {
              parser->current++;
              parser->current = parse_value(current);
          }   
#endif
      }
      else if (*parser->current == '"')
      {
          parser->current++;
          char *value = parser->current;
          size_t value_length;
          while (*parser->current != '"') parser->current++;
          value_length = (parser->current - value);
          parser->current++;//eat the '"'

          //write(1, value, value_length);
      }
      else
      {
          assert(*parser->current != ',');
          parse_variable(parser);
          while (*parser->current == ',')
          {
              parser->current += 1; //eat the ','
              parse_value(parser);
          }
      }

  }

  static char * 
      parse_variable(GDBMIParser *parser)
  {
      char *name = parser->current;
      size_t name_length;
      while (*parser->current != '=') parser->current++;
      name_length = parser->current - name;
      parser->current++;
      parse_value(parser);

      if (matches_literal(name, "file"))
      {
          write(1, name, name_length);
          write_literal(1, "=");
//      write(1, value
        


      }

      return parser->current;
  }


#if 0   
  static char *
      parse_value(char *current)
  {
      if (*current == '[')
      {
          current++;
          current = parse_value(current);
          while (*current == ',')
          {
              current++;
              current = parse_value(current);
          }
        
          assert(*current == ']');
          current++;
      }
      else if (*current == '{')
      {
          current++;
          current = parse_value(current);
          assert(*current == '}');
          current++;
#if 0
          while(*current == ',')
          {
              current++;
              current = parse_value(current);
          }   
#endif
      }
      else if (*current == '"')
      {
          current = current + 1; //eat the '"'
          char *value = current;
          size_t value_length;
          while (*current != '"') current++;
          value_length = (current - value);
          current = current + 1; //eat the '"'

          write(1, value, value_length);
      }
      else
      {
          assert(*current != ',');
          current = parse_variable(current);
          while (*current == ',')
          {
              current += 1; //eat the ','
              current = parse_variable(current);
          }
      }

      return current;
  }

#endif
#endif

