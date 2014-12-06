#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>

#include "chat_includes.h"
#include "vector.h"

#define MAX_CHATROOM_COUNT 100
#define MAX_LINE_COUNT 1000
#define MAX_USER_COUNT 64

typedef struct
{
  char content[MAX_LINE_LENGTH];
  char sender[MAX_GROUP_NAME];
  int like_count;
  // All users who have liked this.
  struct vector *likes;
} Line;

typedef struct
{
  char name[80];
  struct vector *members;
  struct vector *lines;
} Chatroom;

static mailbox Mbox;
static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static int server_index;
static Chatroom *chatrooms[MAX_CHATROOM_COUNT];
static int chat_room_count = 0;
static char ret_groups[MAX_GROUPS][MAX_GROUP_NAME];

static void Bye();
static void Print_help();
static void Runloop();
static void Print_help();
static void Usage();
static void freeString(void *data);
static int findRoomWithName(char *roomName);
static void CreateHistoryReply(Reply **reply, ReplyType type, Chatroom *room, int limit);
static void CreateHistoryReplyWithNewUser(Reply **reply, ReplyType type, Chatroom *room, int limit, char *newUserName);
static void printChatrooms();

int main(int argc, char const *argv[])
{
  int ret;
  char spread_group_name[MAX_GROUP_NAME];

  // arrayListTest();
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
  static Update *recv_mess;

  int room_found = -1;
  static Chatroom *current_room;
  char *temp_name;
  char temp_group_name[MAX_GROUP_NAME];
  Line *temp_line;
  Reply *temp_reply = NULL;
  membership_info memb_info;


  recv_mess = malloc(MAX(MAX_REPLY_SIZE, UpdateMsgSize));

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
                    &mess_type, &dummy_endian_mismatch, MAX(UpdateMsgSize, MAX_REPLY_SIZE), (char *)recv_mess);
    if(ret < 0) {
      if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
        service_type = DROP_RECV;
        printf("\n========Buffers or Groups too Short=======\n");
        SP_error(ret);
        exit(1);
      }
    }
    if (Is_regular_mess(service_type)) {
      if (mess_type == UpdateMessageType) {
        // Process it if it's not from myself
        if (recv_mess->sender_index == server_index) {
          // printf("Ignoring msg from myself.\n");
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
            current_room = malloc(sizeof(Chatroom));
            strcpy(current_room->name, recv_mess->room);
            current_room->members = vector_init(&freeString);
            strcpy(temp_name, recv_mess->user);
            vector_append(current_room->members, temp_name);
            current_room->lines = vector_init(&freeString);
            chatrooms[chat_room_count++] = current_room;

            // Create spread group for chat group on this server
            sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, current_room->name);
            SP_join(Mbox, temp_group_name);

            printf("Chat room %s created. Users: %s\n", current_room->name, temp_name);
          } else {
            // Chat group found: add user to that group only
            current_room = chatrooms[room_found];
            strcpy(temp_name, recv_mess->user);
            vector_append(current_room->members, temp_name);
            printf("User %s joined chat room %s. Users: ", temp_name, current_room->name);
            vector_print(current_room->members);
          }
          // Send other user's update about the join
          sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, current_room->name);
          CreateHistoryReplyWithNewUser(&temp_reply, ReplyNewUserJoin, current_room, 25, recv_mess->user);
          ret = SP_multicast( Mbox, AGREED_MESS, temp_group_name, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
          if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
            {
              if( ret < 0 )
              {
                SP_error( ret );
                exit(1);
              }
            }
          printf("Join message multicast.\n");
        } else if (recv_mess->type == AddLine) {
          // printChatrooms(); // DEBUG
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
            temp_line->likes = vector_init(&freeString);
            vector_append(current_room->lines, temp_line);
            // printf("* current room line count = %d\n", vector_size(current_room->lines));
            // Broadcast line to clients in the same chatroom
            sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, current_room->name);
            CreateHistoryReply(&temp_reply, ReplyEchoLine, current_room, 25);
            ret = SP_multicast( Mbox, AGREED_MESS, temp_group_name, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
            if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
            {
              if( ret < 0 )
              {
                SP_error( ret );
                exit(1);
              }
            }
            // free(temp_reply);
            printf("%s> %s\n", temp_line->sender, temp_line->content);
            // printChatrooms(); // DEBUG
          }
        } else if (recv_mess->type == AddLike) {

        } else if (recv_mess->type == RemoveLike) {

        } else if (recv_mess->type == PrintHistory) {
          room_found = findRoomWithName(recv_mess->room);
          if (room_found == -1) {
            // Cannot find room, error
            printf("Cannot find room %s\n", recv_mess->room);
            Bye();
          } else {
            current_room = chatrooms[room_found];
            CreateHistoryReply(&temp_reply, ReplyPrintHistory, current_room, 100);
            printf("Create history records.\n");
            ret = SP_multicast( Mbox, AGREED_MESS, sender, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
            if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
            {
              if( ret < 0 )
              {
                SP_error( ret );
                exit(1);
              }
            }
            // free(temp_reply);
            printf("History sent to %s\n", sender);

          }
        }
      } else {
        // printf("* Unknown msg received with type %d\n", mess_type);
        // if (mess_type == ReplyMessageType) {
          // temp_reply = (Reply *)recv_mess;
          // printf("* Reply type %d, count %d\n", temp_reply->type, temp_reply->count);
          // temp_reply = NULL;
        // }
      }
    } else if (Is_membership_mess( service_type)) {
      ret = SP_get_memb_info(recv_mess, service_type, &memb_info);
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
            printf("Due to chatroom JOIN of %s\n", memb_info.changed_member);
          }
        }
      }
    } else {
      // Service type
      if ( Is_reject_mess( service_type ) ) {
        printf("REJECTED message from %s, of servicetype 0x%x messtype %d, (endian %d) to %d groups \n(%d bytes)\n", sender, service_type, mess_type, dummy_endian_mismatch, num_groups, ret);
      } else printf("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
    }

  } while(true);
}

static void Bye() {
  SP_disconnect(Mbox);
  exit(0);
}

static void freeString(void *data) {
  // printf("* Freed: %s\n", (char *)data);
  free(data);
}

static int findRoomWithName(char *roomName) {
  int room_found = -1;
  int i;
  for (i = 0; i < chat_room_count; i++) {
    if (strcmp(chatrooms[i]->name, roomName) == 0) {
      room_found = i;
      break;
    }
  }
  return room_found;
}

static void printChatrooms() {
  int i;
  printf("* %d Chatrooms: ", chat_room_count);
  for (i = 0; i < chat_room_count; i++) {
    printf("%s", chatrooms[i]->name);
    if (i + 1 < chat_room_count) {
      printf(", ");
    }
  }
  printf("\n");
}

static void CreateHistoryReply(Reply **reply, ReplyType type, Chatroom *room, int limit) {
  int i;
  int start = 0;
  if (*reply) free(*reply);
  *reply = malloc(HistoryReplySizeWithCount(MIN(vector_size(room->lines), limit)));
  (*reply)->type = type;
  if (limit < 0) {
    (*reply)->count = vector_size(room->lines);
    start = 0;
    limit = vector_size(room->lines);
  } else {
    (*reply)->count = MIN(vector_size(room->lines), limit);
    if (vector_size(room->lines) <= limit) {
      start = 0;
      limit = vector_size(room->lines);
    } else {
      start = vector_size(room->lines) - limit;
    }
  }
  // printf("* Create history reply type = %d count = %d\n", type, (*reply)->count);
  for (i = start; i < start + limit; i++) {
    strcpy((*reply)->content + sizeof(char) * MAX_GROUP_NAME * i, ((Line *)vector_get(room->lines, i))->sender);
    strcpy((*reply)->content + sizeof(char) * MAX_GROUP_NAME * (*reply)->count + sizeof(char) * MAX_LINE_LENGTH * i, ((Line *)vector_get(room->lines, i))->content);
    memcpy((*reply)->content + sizeof(char) * MAX_GROUP_NAME * (*reply)->count + sizeof(char) * MAX_LINE_LENGTH * (*reply)->count + i * sizeof(int), &((Line *)vector_get(room->lines, i))->like_count, sizeof(int));
  }
}

static void CreateHistoryReplyWithNewUser(Reply **reply, ReplyType type, Chatroom *room, int limit, char *newUserName) {
  CreateHistoryReply(reply, type, room, limit);
  strcpy((*reply)->content + sizeof(char) * MAX_GROUP_NAME * (*reply)->count + sizeof(char) * MAX_LINE_LENGTH * (*reply)->count + (*reply)->count * sizeof(int), newUserName);
}
