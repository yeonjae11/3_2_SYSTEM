#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/* This is skeleton code for reading characters from 
standard input (e.g., a file or console input) one by one until 
the end of the file (EOF) is reached. It keeps track of the current 
line number and is designed to be extended with additional 
functionality, such as processing or transforming the input data. 
In this specific task, the goal is to implement logic that removes 
C-style comments from the input. */

// name :             kim yeon jae
// # of assignment :  HW1 
// File name:         decomment.c

#define NEW_LINE '\n'
#define DOUBLE_QUOTE '\"'
#define SINGLE_QUOTE '\''
#define SLASH '/'

#define NOT_EOF 0
#define GET_EOF 1
#define FAIL_EOF 2

int inStrOrChar(char quote, int* line_cur){
  // Reads characters from the standard input stream until it encounters quote or EOF.
  // Parameters:
  // - quote: A character double quote(") or single quote(')
  // - line_cur: A pointer to an integer that tracks the current line number
  // Return value:
  // - Returns GET_EOF if EOF is encountered, otherwise returns NOT_EOF
  // Input/output:
  // - Reads from the standard input stream (stdin).
  // - Writes each character read from stdin to the standard output stream (stdout).
  // Global variables: None
  char ch;
  int ich;

  while((ich = getchar()) != EOF){
    ch = (char) ich;
    fprintf(stdout, "%c",ch);
    if(ch == quote){
      break;
    }
    if(ch == NEW_LINE){
      (*line_cur)++; 
    }
  }
  if(ich == EOF) return GET_EOF;

  return NOT_EOF;
}

int inSingleComment(int* line_cur){
  // Skips characters from the standard input stream until it encounters a newline('\n') or EOF.
  // Parameters:
  // - line_cur: A pointer to an integer that tracks the current line number
  // Return value:
  // - Returns GET_EOF if EOF is encountered before a newline, otherwise returns NOT_EOF
  // Input/output:
  // - Reads characters from the standard input stream (stdin).
  // - Writes the newline character to the standard output stream (stdout)
  // Global variables: None

  char ch;
  int ich;

  while((ich = getchar()) != EOF){
    ch = (char)ich;
    if(ch == NEW_LINE){
      (*line_cur)++;
      fprintf(stdout, "%c",ch);
      break;
    }
  }
  if(ich == EOF){
    return GET_EOF;
  }
  return NOT_EOF;
}

int inMultiComment(int* line_cur){
  // Skips characters from the standard input stream until it encounters "*/" or EOF.
  // Parameters:
  // - line_cur: A pointer to an integer that tracks the current line number
  // Return value:
  // - Returns FAIL_EOF if EOF is encountered before "*/" indicating ERROR(FAILURE) otherwise NOT_EOF
  // Input/output:
  // - Reads characters from the standard input stream (stdin).
  // - Writes newline characters from stdin to the standard output stream (stdout).
  // Global variables: None
  char ch;
  int ich;

  while((ich = getchar()) != EOF){
    ch = (char)ich;
    if(ch == NEW_LINE){
      (*line_cur)++;
      fprintf(stdout, "%c",ch);
    }
    else if(ch == '*'){
      while((char)(ich = getchar()) == '*'){
      }

      if(ich == EOF){
        return FAIL_EOF;
      }

      if((char)ich == SLASH){
        return NOT_EOF;
      }

      ch = (char)ich;
      if(ch == NEW_LINE){
        (*line_cur)++;
        fprintf(stdout, "%c",ch);
      }
    }
  }

  if(ich == EOF){
    return FAIL_EOF;
  }

  return NOT_EOF;
}

int main(void)
{
  // ich: int type variable to store character input from getchar() (abbreviation of int character)
  int ich;
  // line_cur & line_com: current line number and comment line number (abbreviation of current line and comment line)
  int line_cur, line_com;
  // ch: character that comes from casting (char) on ich (abbreviation of character)
  char ch;

  line_cur = 1;
  line_com = -1;

  int got_eof = 0;

  // This while loop reads all characters from standard input one by one
  while (1) {    

    ich = getchar();
    if (ich == EOF) break;

    ch = (char)ich;

    switch(ch){
      case NEW_LINE:
          line_cur++;
          fprintf(stdout, "%c",ch);
          break;

      case DOUBLE_QUOTE:
      case SINGLE_QUOTE:
          fprintf(stdout, "%c",ch);
          got_eof = inStrOrChar(ch, &line_cur);
          break;

      case SLASH:
          ich = getchar();
          if(ich == EOF){
            got_eof =1;
            break;
          }
          ch = (char) ich;
          
          if(ch == SLASH){
            fprintf(stdout, " ");
            got_eof = inSingleComment(&line_cur);
            break;
          }
          if(ch == '*'){
            fprintf(stdout, " ");
            line_com = line_cur;
            got_eof = inMultiComment(&line_cur);
            break;
          }
          if(ch == NEW_LINE){
            line_cur++;
          }
          fprintf(stdout, "%c",SLASH);
          fprintf(stdout, "%c",ch);
          break;

      default:
          fprintf(stdout, "%c",ch);
          break;
    }

    if (got_eof) break; 

  }
  
  if(got_eof == FAIL_EOF){
    fprintf(stderr,"Error: line %d: unterminated comment\n",line_com);
    return EXIT_FAILURE;
  }
  return(EXIT_SUCCESS);
}