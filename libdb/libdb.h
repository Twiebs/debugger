#ifndef LIBDB_INCLUDE_GUARD
#define LIBDB_INCLUDE_GUARD
#include <stdint.h>

typedef struct {
  const char *functionNames;
  uintptr_t *functionAddresses;
  const char *fileNames;
  uint64_t functionCount;
  uint64_t fileCount;
} libdb_Symbol_Table;

typedef enum {
  libdb_Program_State_INVALID,
  libdb_Program_State_UNSTARTED,
  libdb_Program_State_RUNNING,
  libdb_Program_State_STOPPED,
  libdb_Program_State_EXITED,
} libdb_Program_State;

typedef enum {
  libdb_Stop_Reason_NONE,
  libdb_Stop_Reason_BREAKPOINT_HIT,
} libdb_Stop_Reason;

typedef struct {
  libdb_Symbol_Table symbol_table;
  int32_t pid;

  libdb_Program_State state;
  libdb_Stop_Reason stop_reason;
  uint64_t rip;
  int64_t breakpoint_id;
} libdb_Program;

int32_t libdb_program_open(const char *executable_path, libdb_Program *program);
int32_t libdb_program_update_state(libdb_Program *program);

int libdb_execution_continue(libdb_Program *program);
void libdb_exectuion_step_over(libdb_Program *program);
void libdb_execution_step_into(libdb_Program *program);
void libdb_exeuction_step_out(libdb_Program *program);

int64_t libdb_breakpoint_create_at_symbol(const char *symbol_name, libdb_Program *program);
int64_t libdb_breakpoint_create_at_location(const char *filename, int64_t line_number, libdb_Program *program);
void libdb_breakpoint_destroy(int64_t breakpoint_id);

#endif//LIBDB_INCLUDE_GUARD

#ifdef LIBDB_IMPLEMENTATION
#undef LIBDB_IMPLEMENTATION

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

#include <stdio.h>
#include <string.h>

#define R15	0
#define R14	1
#define R13	2
#define R12	3
#define RBP	4
#define RBX	5
#define R11	6
#define R10	7
#define R9	8
#define R8	9
#define RAX	10
#define RCX	11
#define RDX	12
#define RSI	13
#define RDI	14
#define ORIG_RAX 15
#define RIP	16
#define CS	17
#define EFLAGS	18
#define RSP	19
#define SS	20
#define FS_BASE 21
#define GS_BASE 22
#define DS	23
#define ES	24
#define FS	25
#define GS	26

#define LIBDB_SIGNAL_TRAP 5

#ifndef libdb_assert
#include <assert.h>
#define libdb_assert(expr) assert(expr)
#endif //libdb_assert
#ifndef libdb_log_error
#define libdb_log_error(...) printf(__VA_ARGS__); printf("\n")
#endif //libdb_log_error
#ifndef libdb_log_warning
#define libdb_log_warning(...) printf(__VA_ARGS__); printf("\n");
#endif //libdb_log_warning
#ifndef libdb_log_info
#define libdb_log_info(...) printf(__VA_ARGS__); printf("\n")
#endif //libdb_log_info
#ifndef libdb_log_debug
#define libdb_log_debug(...) printf(__VA_ARGS__); printf("\n")
#endif //libdb_log_debug

#ifndef libdb_malloc
#include <malloc.h>
#define libdb_malloc(size) malloc(size)
#endif//libdb_malloc
#ifndef libdb_free
#define libdb_free(ptr) free(ptr)
#endif//libdb_free

#define LIBDB_BREAKPOINT_CAPACITY 16

typedef struct {
  uint64_t instruction_address;
  uint64_t replaced_memory;
} libdb_Breakpoint;

typedef struct {
  uint64_t breakpoint_addresses[LIBDB_BREAKPOINT_CAPACITY];
} libdb_Internal;

static libdb_Breakpoint _breakpoints[16];
static uint32_t _breakpoint_count = 0;

#define ELF64_IMPLEMENTATION
#include "elf64.h"

static const char* libdb_SIGNAL_NAME_LIST[] = {
  "NULL SIGNAL",
  "SIGHUP",
  "SIGINT",
  "SIGQUIT",
  "SIGILL",
  "SIGTRAP",
  "SIGARBRT",
  "SIGBUS",
  "SIGFPE",
  "SIGKILL",
  "SIGUSR1",
  "SIGSEGV",
  "SIGUSR2",
  "SIGPIPE",
  "SIGALRM",
  "SIGTERM",
  "UNUSED",
  "SIGCHILD",
  "SIGCONT",
  "SIGSTOP",
  "SIGSTP",
  "SIGTTIN",
  "SIGTTOU",
  "SIGURG",
  "SIGXCPU",
  "SIGXFSZ",
  "SIGVTALRM",
  "SIGPROF",
  "SIGWINCH",
  "SIGIO",
  "SIGPWR",
  "SIGSYS",
};

uint64_t libdb_get_rip(libdb_Program *program) {
  uint64_t result = ptrace(PTRACE_PEEKUSER, program->pid, RIP * 8, 0);
  return result;
}

uint64_t libdb_get_instruction(libdb_Program *program, uint64_t address) {
  uint64_t result = ptrace(PTRACE_PEEKTEXT, program->pid, address);
  return result;

}

int libdb_execution_continue(libdb_Program *program) {
  libdb_assert(program->state != libdb_Program_State_RUNNING);

  if (program->breakpoint_id != -1) {
    libdb_Breakpoint *bp = &_breakpoints[program->breakpoint_id];
    libdb_assert(program->rip == bp->instruction_address);

    ptrace(PTRACE_POKETEXT, program->pid,
      bp->instruction_address, bp->replaced_memory);
    uint64_t new_instruction = 0;
    new_instruction = ptrace(PTRACE_PEEKTEXT, program->pid, program->rip);
    libdb_assert(new_instruction == bp->replaced_memory);

    ptrace(PTRACE_POKEUSER, program->pid, RIP * 8, bp->instruction_address);
    program->rip = ptrace(PTRACE_PEEKUSER, program->pid, RIP * 8);
    assert(program->rip == bp->instruction_address);
    libdb_log_debug("the new instruction to execute is 0x%lX", program->rip);

    int process_status = 0;
    ptrace(PTRACE_SINGLESTEP, program->pid, NULL, NULL);
    waitpid(program->pid, &process_status, 0);
    if (WIFSTOPPED(process_status)) {
      int stopSignalNumber = WSTOPSIG(process_status);
      if (stopSignalNumber == LIBDB_SIGNAL_TRAP) {
        program->rip = ptrace(PTRACE_PEEKUSER, program->pid, RIP * 8);
        libdb_log_debug("new rip is 0x%lX", program->rip);
        uint64_t new_data = (0xFFFFFFFFFFFFFF00 & bp->replaced_memory) | 0xCC;
        ptrace(PTRACE_POKETEXT, program->pid, bp->instruction_address, new_data);
        libdb_log_debug("we sucuessfuly single steped and restored the breakpoint");
      } else {
        libdb_log_error("the Process stoped after single step but it was not a trap signal!\n");
        libdb_assert(0);
      }
    } else {
      libdb_log_error("The process did not stop after single step!");
      libdb_assert(0);
    }
  }

  int process_status = 0;
  assert(program->pid > 0);
  ptrace(PTRACE_CONT, program->pid, NULL, NULL);

  if (program-> breakpoint_id != -1) {
    libdb_log_debug("continued exectuion from breakpoint %ld, "
        "now executing instruction 0x%lX",
        program->breakpoint_id, program->rip);
  } else if (program->rip != 0) {
    libdb_log_debug("continued exectuion from instruction 0x%lX", program->rip);
  } else {
    libdb_log_debug("started execution!");
  }

  program->state = libdb_Program_State_RUNNING;
  program->stop_reason = libdb_Stop_Reason_NONE;
  program->breakpoint_id = -1;
  program->rip = 0;
  return 1;
}


void libdb_exectuion_step_over(libdb_Program *program) {

}

void libdb_execution_step_into(libdb_Program *program) {

}

void libdb_exeuction_step_out(libdb_Program *program) {

}

int64_t libdb_breakpoint_create_at_symbol(const char *symbolName, libdb_Program *program)
{
  //TODO(Torin) Make sure the process is stoped here
  //for testing purposes this is ignored for now because breakpoints
  //are only created while the process is stopped
  libdb_assert(program->state = libdb_Program_State_STOPPED);

  libdb_Symbol_Table *symTable = &program->symbol_table;
  const char *currentFunctionName = symTable->functionNames;
  for (uint32_t i = 0; i < symTable->functionCount; i++) {

    if (strcmp(currentFunctionName, symbolName) == 0) {
      libdb_Breakpoint *breakpoint = &_breakpoints[_breakpoint_count];
      breakpoint->instruction_address = symTable->functionAddresses[i];

      breakpoint->replaced_memory = ptrace(PTRACE_PEEKTEXT, program->pid,
       breakpoint->instruction_address, NULL);
      ptrace(PTRACE_POKETEXT, program->pid, breakpoint->instruction_address, 0xCC);

      libdb_log_info("breakpoint-create: function %s 0x%lX",
          symbolName, breakpoint->instruction_address);
      int64_t breakpointID = _breakpoint_count;
      _breakpoint_count++;
      return breakpointID;
    }

    currentFunctionName += strlen(currentFunctionName) + 1;
  }

  libdb_log_error("breakpoint_create: failed to create breakpoint "
      "could not resolve symbol %s", symbolName);
  return 0;
}

int libdb_program_update_state(libdb_Program *program) {
  int childStatus = 0;
  int child_state_has_changed = waitpid(program->pid, &childStatus, WNOHANG);

  if (child_state_has_changed) {
    if (childStatus == -1) {
      libdb_log_error("error on waitpid for childProcess");
    } else if (childStatus != -1) {

      if (WIFEXITED(childStatus)) {
          int exitStatus = WEXITSTATUS(childStatus);
          libdb_log_info("the child terminated normaly with return code %d", exitStatus);
          kill(program->pid, SIGKILL);
          program->pid = 0;
          program->state = libdb_Program_State_EXITED;
          program->stop_reason = libdb_Stop_Reason_NONE;
        } else if (WIFSIGNALED(childStatus)) {
          int signalNumber = WTERMSIG(childStatus);
          libdb_log_info("the child recived signal %d", signalNumber);
        }

        else if (WIFSTOPPED(childStatus)) {
          int stopSignalNumber = WSTOPSIG(childStatus);
          if (stopSignalNumber == LIBDB_SIGNAL_TRAP) {
            libdb_log_debug("debug program recieved SIGTRAP");
            libdb_assert(_breakpoint_count > 0);

            program->state = libdb_Program_State_STOPPED;
            program->stop_reason = libdb_Stop_Reason_BREAKPOINT_HIT;
            program->breakpoint_id = -1;
            program->rip = ptrace(PTRACE_PEEKUSER, program->pid, 8 * RIP, NULL);
            program->rip -= 1; //rip points to the instruction after the int 3
            //libdb considers the active rip the instruction that was just executed

            for (uint32_t i = 0; i < _breakpoint_count; i++) {
              libdb_Breakpoint* bp = &_breakpoints[i];
              if (bp->instruction_address == program->rip) {
                libdb_log_debug("breakpoint-hit: index %d, rip 0x%lX", i, program->rip);
                program->breakpoint_id = i;
              }
            }

            if (program->breakpoint_id == -1) {
              assert(0);
            }
          }
        } else {
          libdb_log_debug("UNHANLDED STOP SIGNAL");
        }

      }
    return 1;
  }
  return 0;
}

size_t leb128_decode_uint64(uint8_t *input, uint64_t *result) {
  size_t bytes_consumed = 0;
  *result = 0;
  while (1) {
    *result |= ((input[bytes_consumed]  & 0x7F) << (7 * bytes_consumed));
    bytes_consumed++;
    if (!(input[bytes_consumed] & 0x80)) break;
  }
  return bytes_consumed;
}


int strings_match(const char *a, const char *b) {
  size_t index = 0;
  while (a[index] == b[index]) {
    if (a[index] == 0 || b[index] == 0) return 1;
    index++;
  }
  if (a[index] == 0 || b[index] == 0) return 1;
  return 0;
}

uint64_t dw_get_form_size(uint64_t form) {
  switch (form) {
    case DW_FORM_flag_present:
    case DW_FORM_ref1:
    case DW_FORM_data1:
    case DW_FORM_flag: {
      return 1;
    } break;

    case DW_FORM_ref2:
    case DW_FORM_data2: {
      return 2;
    } break;

    case DW_FORM_ref_addr:
    case DW_FORM_ref4:
    case DW_FORM_strp:
    case DW_FORM_data4:
    case DW_FORM_sec_offset: {
      return 4;
    } break;

    case DW_FORM_ref8:
    case DW_FORM_data8:
    case DW_FORM_addr: {
      return 8;
    } break;

    case DW_FORM_exprloc: {
      uint64_t length = 0;
    } break;


    default: {
      libdb_log_debug("Unhandled form: 0x%lX", form);
      libdb_assert(0);
    }
  }
  libdb_assert(0);
  return 0;
}


int libdb_program_open(const char* path, libdb_Program *program) {
  FILE *file_handle = fopen(path, "rb");
  if(file_handle == NULL){
    libdb_log_error("Could not find exectuable file %s when attempting to open program", path);
    return 0;
  }

  fseek(file_handle, 0, SEEK_END);
  size_t file_size = ftell(file_handle);
  fseek(file_handle, 0, SEEK_SET);
  uint8_t *fileData = (uint8_t *)libdb_malloc(file_size);
  fread(fileData, 1, file_size, file_handle);
  fclose(file_handle);

  ELF64Header* header = (ELF64Header*)fileData;
  if (header->magicNumber != ELF64_MAGIC_NUMBER) {
    libdb_log_error("executable file \"%s\" is not a valid ELF binary", path);
  }

  ELFSectionHeader *sectionStringTableSectionHeader = (ELFSectionHeader*)(fileData +
      header->sectionHeaderOffset + ((header->sectionStringTableSectionIndex)
        * header->sectionHeaderEntrySize));
  char *sectionStringTableData = (char *)(fileData +
      (sectionStringTableSectionHeader->fileOffsetOfSectionData));
  assert(*sectionStringTableData == 0); //First byte of the stringTableData is always null

  ELFSectionHeader* symbolTableHeader = NULL;
  ELFSectionHeader* stringTableHeader = NULL;
  char* symbolStringTableData = NULL;
  uint32_t symbolTableEntryCount = 0;

  ELFSectionHeader *debug_abbrev_section = 0;
  ELFSectionHeader *debug_info_section = 0;
  ELFSectionHeader *debug_line_section = 0;
  ELFSectionHeader *debug_str_section = 0;

  for (uint32_t i = 0; i < header->sectionHeaderEntryCount; i++) {
    ELFSectionHeader *sectionHeader = (ELFSectionHeader*)(fileData +
      header->sectionHeaderOffset + ((i+1) * header->sectionHeaderEntrySize));

    const char *sectionName = sectionStringTableData + sectionHeader->nameOffset;
    if (sectionHeader->sectionType == ELF_SECTION_TYPE_STRING_TABLE) {
      if (strcmp(sectionName, ".strtab") == 0) {
        stringTableHeader = sectionHeader;
        symbolStringTableData = (char*)(sectionHeader->fileOffsetOfSectionData + fileData);
      }
    } else if (sectionHeader->sectionType == ELF_SECTION_TYPE_SYMBOL_TABLE) {
      symbolTableHeader = sectionHeader;
      symbolTableEntryCount = symbolTableHeader->sectionSize /
        symbolTableHeader->sectionEntrySize;
    }

    else if (strings_match(sectionName, ".debug_")) {
      const char *debug_section_name = sectionName + (sizeof(".debug_")-1);
      if (strcmp(debug_section_name, "abbrev") == 0) {
        debug_abbrev_section = sectionHeader;
      } else if (strcmp(debug_section_name, "info") == 0) {
        debug_info_section = sectionHeader;
      } else if (strcmp(debug_section_name, "str") == 0) {
        debug_str_section = sectionHeader;
      } else if (strcmp(debug_section_name, "line") == 0) {
        debug_line_section = sectionHeader;
      }
    }
  }


  if(debug_info_section == 0){ libdb_log_error("could not find .debug_info section header"); return 0; }
  if(debug_abbrev_section == 0){ libdb_log_error("could not find .debug_abbrev section header"); return 0; }
  if(debug_str_section == 0) { libdb_log_error("could not find .debug_str section header"); return 0; }
  if(debug_line_section == 0) { libdb_log_error("could not find .debug_line section header"); return 0; }


  typedef struct {
    uint32_t producerNameOffset;
    uint16_t languageIDOffset;
    uint32_t unitNameOffset;
  } DWCompUnitAbbrev;

  typedef struct {
    uint32_t name_offset;
    uint32_t type_id;
    uint8_t decl_file;
    uint8_t decl_line;
  } DWVariableAbbrev;

  typedef struct {
    union {
      DWCompUnitAbbrev compUnitAbbrev;
      DWVariableAbbrev variableAbbrev;
    };
  } DWAbbrevEntry;

#define LIBDB_ARRAY_COUNT(a) ((sizeof(a) / (sizeof(*a))

  //TODO(Torin) What the fuck does Targeting 1k mean?
  DWAbbrevEntry abbrev_entries[64]; //Targeting 1k
  uint32_t abbrev_entry_count = 0;

  { //DWARF4 .debug_abbrev section handler
    libdb_log_debug("reading .debug_abbrev section");
    uint8_t *abbrev_data = (uint8_t *)(debug_abbrev_section->fileOffsetOfSectionData + fileData);
    uint64_t abbrev_section_offset = 0;

    //Parse each abbrev entry and store relevant data in the abbrev_entries array
    while(abbrev_section_offset < debug_abbrev_section->sectionSize){
      libdb_assert(abbrev_entry_count + 1 < 64);
      uint64_t dw_abbrev_code = 0, dw_tag = 0;
      abbrev_section_offset += leb128_decode_uint64(&abbrev_data[abbrev_section_offset], &dw_abbrev_code);
      abbrev_section_offset += leb128_decode_uint64(&abbrev_data[abbrev_section_offset], &dw_tag);
      uint8_t entry_has_children = abbrev_data[abbrev_section_offset];
      abbrev_section_offset++;

      if (dw_tag == 0 && entry_has_children == 0) break;
      libdb_assert(entry_has_children == 0 || entry_has_children == 1);
      libdb_log_debug("dwarf tag: %s, has_children: %s", dw_get_tag_string(dw_tag), entry_has_children ? "true" : "false");

      switch(dw_tag){
        case DW_TAG_compile_unit:{
          DWCompUnitAbbrev *comp_unit_abbrev = (DWCompUnitAbbrev *)&abbrev_entries[abbrev_entry_count];
          size_t abbrev_entry_offset = 0;
          while(1){
            uint64_t dw_attrib = 0, dw_form = 0;
            abbrev_section_offset += leb128_decode_uint64(&abbrev_data[abbrev_section_offset], &dw_attrib);
            abbrev_section_offset += leb128_decode_uint64(&abbrev_data[abbrev_section_offset], &dw_form);
            switch(dw_attrib){
              case DW_AT_name: {
                comp_unit_abbrev->unitNameOffset = abbrev_entry_offset;
                //printf("name offset is %u\n", comp_unit_abbrev->unitNameOffset);
              } break;
              case DW_AT_producer:{
                comp_unit_abbrev->producerNameOffset = abbrev_entry_offset;
              } break;
            }

            if(dw_attrib == 0 && dw_form == 0) break;
            abbrev_entry_offset += dw_get_form_size(dw_form);
            libdb_log_debug("> dw_attrib: %s, dw_form: %s", dw_get_attrib_string(dw_attrib), dw_get_form_string(dw_form));
          }
        }break;

        default: {
          while(1){
            uint64_t dw_attrib = 0, dw_form = 0;
            abbrev_section_offset += leb128_decode_uint64(&abbrev_data[abbrev_section_offset], &dw_attrib);
            abbrev_section_offset += leb128_decode_uint64(&abbrev_data[abbrev_section_offset], &dw_form);
            if(dw_attrib == 0 && dw_form == 0) break;
          }

        } break;


      }

      libdb_log_debug("end_tag");
      abbrev_entry_count++;
    }
  }


  { //DWARF4 .debug_info section handler
    struct CompilationUnitHeader64 {
      uint32_t unit_length;
      uint16_t version;
      uint32_t debug_abbrev_offset;
      uint8_t address_size;
    } __attribute__((packed));

    struct CompilationUnitHeader64* header = (struct CompilationUnitHeader64*)
      (debug_info_section->fileOffsetOfSectionData + fileData);
    libdb_log_debug("");
    libdb_log_debug("dwarf version: %d", ((int)header->version));
    libdb_log_debug("dwarf abbrev offset: %u", header->debug_abbrev_offset);
    libdb_log_debug("dwarf unit length: %u", header->unit_length);

    const uint8_t *debugStringData = (debug_str_section->fileOffsetOfSectionData + fileData);
    uint8_t *compunitData = (uint8_t *)(header + 1) + 1;
    DWCompUnitAbbrev *comp_unit_abbrev = (DWCompUnitAbbrev *)&abbrev_entries[header->debug_abbrev_offset];

    assert(comp_unit_abbrev->unitNameOffset != 0);
    uint32_t *producerStringOffsetPtr = (uint32_t *)(compunitData + comp_unit_abbrev->producerNameOffset);
    uint32_t *nameStringOffsetPtr = (uint32_t *)(compunitData + comp_unit_abbrev->unitNameOffset);
    uint16_t *languageIDPtr= (uint16_t *)(compunitData + comp_unit_abbrev->languageIDOffset);
    uint32_t producerStringOffset = *producerStringOffsetPtr;
    uint32_t nameStringOffset = *nameStringOffsetPtr;
    uint16_t languageID = *languageIDPtr;

    const char *producerName = (const char*)(debugStringData + producerStringOffset);
    const char *compiationUnitName = (const char *)(debugStringData + nameStringOffset);

    libdb_log_debug(" ");
    libdb_log_debug("Compilation Unit: %s, offset %u", compiationUnitName, nameStringOffset);
    libdb_log_debug("Producer: %s, offset %u", producerName, producerStringOffset);
    libdb_log_debug("LanguageID: %hu, offset %u", languageID, comp_unit_abbrev->languageIDOffset);
    libdb_log_debug(" ");
  }

  { //DWARF4 .debug_line section
    uint8_t *line_section_data = (uint8_t *)(debug_line_section->fileOffsetOfSectionData + fileData);

    struct Line_Section_Header_V2 {
      uint32_t total_length;
      uint16_t version;
      uint32_t prologue_length;
      uint8_t minimum_instruction_length;
      uint8_t default_is_stmt;
      int8_t line_base;
      uint8_t line_range;
      uint8_t opcode_base;
    } __attribute((packed));

    struct Line_Section_Header_V2 *header = (struct Line_Section_Header_V2 *)line_section_data;
    if(header->version != 2) {
      libdb_log_error("currently only support version 2 line sections");
      return 0;
    }

    typedef enum {
      libdb_Line_Entry_Type_Invalid,
      libdb_Line_Entry_Type_Statement,
      libdb_Line_Entry_Type_Basic_Block
    } libdb_Line_Entry_Type;

    typedef struct {
      uint8_t entry_type;
      uint32_t line_number_begin;
      uint32_t column_number_begin;
      uint32_t line_number_end;
      uint32_t column_number_end;
    } libdb_Line_Entry;

    typedef struct {
      const char *path;
      uint64_t directory_index;
      uint64_t file_size;
      uint64_t last_modification_time;
      uint64_t *addressess;
      libdb_Line_Entry *line_entries;
      uint64_t line_entry_count;
    } libdb_File_Info;

    libdb_File_Info file_infos[16];
    size_t file_info_count = 0;

    uint8_t *readptr = (uint8_t *)(header + 1);
    uint8_t *standard_opcode_lengths = readptr;
    readptr += header->opcode_base - 1;

    //read null-terminated include_directories string list
    while(*readptr != 0){
      char *include_directory = (char *)readptr;
      size_t length = strlen(include_directory);
      libdb_log_debug("include_directory: %s", include_directory);
      readptr += length + 1;
    }
    readptr++;


    //Parse file entries
    while(*readptr != 0){
      libdb_File_Info *entry = &file_infos[file_info_count++];
      entry->path = (char *)readptr;
      size_t length = strlen(entry->path);
      readptr += length + 1;
      readptr += leb128_decode_uint64(readptr, &entry->directory_index);
      readptr += leb128_decode_uint64(readptr, &entry->last_modification_time);
      readptr += leb128_decode_uint64(readptr, &entry->file_size);
      libdb_log_debug("file_info: %s", entry->path);
    }
    readptr++;

    static const uint32_t DW_LNS_copy = 1;
    static const uint32_t DW_LNS_advance_pc = 2;
    static const uint32_t DW_LNS_advance_line = 3;
    static const uint32_t DW_LNS_set_file = 4;
    static const uint32_t DW_LNS_set_column = 5;
    static const uint32_t DW_LNS_negate_stmt = 6;
    static const uint32_t DW_LNS_set_basic_block = 7;
    static const uint32_t DW_LNS_const_add_pc = 8;
    static const uint32_t DW_LNS_fixed_advance_pc = 9;
    static const uint32_t DW_LNS_set_prologue_end = 10;

    #if 0
    static const uint32_t DW_LNS_set_prologue_end; //DWARF 3
    static const uint32_t DW_LNS_set_epilogue_begin; //DWARF 3
    static const uint32_t DW_LNS_set_isa; //DWARF 3
    #endif
    //NOTE(Torin) Extended Opcodes
    static const uint32_t DW_LNE_end_sequence = 1;
    static const uint32_t DW_LNE_set_address = 2;
    static const uint32_t DW_LNE_define_file = 3;

    uint64_t address = 0;  //instruction-pointer value of instruction
    uint64_t file = 1;
    uint64_t line = 1;
    uint64_t column = 0;
    uint8_t is_stmt = header->default_is_stmt;
    uint8_t basic_block = 0;
    uint8_t end_sequence = 0;

    uint8_t *statement_program = ((uint8_t *)(&header->prologue_length)) + header->prologue_length + sizeof(header->prologue_length);
    uintptr_t statement_program_offset = (uintptr_t)statement_program - (uintptr_t)header;
    size_t statement_program_size = (header->total_length - statement_program_offset + sizeof(header->total_length));

    uint64_t test_addresses[1024];
    libdb_Line_Entry test_line_entries[1024];
    size_t test_line_entry_count = 0;

    uintptr_t current_instruction_offset = 0;
    while(current_instruction_offset < statement_program_size){
      uint8_t opcode = statement_program[current_instruction_offset];
      libdb_log_debug("[0x%lX]", statement_program_offset + current_instruction_offset);
      switch(opcode){

        case DW_LNS_copy: {
          //creates a row in the matrix
          libdb_log_debug("Copy", line, column);
          basic_block = 0;
          current_instruction_offset += 1;

          test_addresses[test_line_entry_count] = address;
          libdb_Line_Entry *entry = &test_line_entries[test_line_entry_count++];
          entry->line_number_begin = line;
          entry->column_number_begin = column;
          if(is_stmt){
            entry->entry_type = libdb_Line_Entry_Type_Statement;
          }
          
        } break;

        case DW_LNS_advance_pc:{
          uint64_t increment = 0;
          current_instruction_offset++;
          current_instruction_offset += leb128_decode_uint64(statement_program + current_instruction_offset, &increment);
          increment *= header->minimum_instruction_length;
          address += increment;
          libdb_log_debug("address is now: %lu", address);
        }break;

        case DW_LNS_advance_line:{
          uint64_t increment = 0;
          current_instruction_offset++;
          current_instruction_offset += leb128_decode_uint64(statement_program + current_instruction_offset, &increment);
          line += increment;
          libdb_log_debug("Advance line by %lu to %lu", increment, line);
          current_instruction_offset++;
        }break;

        case DW_LNS_set_file:{
          uint64_t new_file = 0;
          current_instruction_offset++;
          current_instruction_offset += leb128_decode_uint64(statement_program + current_instruction_offset, &new_file);
          file = new_file;
          libdb_log_debug("setting file index: %u", (uint32_t)file);
        }break;

        case DW_LNS_set_column:{
          uint64_t new_column = 0;
          current_instruction_offset++;
          current_instruction_offset += leb128_decode_uint64(statement_program + current_instruction_offset, &new_column);
          column = new_column;
          libdb_log_debug("Set column number to %u", (uint32_t)new_column);
        }break;

        case DW_LNS_negate_stmt:{
          is_stmt = !is_stmt;

          
          if(is_stmt){
            if(test_line_entry_count > 0){
              libdb_Line_Entry *previous_entry = &test_line_entries[test_line_entry_count-1];
              previous_entry->line_number_end = line;
              previous_entry->column_number_end = column;
            }
            
            test_addresses[test_line_entry_count] = address;
            libdb_Line_Entry *entry = &test_line_entries[test_line_entry_count++];
            entry->line_number_begin = line;
            entry->column_number_begin = column;
          }

          
          current_instruction_offset++;
          libdb_log_debug("statement was negated");
        }break;

        case DW_LNS_set_basic_block:{
          basic_block = 1;
          current_instruction_offset++;
          libdb_log_debug("basic block is now true");
        }break;

        case DW_LNS_const_add_pc:{
          uintptr_t increment_amount = 255 / header->line_range;
          address += increment_amount;
          current_instruction_offset++;
          libdb_log_debug("Increment address register by const value: %u", (uint32_t)increment_amount);
        }break;

        case DW_LNS_fixed_advance_pc:{
          uint16_t increment_amount = *(uint16_t *)(&statement_program[current_instruction_offset]);
          address += increment_amount;
          current_instruction_offset++;
          libdb_log_debug("Increment address register: %u", (uint32_t)increment_amount);
        }break;

        case DW_LNS_set_prologue_end:{
          current_instruction_offset++;
          libdb_log_debug("set proglouge_end to true");
        }break;

        case 0: {
          //NOTE(Torin) Extended(Contains sub-opcodes) Opcodes
          uint64_t instruction_byte_count = 0;
          current_instruction_offset++;
          current_instruction_offset += leb128_decode_uint64(statement_program + current_instruction_offset, &instruction_byte_count);
          uint8_t extended_opcode = *(statement_program + current_instruction_offset);
          switch(extended_opcode){
            case DW_LNE_end_sequence:{
              libdb_log_debug("end_sequence");
              current_instruction_offset++;
              end_sequence = 1;
              address = 0;  //instruction-pointer value of instruction
              file = 1;
              line = 1;
              column = 0;
              is_stmt = header->default_is_stmt;
              basic_block = 0;
              end_sequence = 0;

              //TODO(Torin) Emit row info


            }break;

            //TODO(Torin) Should be 32bits or 64bits based on target machine
            case DW_LNE_set_address:{
              current_instruction_offset++;
              uint64_t new_address = 0;
              new_address = *(uint64_t *)(statement_program + current_instruction_offset);
              current_instruction_offset += sizeof(new_address);
              address = new_address;
              libdb_log_debug("Address Set [0x%lX]", new_address);
            }break;

            case DW_LNE_define_file:{
              current_instruction_offset++;
              const char *name = (const char *)(statement_program + current_instruction_offset);
              current_instruction_offset += strlen(name) + 1;
              uint64_t directory_index = 0;
              uint64_t last_modification_time = 0;
              current_instruction_offset += leb128_decode_uint64(statement_program + current_instruction_offset, &directory_index);
              current_instruction_offset += leb128_decode_uint64(statement_program + current_instruction_offset, &last_modification_time);
            }break;

            default: {
              libdb_log_error("unknown extended opcode");
              current_instruction_offset++;
            };
          }
        }break;


        default: {
          libdb_assert(opcode > header->opcode_base);
          uint8_t special_opcode = opcode - header->opcode_base;
          uintptr_t address_increment_amount = special_opcode / header->line_range;
          uintptr_t line_increment_amount = header->line_base + (special_opcode % header->line_range);
          line += line_increment_amount;
          address += address_increment_amount;
          current_instruction_offset += 1;
          libdb_log_debug("line_increment: %lu; address_increment: %lu", line_increment_amount, address_increment_amount);
        } break;
      }
    }





#if 0
    //Unused in DWARF2
    uint64_t op_index = 0; //only used in VLIW architectures
    uint8_t prologue_end = 0;
    uint8_t epilogue_begin = 0;
    uint64_t isa = 0;
    uint64_t discriminator = 0;
#endif

  for(size_t i = 0; i < test_line_entry_count; i++){
    libdb_Line_Entry *entry = &test_line_entries[i];
    libdb_File_Info *file_info = &file_infos[0];
    libdb_log_debug("[%s %u:%u]", file_info->path, entry->line_number_begin, entry->column_number_begin);
  }






  }

  assert(symbolTableHeader != NULL);
  assert(stringTableHeader != NULL);
  assert(symbolTableEntryCount > 1);
  ELFSymbol *symbolTableData =
    (ELFSymbol*)(symbolTableHeader->fileOffsetOfSectionData + fileData);

  uint64_t functionSymbolCount = 0;
  uint64_t functionStringMemoryRequirement = 0;
  uint64_t fileSymbolCount = 0;
  uint64_t fileStringMemoryRequirement = 0;

  for (uint32_t i = 1; i < symbolTableEntryCount; i++) {
    ELFSymbol *symbol = &symbolTableData[i];
    const char *symbolName = symbol->nameOffset + symbolStringTableData;
    uintptr_t symbolAddress = symbol->symbolValue;
    if (symbol->type == ELF_SYMBOL_TYPE_FUNCTION) {
      functionSymbolCount++;
      functionStringMemoryRequirement += strlen(symbolName) + 1;
    } else if (symbol->type == ELF_SYMBOL_TYPE_FILE) {
      fileSymbolCount++;
      fileStringMemoryRequirement += strlen(symbolName) + 1;
    }
  }

  size_t requiredSymbolTableMemory  = (functionSymbolCount * sizeof(uint64_t)) +
    functionStringMemoryRequirement + fileStringMemoryRequirement;
  uint8_t *symbolTableMemory = (uint8_t *)libdb_malloc(requiredSymbolTableMemory);

  libdb_Symbol_Table *symTable = &program->symbol_table;
  symTable->functionNames = (const char *)symbolTableMemory;
  symTable->functionAddresses = (uintptr_t *)(symbolTableMemory +
      functionStringMemoryRequirement);
  symTable->fileNames = (const char *)symTable->functionAddresses + functionSymbolCount;
  symTable->functionCount = functionSymbolCount;
  symTable->fileCount = fileSymbolCount;

  uint32_t currentFunctionIndex = 0;
  uint32_t currentFileIndex = 0;
  char *functionNameWrite = (char *)symTable->functionNames;
  char *fileNameWrite = (char *)symTable->fileNames;
  for (uint32_t i = 1; i < symbolTableEntryCount; i++) {
    ELFSymbol *symbol = &symbolTableData[i];
    const char *symbolName = symbol->nameOffset + symbolStringTableData;
    uintptr_t symbolAddress = symbol->symbolValue;

    if (symbol->type == ELF_SYMBOL_TYPE_FUNCTION) {
      size_t symbolNameLength = strlen(symbolName);
      memcpy(functionNameWrite, symbolName, symbolNameLength);
      functionNameWrite[symbolNameLength] = 0;
      functionNameWrite += symbolNameLength + 1;
      symTable->functionAddresses[currentFunctionIndex] = symbolAddress;
      currentFunctionIndex++;
    } else if(symbol->type == ELF_SYMBOL_TYPE_FILE) {
      size_t symbolNameLength = strlen(symbolName);
      memcpy(fileNameWrite, symbolName, symbolNameLength);
      fileNameWrite[symbolNameLength] = 0;
      fileNameWrite += symbolNameLength + 1;
      currentFileIndex++;
    }
  }

  assert(currentFunctionIndex == symTable->functionCount);
  assert(currentFileIndex == symTable->fileCount);

  program->state = libdb_Program_State_STOPPED;
  program->stop_reason = libdb_Stop_Reason_NONE;
  program->breakpoint_id = -1;
  program->rip = 0;

  pid_t pid = fork();
  program->pid = pid;

  if (pid == -1) {
    return 1;
  } else if (pid == 0) {
    char* const args[] = { NULL };
    char* const envp[] = { NULL };
    ptrace(PTRACE_TRACEME, NULL, NULL);
    execve("test", args, envp);
    libdb_log_error("the child process failed to execute\n");
    return 1;
  } else {
    int childStatus = 0;
    waitpid(pid, &childStatus, __WALL);
    if (childStatus == -1) {
      libdb_log_error("error on waitpid for childProcess");
    } else {
      assert(WIFSTOPPED(childStatus));
      int stopSignalNumber = WSTOPSIG(childStatus);
      assert(stopSignalNumber == LIBDB_SIGNAL_TRAP);
    }
  }

  libdb_log_debug("childpid is %d", pid);

  return 0;
}

#endif//LIBDB_IMPLEMENTATION

