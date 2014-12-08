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
static char temp_room_name[MAX_GROUP_NAME];
static int temp_server_index;

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
static void CreateLikeLineUpdate(Update **update, int line_number, char *room, bool isAdd);

static void PrintHistoryWithReply(Reply *reply);

static void Usage(int argc, char const *argv[]) {
  sprintf( User, "client" );
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
  char like_update = -1; // -1 undefined, 1 add like, 0 remove like
  int line_number = -1;

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
      ret = sscanf( &command[2], "%d", &temp_server_index);
      if (temp_server_index < 1 || temp_server_index > 5) {
        printf("Server range: [1, 5]. Please choose it again.\n");
        break;
      }
      if (server_index != -1) {
        if (temp_server_index == server_index) {
          printf("You've already connected to server %d\n", server_index);
          break;
        } else {
          // Disconnect from old server and connect to the new server
          sprintf(temp_group_name, ServerClientGroupNameWithIndex(server_index));
          SP_leave(Mbox, temp_group_name);
        }
      }
      server_index = temp_server_index;
      if( ret < 1 )
      {
        printf("Invalid server index to connect.\n");
        break;
      } else {
        sprintf(temp_group_name, ServerClientGroupNameWithIndex(server_index));
        SP_join(Mbox, temp_group_name);
        printf("Connected to server: %d\n", server_index);
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
      ret = sscanf( &command[2], "%s", temp_room_name);
      if( ret < 1 )
      {
        printf("Invalid chat room name.\n");
        break;
      }
      if (roomName[0] != '\0') {
        if (strcmp(roomName, temp_room_name) == 0) {
          printf("You are in chatroom %s right now.\n", roomName);
          break;
        }
        // Leave the corresponding chat room spread group
        sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, roomName);
        SP_leave(Mbox, temp_room_name);

        // Send leave room message
        // CreateLeaveRoomUpdate(&update, roomName);
        // ret = SP_multicast( Mbox, AGREED_MESS, server_group_name, UpdateMessageType, UpdateMsgSize, (char *)update);
        // if( ret != UpdateMsgSize)
        // {
        //   if( ret < 0 )
        //   {
        //     SP_error( ret );
        //     exit(1);
        //   }
        // }

        printf("Left chat room %s\n", roomName);
      }
      // Send join new room message
      // First copy the temp room name to room name
      strcpy(roomName, temp_room_name);

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
        printf("Invalid line content. Must > 0 and <= 120 characters.\n");
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
    case 'l':
      like_update = 1;
      // no break here: like and not like are sent through the same mechanism
    case 'r':
      if (like_update == -1) like_update = 0;
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
      ret = sscanf( &command[2], "%d", &line_number);
      if( ret < 1 )
      {
        printf("Please like/unlike a line number.\n");
        break;
      }
      CreateLikeLineUpdate(&update, line_number, roomName, like_update);
      ret = SP_multicast( Mbox, AGREED_MESS, server_group_name, UpdateMessageType, UpdateMsgSize, (char *)update);
      if( ret != UpdateMsgSize)
      {
        if( ret < 0 )
        {
          SP_error( ret );
          exit(1);
        }
      }
      printf("%s attemps to %s line %d\n", username, like_update ? "like" : "unlike", line_number);
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
    case 'v':
      if (username[0] == '\0') {
        printf("You haven't logged in.\n");
        break;
      }
      if (server_index == -1) {
        printf("You haven't connect to a server.\n");
        break;
      }
      if (update) free(update);
      update = malloc(sizeof(Update));
      update->type = PrintView;
      strcpy(update->user, username);
      ret = SP_multicast( Mbox, AGREED_MESS, server_group_name, UpdateMessageType, UpdateMsgSize, (char *)update);
      if( ret != UpdateMsgSize)
      {
        if( ret < 0 )
        {
          SP_error( ret );
          exit(1);
        }
      }
      printf("Queried server view.\n");
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
  bool like_success;
  bool server_found;

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
      if (strcmp(temp_name, username) != 0) {
        printf("New user %s joined the chatroom.\n", temp_name);
      }
      PrintHistoryWithReply(reply);
    } else if (reply->type == ReplyAddLike) {
      memcpy(&like_success, reply->content + sizeof(char) * MAX_GROUP_NAME * reply->count + sizeof(char) * MAX_LINE_LENGTH * reply->count + sizeof(int) * reply->count + sizeof(char) * MAX_GROUP_NAME, sizeof(bool));
      printf("Like line %s\n", like_success ? "succeeded" : "failed");
      PrintHistoryWithReply(reply);
    } else if (reply->type == ReplyRemoveLike) {
      memcpy(&like_success, reply->content + sizeof(char) * MAX_GROUP_NAME * reply->count + sizeof(char) * MAX_LINE_LENGTH * reply->count + sizeof(int) * reply->count + sizeof(char) * MAX_GROUP_NAME, sizeof(bool));
      printf("Unlike line %s\n", like_success ? "succeeded" : "failed");
      PrintHistoryWithReply(reply);
    } else if (reply->type == ReplyPrintView) {
      printf("Servers from view of server %d: ", server_index);
      for (j = 0; j < reply->count; j++) {
        memcpy(&i, reply->content + sizeof(int) * j, sizeof(int));
        printf("%d", i);
        if (j + 1 < reply->count) {
          printf(", ");
        }
      }
      printf("\n");
    }
    free(reply);
  } else if (Is_membership_mess( service_type)) {
    ret = SP_get_memb_info((char *)reply, service_type, &memb_info);
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
        } else if( Is_caused_network_mess( service_type )) {
          server_found = false;
          for (i = 0; i < num_groups; i++) {
            if (strncmp(&ret_groups[i][0], "#server", strlen("#server")) == 0) {
              server_found = true;
              break;
            }
          }
          if (!server_found) {
            printf("Disconnected from server. Please restart and connect again.\n");
            Bye();
          }
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
  // strcpy((*update)->client_private_group, Private_group);
}

static void CreateLikeLineUpdate(Update **update, int line_number, char *room, bool isAdd) {
  if (*update != NULL) free(*update);
  *update = malloc(sizeof(Update));
  (*update)->type = isAdd ? AddLike : RemoveLike;
  (*update)->line_number = line_number;
  strcpy((*update)->room, room);
  strcpy((*update)->user, username);
  (*update)->sender_index = 0;
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
  printf("Room: %s\n", roomName);
  printf("Attendees (%d): ", reply->member_count);
  for (i = 0; i < reply->member_count; i++) {
    printf("%s", reply->content + sizeof(char) * MAX_GROUP_NAME * reply->count + sizeof(char) * MAX_LINE_LENGTH * reply->count + sizeof(int) * reply->count + sizeof(char) * MAX_GROUP_NAME + sizeof(bool) + sizeof(char) * MAX_GROUP_NAME * i);
    if (i + 1 < reply->member_count) {
      printf(", ");
    }
  }
  printf("\n");
  for (i = 0; i < reply->count; i++) {
    printf("%d. %s: %s", i + 1,
      reply->content + sizeof(char) * i * MAX_GROUP_NAME,
      reply->content + sizeof(char) * reply->count * MAX_GROUP_NAME + sizeof(char) * i * MAX_LINE_LENGTH);
    memcpy(&like_count, reply->content + sizeof(char) * reply->count * MAX_GROUP_NAME + sizeof(char) * reply->count * MAX_LINE_LENGTH + sizeof(int) * i, sizeof(int));
    if (like_count > 0) {
      printf(" (%d like%s)\n", like_count, like_count != 1 ? "s" : "");
    } else {
      printf("\n");
    }
  }
  printf("%s>", username);
}
