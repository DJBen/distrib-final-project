#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

static  char  User[80];
static  char  Spread_name[80];

static  char  Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;

static  void  Print_menu();
static  void  User_command();
static  void  Read_message();

static void Usage(int argc, char *argv[]) {

}

int main(int argc, char const *argv[])
{
  int     ret;

  Usage( argc, argv );

  ret = SP_connect( Spread_name, User, 0, 1, &Mbox, Private_group );
  if( ret != ACCEPT_SESSION )
  {
    SP_error( ret );
    exit(0);
  }

  printf("User: connected to %s with private group %s\n", Spread_name, Private_group );

  E_init();

  E_attach_fd( 0, READ_FD, User_command, 0, NULL, LOW_PRIORITY );

  E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY );

  printf("\nUser> ");
  fflush(stdout);

  Num_sent = 0;

  E_handle_events();

  return 0;
}


