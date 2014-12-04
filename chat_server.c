#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "chat_includes.h"

#define MAX_CHATROOM_COUNT 100
#define MAX_LINE_COUNT 1000
#define MAX_USER_NAME_LENGTH 80
#define MAX_USER_COUNT 64
#define MAX_GROUPS 10

typedef struct
{
  char content[MAX_LINE_LENGTH];
  char sender[MAX_USER_NAME_LENGTH];
  // All users who have liked this.
  int like_count;
  char likes[MAX_USER_COUNT][MAX_USER_NAME_LENGTH];
} Line;

typedef struct
{
  char name[80];
  Line lines;
} Chatroom;

static mailbox Mbox;
static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static int server_index;
static Chatroom *chatrooms;

static char ret_groups[MAX_GROUPS][MAX_GROUP_NAME];

static char temp_group_name[MAX_GROUP_NAME];

static void Print_help()
{
  printf("chat_server <server index>");
  exit( 0 );
}

static void Usage(int argc, char const *argv[])
{
  sprintf( User, "slu21" );
  sprintf( Spread_name, "10270");
  if (argc != 2) {
    Print_help();
  }
  server_index = (int)strtol(argv[1], (char **)NULL, 10);
  if (server_index < 1 || server_index > 5) {
    Print_help();
  }
}

char* serverNameWithIndex(int index) {
  sprintf(temp_group_name, "server_%d", index);
  return temp_group_name;
}

static void Runloop() {
  int i, j;
  int num_groups; /* number of members in the group */
  char sender[MAX_GROUP_NAME];
  int16 mess_type;
  int dummy_endian_mismatch;
  int ret;
  int service_type;
  Update *recv_mess;

  int servers_joined = 0;

  chatrooms = malloc(sizeof(Chatroom) * MAX_CHATROOM_COUNT);
  recv_mess = malloc(sizeof(Update));

  while (servers_joined < 5) {
    ret = SP_receive( Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
                        &mess_type, &dummy_endian_mismatch, UpdateMsgSize, (char *)recv_mess );

    if (Is_reg_memb_mess(service_type)) {
      printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n", sender, num_groups, mess_type );
      if (strcmp(sender, "servers") == 0) {
        // Count joined servers
        servers_joined++;
      }
      for( i=0; i < num_groups; i++ ) printf("\t%s\n", &ret_groups[i][0] );
    } else {
      printf("received message of other message type 0x%x with ret %d\n", service_type, ret);
    }
  }

  // All servers joined at this point
  printf("All servers joined at this point.\n");


}

int main(int argc, char const *argv[])
{
  int ret;
  char *spread_group_name;

  Usage(argc, argv);

  ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
  if (ret < 0) {
    SP_error(ret);
    exit(1);
  }

  // Create a group with only server
  spread_group_name = serverNameWithIndex(server_index);
  SP_join( Mbox, spread_group_name );

  // Create a group with clients
  sprintf(spread_group_name, "server_clients_%d", server_index);
  SP_join(Mbox, spread_group_name);

  // Join only group containing all servers
  SP_join(Mbox, "servers");

  Runloop();

  return 0;
}
