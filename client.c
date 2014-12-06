#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>
#include "chat_includes.h"

static  char  User[80];
static  char  Spread_name[80];
static  char  username[80];
static char roomName[MAX_GROUP_NAME];

static  char  Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;

static char temp_group_name[MAX_GROUP_NAME];
static char server_group_name[MAX_GROUP_NAME];
static char server_client_group_name[MAX_GROUP_NAME];
static bool isConnected;
static int  server_index = -1;

static  void  Print_menu();
static  void  User_command();
static  void  Read_message();
static void Bye();
static void CreateJoinRoomUpdate(Update **update, char *groupName);
static void CreateAddLineUpdate(Update **update, char *line, char *room);
static void PrintHistoryWithReply(Reply *reply);

static void Usage(int argc, char const *argv[]) {
  sprintf( User, "slu21" );
  sprintf( Spread_name, "10270");
}

int main(int argc, char const *argv[])
{
  int     ret;

  Usage( argc, argv );

  ret = SP_connect( Spread_name, User, 0, 1, &Mbox, Private_group );
  if( ret != ACCEPT_SESSION )
  {
    SP_error( ret );
    exit(1);
  }

  printf("User: connected to %s with private group %s\n", Spread_name, Private_group );

  Print_menu();
  printf("\nUser> ");
  // fflush(stdout);

  isConnected = false;

  E_init();

  E_attach_fd( 0, READ_FD, User_command, 0, NULL, LOW_PRIORITY );

  E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY );

  E_handle_events();

  return 0;
}

static void User_command() {
  char  command[130];
  int ret;
  int i;
  Update *update = NULL;
  char line[MAX_LINE_LENGTH];

  for (i=0; i < sizeof(command); i++ ) command[i] = 0;
  if( fgets( command, 130, stdin ) == NULL ) Bye();

  switch( command[0] ) {
    case 'u':
      if (username[0] == '\0') {
        ret = sscanf( &command[2], "%s", username );
        if( ret < 1 )
        {
          printf("Invalid username.\n");
          break;
        } else {
          printf("Logged in with username: %s\n", username);
        }
      } else {
        // TODO: Quit chat group if relog in with another username
      }
      break;
    case 'c':
      ret = sscanf( &command[2], "%d", &server_index );
      if( ret < 1 )
      {
        printf("Invalid server index to connect.\n");
        break;
      } else {
        printf("Prepare to connect server: %d\n", server_index);
        sprintf(temp_group_name, ServerClientGroupNameWithIndex(server_index));
        SP_join(Mbox, temp_group_name);
        isConnected = true;
        sprintf(server_group_name, ServerGroupNameWithIndex(server_index));
        strcpy(server_client_group_name, temp_group_name);
      }
      break;
    case 'j':
      if (username[0] == '\0') {
        printf("You haven't logged in.\n");
        break;
      }
      if (server_index == -1) {
        printf("You haven't connect to a server.\n");
        break;
      }
      ret = sscanf( &command[2], "%s", roomName);
      if( ret < 1 )
      {
        printf("Invalid chat room name.\n");
        break;
      }
      CreateJoinRoomUpdate(&update, roomName);
      ret = SP_multicast( Mbox, AGREED_MESS, server_group_name, UpdateMessageType, UpdateMsgSize, (char *)update);

      if( ret != UpdateMsgSize)
      {
        if( ret < 0 )
        {
          SP_error( ret );
          exit(1);
        }
      }

      // Join the corresponding chat room spread group
      sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, roomName);
      SP_join(Mbox, temp_group_name);

      printf("Joined chat room %s\n", roomName);
      break;
    case 'a':
      if (username[0] == '\0') {
        printf("You haven't logged in.\n");
        break;
      }
      if (server_index == -1) {
        printf("You haven't connect to a server.\n");
        break;
      }
      if (roomName[0] == '\0') {
        printf("You haven't joined a room.\n");
        break;
      }
      ret = sscanf( &command[2], "%[^\t\n]", line);
      if( ret < 1 )
      {
        printf("Invalid line content. Must <= 120 characters.\n");
        break;
      }
      CreateAddLineUpdate(&update, line, roomName);
      ret = SP_multicast( Mbox, AGREED_MESS, server_group_name, UpdateMessageType, UpdateMsgSize, (char *)update);
      if( ret != UpdateMsgSize)
      {
        if( ret < 0 )
        {
          SP_error( ret );
          exit(1);
        }
      }
      break;
    case 'h':
      if (username[0] == '\0') {
        printf("You haven't logged in.\n");
        break;
      }
      if (server_index == -1) {
        printf("You haven't connect to a server.\n");
        break;
      }
      if (roomName[0] == '\0') {
        printf("You haven't joined a room.\n");
        break;
      }
      if (update) free(update);
      update = malloc(sizeof(Update));
      update->type = PrintHistory;
      strcpy(update->room, roomName);
      strcpy(update->user, username);
      update->sender_index = 0;
      ret = SP_multicast( Mbox, AGREED_MESS, server_group_name, UpdateMessageType, UpdateMsgSize, (char *)update);
      if( ret != UpdateMsgSize)
      {
        if( ret < 0 )
        {
          SP_error( ret );
          exit(1);
        }
      }
      printf("Print history request sent");
      break;
    default:
      Print_menu();
      break;
  }
  if (username[0] == '\0') {
    printf("\nUser> ");
  } else {
    printf("\n%s> ", username);
  }
  fflush(stdout);
}

static void Read_message() {
  static  Reply  *reply;
  char     sender[MAX_GROUP_NAME];
  char     ret_groups[MAX_GROUPS][MAX_GROUP_NAME];
  int    num_groups;
  int    service_type;
  int16    mess_type;
  int    endian_mismatch;
  int    i,j;
  int    ret;
  membership_info memb_info;
  char temp_name[MAX_GROUP_NAME];

  reply = malloc(MAX_REPLY_SIZE);

  ret = SP_receive( Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
    &mess_type, &endian_mismatch, MAX_REPLY_SIZE, (char *)reply );

  if (Is_regular_mess(service_type)) {
    if (mess_type != ReplyMessageType) {
      printf("Message type %d not correct!", mess_type);
      return;
    }

    printf("Received reply with type %d, count %d.\n", reply->type, reply->count);
    if (reply->type == ReplyPrintHistory) {
      PrintHistoryWithReply(reply);
    } else if (reply->type == ReplyEchoLine) {
      PrintHistoryWithReply(reply);
    } else if (reply->type == ReplyNewUserJoin) {
      strcpy(temp_name, reply->content + sizeof(char) * MAX_GROUP_NAME * reply->count + sizeof(char) * MAX_LINE_LENGTH * reply->count + sizeof(int) * reply->count);
      printf("New user %s joined to the chat.\n", temp_name);
    }
    free(reply);
  } else if (Is_membership_mess( service_type)) {
    ret = SP_get_memb_info(reply, service_type, &memb_info);
    if (ret < 0) {
      printf("BUG: membership message does not have valid body\n");
      SP_error(ret);
      exit(1);
    }
    if (Is_reg_memb_mess(service_type)) {
      printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n", sender, num_groups, mess_type );
      for( i=0; i < num_groups; i++) printf("\t%s\n", &ret_groups[i][0] );
      if (strncmp(sender, "server_chatroom_", strlen("server_chatroom_")) == 0) {
        if (Is_caused_join_mess(service_type)) {
          printf("Due to JOIN of %s\n", memb_info.changed_member);
        }
      }
    }
  }
}

static void CreateJoinRoomUpdate(Update **update, char *roomName) {
  if (*update != NULL) free(*update);
  *update = malloc(sizeof(Update));
  (*update)->type = JoinRoom;
  strcpy((*update)->room, roomName);
  strcpy((*update)->user, username);
  (*update)->sender_index = 0;
}

static void CreateLikeLineUpdate(Update **update, char *roomName, bool isAdd) {
  if (*update != NULL) free(*update);
  *update = malloc(sizeof(Update));

}

static void CreateAddLineUpdate(Update **update, char *line, char *room) {
  *update = malloc(sizeof(Update));
  (*update)->type = AddLine;
  strcpy((*update)->line, line);
  strcpy((*update)->room, room);
  strcpy((*update)->user, username);
  (*update)->sender_index = 0;
}

static void Bye() {
  SP_disconnect(Mbox);
  exit(0);
}

static  void  Print_menu() {
  printf("\n");
  printf("==========================================\n");
  printf("Client Menu:\n");
  printf("------------------------------------------\n");
  printf("u <username> -- log in with user\n");
  printf("c <server index> -- connect to a server\n");
  printf("j <room> -- join a room\n");
  printf("------------------------------------------\n");
  printf("a <line> -- append a line to the chat\n");
  printf("l <line #> -- like a line\n");
  printf("r <line #> -- remove the like from line\n");
  printf("h -- print chat history\n");
  printf("v -- print view of current server\n");
  printf("==========================================\n");
  fflush(stdout);
}

static void PrintHistoryWithReply(Reply *reply) {
  int like_count = -1;
  int i;
  printf("\n");
  for (i = 0; i < reply->count; i++) {
    printf("%d. %s: %s", i + 1,
      reply->content + sizeof(char) * i * MAX_GROUP_NAME,
      reply->content + sizeof(char) * reply->count * MAX_GROUP_NAME + sizeof(char) * i * MAX_LINE_LENGTH);
    memcpy(&like_count, reply->content + sizeof(char) * reply->count * MAX_GROUP_NAME + sizeof(char) * reply->count * MAX_LINE_LENGTH + sizeof(int) * i, sizeof(int));
    if (like_count > 0) {
      printf(" (%d likes)\n", like_count);
    } else {
      printf("\n");
    }
  }
  printf("%s>", username);
}
