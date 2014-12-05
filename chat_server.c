#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>

#include "chat_includes.h"
#include "arraylist.h"

#define MAX_CHATROOM_COUNT 100
#define MAX_LINE_COUNT 1000
#define MAX_USER_COUNT 64

typedef struct
{
  char content[MAX_LINE_LENGTH];
  char sender[MAX_GROUP_NAME];
  int like_count;
  // All users who have liked this.
  struct array_list *likes;
} Line;

typedef struct
{
  char name[80];
  struct array_list *members;
  struct array_list *lines;
} Chatroom;

static mailbox Mbox;
static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static int server_index;
static Chatroom *chatrooms;
static int chat_room_count = 0;
static char ret_groups[MAX_GROUPS][MAX_GROUP_NAME];

static void Bye();
static void Print_help();
static void Runloop();
static void Print_help();
static void Usage();
static void freeString(void *data);
static int findRoomWithName(char *roomName);

int main(int argc, char const *argv[])
{
  int ret;
  char spread_group_name[MAX_GROUP_NAME];

  Usage(argc, argv);

  ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
  if (ret < 0) {
    SP_error(ret);
    exit(1);
  }

  // Create a group with only server
  sprintf(spread_group_name, "server_%d", server_index);
  SP_join( Mbox, spread_group_name );

  // Create a group with clients
  sprintf(spread_group_name, "server_clients_%d", server_index);
  SP_join(Mbox, spread_group_name);

  // Join only group containing all servers
  SP_join(Mbox, "servers");

  Runloop();

  return 0;
}


static void Print_help()
{
  printf("chat_server <server index>");
  Bye();
}

static void Usage(int argc, char const *argv[])
{
  sprintf( User, "slu21_client" );
  sprintf( Spread_name, "10270");
  if (argc != 2) {
    Print_help();
  }
  server_index = (int)strtol(argv[1], (char **)NULL, 10);
  if (server_index < 1 || server_index > 5) {
    Print_help();
  }
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

  int room_found = -1;
  Chatroom current_room;
  char *temp_name;
  Line *temp_line;
  Reply *temp_reply;

  chatrooms = malloc(sizeof(Chatroom) * MAX_CHATROOM_COUNT);
  recv_mess = malloc(sizeof(Update));

  num_groups = 0;
  while (true) {
    ret = SP_receive( Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
                        &mess_type, &dummy_endian_mismatch, UpdateMsgSize, (char *)recv_mess );

    if (Is_reg_memb_mess(service_type)) {
      printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n", sender, num_groups, mess_type );
      if (strcmp(sender, "servers") == 0 && num_groups >= 5) {
        break;
      }
      // for( i=0; i < num_groups; i++ ) printf("\t%s\n", &ret_groups[i][0] );
    } else {
      printf("received message of other message type 0x%x with ret %d\n", service_type, ret);
    }
  }

  // All servers joined at this point
  printf("All servers joined at this point.\n");

  do {
    ret = SP_receive(Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
                    &mess_type, &dummy_endian_mismatch, UpdateMsgSize, (char *)recv_mess);
    if (Is_regular_mess(service_type)) {
      // Process it if it's not from myself
      if (recv_mess->sender_index == server_index) {
        continue;
      }
      // Resend this message to other servers
      if (recv_mess->sender_index == 0 && recv_mess->type != PrintHistory && recv_mess->type != PrintView) {
        recv_mess->sender_index = server_index;
        ret = SP_multicast( Mbox, AGREED_MESS, "servers", UpdateMessageType, UpdateMsgSize, (char *)recv_mess);
        if( ret != UpdateMsgSize)
        {
          if( ret < 0 )
          {
            SP_error( ret );
            exit(1);
          }
        }
      }
      // Sender is the client private group name
      if (recv_mess->type == JoinRoom) {
        room_found = findRoomWithName(recv_mess->room);
        temp_name = malloc(sizeof(char) * MAX_GROUP_NAME);
        if (room_found == -1) {
          // Chat group not found: create the group
          strcpy(current_room.name, recv_mess->room);
          current_room.members = array_list_new(&freeString);
          strcpy(temp_name, recv_mess->user);
          array_list_add(current_room.members, temp_name);
          current_room.lines = array_list_new(&freeString);
          chatrooms[chat_room_count++] = current_room;
          printf("Chat room %s created. Users: %s\n", current_room.name, temp_name);
        } else {
          // Chat group found: add user to that group only
          current_room = chatrooms[room_found];
          strcpy(temp_name, recv_mess->user);
          array_list_add(current_room.members, temp_name);
          printf("User %s joined chat room %s. Users: ", temp_name, current_room.name);
          array_list_print(current_room.members);
        }
      } else if (recv_mess->type == AddLine) {
        room_found = findRoomWithName(recv_mess->room);
        if (room_found == -1) {
          // Cannot find room, error
          printf("Cannot find room %s\n", recv_mess->room);
          Bye();
        } else {
          current_room = chatrooms[room_found];
          temp_line = malloc(sizeof(Line));
          strcpy(temp_line->content, recv_mess->line);
          strcpy(temp_line->sender, recv_mess->user);
          temp_line->like_count = 0;
          temp_line->likes = array_list_new(&freeString);
          array_list_add(current_room.lines, temp_line);
          printf("%s> %s\n", temp_line->sender, temp_line->content);
        }
      } else if (recv_mess->type == PrintHistory) {
        room_found = findRoomWithName(recv_mess->room);
        if (room_found == -1) {
          // Cannot find room, error
          printf("Cannot find room %s\n", recv_mess->room);
          Bye();
        } else {
          current_room = chatrooms[room_found];
          temp_reply = malloc(PrintHistoryReplySizeWithCount(array_list_length(current_room.lines)));
          temp_reply->type = ReplyPrintHistory;
          temp_reply->count = array_list_length(current_room.lines);
          for (i = 0; i < temp_reply->count; i++) {
            strcpy(temp_reply->content + sizeof(char) * MAX_GROUP_NAME * i, ((Line *)array_list_get_idx(current_room.lines, i))->sender);
          }
          for (i = 0; i < temp_reply->count; i++) {
            strcpy(temp_reply->content + sizeof(char) * MAX_GROUP_NAME * temp_reply->count + sizeof(char) * MAX_LINE_LENGTH * i, ((Line *)array_list_get_idx(current_room.lines, i))->content);
          }
          for (i = 0; i < temp_reply->count; i++) {
            memcpy(temp_reply->content + sizeof(char) * MAX_GROUP_NAME * temp_reply->count + sizeof(char) * MAX_LINE_LENGTH * temp_reply->count + i * sizeof(int), &((Line *)array_list_get_idx(current_room.lines, i))->like_count, sizeof(int));
          }
          ret = SP_multicast( Mbox, AGREED_MESS, sender, ReplyMessageType, PrintHistoryReplySizeWithCount(temp_reply->count), (char *)temp_reply);
          if (ret != PrintHistoryReplySizeWithCount(temp_reply->count))
          {
            if( ret < 0 )
            {
              SP_error( ret );
              exit(1);
            }
          }
          free(temp_reply);
          printf("History sent to %s\n", sender);
        }
      }
    } else if (Is_reg_memb_mess(service_type)) {
      printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n", sender, num_groups, mess_type );
      for( i=0; i < num_groups; i++) printf("\t%s\n", &ret_groups[i][0] );
    }

  } while(true);
}

static void Bye() {
  SP_disconnect(Mbox);
  exit(0);
}

static void freeString(void *data) {
  free(data);
}

static int findRoomWithName(char *roomName) {
  int room_found = -1;
  int i;
  for (i = 0; i < chat_room_count; i++) {
    if (strcmp(chatrooms[i].name, roomName) == 0) {
      room_found = i;
      break;
    }
  }
  return room_found;
}
