#define LIBDB_IMPLEMENTATION
#include "libdb.h"

#include <assert.h>
#include <stdio.h>
#include <malloc.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>

#if 1
void PrintSymbolTable(libdb_Symbol_Table *symbolTable)
{
  const char *filename = symbolTable->fileNames;
  for (uint32_t i = 0; i < symbolTable->fileCount; i++) {
    printf("file: %s\n", filename);
    filename += strlen(filename) + 1;
  }

  const char *functionName = symbolTable->functionNames;
  for (uint32_t i = 0; i < symbolTable->functionCount; i++) {
    uintptr_t functionAddress = symbolTable->functionAddresses[i];
    printf("function %s %lX\n", functionName, functionAddress);
    functionName += strlen(functionName) + 1;
  }
}

#endif

#if 0
int CreateBreakpoint(DebugState *debugState, const char* symbolName) 
{
  SymbolTable *symTable = &debugState->symbolTable;
  const char *currentFunctionName = symTable->functionNames;
  for (uint32_t i = 0; i < symTable->functionCount; i++) {
    if (strcmp(currentFunctionName, symbolName) == 0) {
      uint64_t functionAddress = symTable->functionAddresses[i];
      uint64_t previousOpCode = ptrace(PTRACE_PEEKTEXT, debugState->debugPID, 
          functionAddress, NULL); 
      ptrace(PTRACE_POKETEXT, debugState->debugPID, functionAddress, 0xCC); 
      libdb_log_info("breakpoint-create: at symbol %s (%lX)", symbolName, functionAddress);
      return 1;
    }
    currentFunctionName += strlen(currentFunctionName) + 1;
  }
  libdb_log_error("breakpoint-create: failed to create breakpoint, could not resolve symbol %s", symbolName);
  return 0;
}
#endif


int main() {
  libdb_Program program;
  libdb_program_open("test", &program);

  #if 0
  libdb_breakpoint_create_at_symbol("main", &program);
  //libdb_program_execute(program);


  //PrintSymbolTable(&program.symbol_table);

  
  libdb_execution_continue(&program);

  int isRunning = 1;
  uint64_t counter = 0;
  while (isRunning) {
    if (libdb_program_update_state(&program)) {
      if (program.state == libdb_Program_State_EXITED) {
        return 0;
      }

      
      switch (program.stop_reason) {
        case libdb_Stop_Reason_BREAKPOINT_HIT : {

          printf("we stopped yo.  contiuning yo \n");
          libdb_execution_continue(&program);
        } break;
      }
    } else {

#if 0
      if (counter % 100 == 0) {
        uint64_t rip = libdb_get_rip(&program);
        uint64_t instruction = libdb_get_instruction(&program, rip); 
        printf("current rip: 0x%lX, 0x%lX\n", rip, instruction);
      } 
      counter++;
#endif

    }
  } 
#endif

  return 0;
}

#if 0
int libdb_execute_program(DebugState *debugState, 
    const char* filepath, const char** argv, const char** envp) 
{
  pid_t pid = fork();
  if (pid == -1) {
    return 1;  
  } else if (pid == 0) {
    char* const args[] = { NULL };
    char* const envp[] = { NULL };
    ptrace(PTRACE_TRACEME, NULL, NULL);
    //raise(SIGSTOP);
    execve("test", args, envp);
    libdb_log_error("the child process failed to execute\n");
    return 1;
  } else {
    pid_t childPid = pid;
    int childStatus = 0;
    waitpid(childPid, &childStatus, __WALL);
    if (childStatus == -1) {
      libdb_log_error("error on waitpid for childProcess");
    } else {
      libdb_log_info("recieved signal from childProcess");
      if (WIFEXITED(childStatus)) {
        int exitStatus = WEXITSTATUS(childStatus);
        libdb_log_info("the child terminated normaly with return code %d", exitStatus);
      } else if (WIFSIGNALED(childStatus)) {
        int signalNumber = WTERMSIG(childStatus);
        libdb_log_info("the child recived signal %d", signalNumber);
      } else if (WIFSTOPPED(childStatus)) {
        int stopSignalNumber = WSTOPSIG(childStatus);
        libdb_log_info("the child was stopped with signal %s", 
            SIGNAL_NAME_LIST[stopSignalNumber]);
      }
    }
  }
  return 0;
}


int main() {
  size_t fileSize = 0;
  uint8_t* fileData = ReadFileData("test", &fileSize);
  assert(fileData != null);

  ELF64Header* header = (ELF64Header*)fileData;

#if 0
  if (header->magicNumber == ELF64_MAGIC_NUMBER) {
    printf("valid elf header\n");
  } else {
    printf("invalid elf header\n");
  }

  if (header->bitType == 1) {
    printf("header is 32 bit\n");
  } else if (header->bitType == 2) {
    printf("header is 64 bit\n");
  }

  if (header->dataEncoding == ELF_DATA_ENCODING_LITTLE_ENDIAN) {
    printf("header is littleEndian\n");
  } else if (header->dataEncoding == ELF_DATA_ENCODING_BIG_ENDIAN) {
    printf("header is bigEndian\n");
  }

  if (header->instructionSet == 0x3) {
    printf("instructionSet is x86\n");
  } else if (header->instructionSet == 0x3E) {
    printf("instructionSet is x86_64\n");
  } else if (header->instructionSet == 0x28) {
    printf("instructionSet is arm\n");
  }

  if (header->objectFileType == ELF_OBJ_TYPE_RELOCATABLE) {
    printf("the elf file is relocatable\n");
  }
#endif
 
  ELFSectionHeader *sectionStringTableSectionHeader = (ELFSectionHeader*)(fileData + 
      header->sectionHeaderOffset + ((header->sectionStringTableSectionIndex) 
        * header->sectionHeaderEntrySize));
  char *sectionStringTableData = (char *)(fileData + 
      (sectionStringTableSectionHeader->fileOffsetOfSectionData));
  assert(*sectionStringTableData == 0); //First byte of the stringTableData is always null

  printf("elf file has %u sections\n", header->sectionHeaderEntryCount);


  ELFSectionHeader* symbolTableHeader = NULL;
  ELFSectionHeader* stringTableHeader = NULL;
  char* symbolStringTableData = NULL;
  uint32_t symbolTableEntryCount = 0;

  for (uint32_t i = 0; i < header->sectionHeaderEntryCount; i++) {  
    ELFSectionHeader *sectionHeader = (ELFSectionHeader*)(fileData + 
      header->sectionHeaderOffset + ((i+1) * header->sectionHeaderEntrySize));

    const char *sectionName = sectionStringTableData + sectionHeader->nameOffset;
    printf("found section header: %s\n", sectionName);
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
    }

    if (symbol->type == ELF_SYMBOL_TYPE_FILE) {
      fileSymbolCount++;
      fileStringMemoryRequirement += strlen(symbolName) + 1;
    }
  }

  size_t requiredSymbolTableMemory  = (functionSymbolCount * sizeof(uint64_t)) +
    functionStringMemoryRequirement + fileStringMemoryRequirement;
  uint8_t *symbolTableMemory = (uint8_t *)malloc(requiredSymbolTableMemory);

  DebugState debugState = {};
  SymbolTable *symTable = &debugState.symbolTable;
  
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
      
    } else if(symbol->type == ELF_SYMBOL_TYPE_FILE) { 
      size_t symbolNameLength = strlen(symbolName);
      memcpy(fileNameWrite, symbolName, symbolNameLength);
      fileNameWrite[symbolNameLength] = 0;
      fileNameWrite += symbolNameLength + 1;
    }
  }


  //PrintSymbolTable(symTable);
#if 0

  pid_t pid = fork();
  if (pid == -1) {
    return 1;  
  } else if (pid == 0) {
    char* const args[] = { NULL };
    char* const envp[] = { NULL };
    ptrace(PTRACE_TRACEME, NULL, NULL);
    //raise(SIGSTOP);
    execve("test", args, envp);
    libdb_log_error("the child process failed to execute\n");
    return 1;
  } else {

    CreateBreakpoint(&debugState, "main");

    int isRunning = 1;
    pid_t childPid = pid;
    int childStatus = 0;
    
    while (isRunning) {
      int childHasChangedState = waitpid(childPid, &childStatus, __WALL);
      if (childHasChangedState == 0) continue;

      if (childStatus == -1) {
        libdb_log_error("error on waitpid for childProcess");
      } else if (childStatus == 0) {
        //waiting for a signal
      } else {
        libdb_log_info("recieved signal from childProcess");
        if (WIFEXITED(childStatus)) {
          int exitStatus = WEXITSTATUS(childStatus);
          libdb_log_info("the child terminated normaly with return code %d", exitStatus);
        } else if (WIFSIGNALED(childStatus)) {
          int signalNumber = WTERMSIG(childStatus);
          libdb_log_info("the child recived signal %d", signalNumber);
        } else if (WIFSTOPPED(childStatus)) {
          int stopSignalNumber = WSTOPSIG(childStatus);
          libdb_log_info("the child was stopped with signal %s", 
              SIGNAL_NAME_LIST[stopSignalNumber]);
          ptrace(PTRACE_CONT, childPid, NULL, NULL);
        }
      }
    }

    //CreateBreakpoint(&debugState, "main");
  }

#endif

  
  return 0;
}


#endif


