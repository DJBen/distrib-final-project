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
#define MAX_VSSETS      10
#define MAX_MEMBERS     100
#define StatusMessageType 3389

typedef struct
{
  char content[MAX_LINE_LENGTH];
  char sender[MAX_GROUP_NAME];
  int like_count;
  // All users who have liked this.
  Vector *likes;
} Line;

typedef struct
{
  char name[80];
  Vector *members;
  // Vector *client_ids; // The private groups of clients connected to the chatroom
  Vector *lines;
} Chatroom;

typedef struct
{
  int server_index;
  int last_updates[5];
} Status;

static mailbox Mbox;
static char User[80];
static char Spread_name[80];
static char Private_group[MAX_GROUP_NAME];
static int server_index;
static Chatroom *chatrooms[MAX_CHATROOM_COUNT];
static int chat_room_count = 0;
static char ret_groups[MAX_GROUPS][MAX_GROUP_NAME];

static Vector *update_history;
static int update_timestamps[5] = {0, 0, 0, 0, 0};
static int current_servers_count;
static int current_servers[5];

static void Bye();
static void Print_help();
static void Runloop();
static void Print_help();
static void Usage();
void freeString(void *data);
void freeLine(void *data);
void freeUpdate(void *data);
static int FindRoomWithName(char *roomName);
static void CreateHistoryReply(Reply **reply, ReplyType type, Chatroom *room, int limit);
static void CreateHistoryReplyWithUserName(Reply **reply, ReplyType type, Chatroom *room, int limit, char *userName);
static void CreateHistoryReplyWithStatus(Reply **reply, ReplyType type, Chatroom *room, int limit, bool status);
static void CreatePrintViewReply(Reply **reply);
static void printChatrooms();
static void printTimestamps(int *timestamp_array);

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
  if (argc != 2) {
    Print_help();
  }
  server_index = (int)strtol(argv[1], (char **)NULL, 10);
  if (server_index < 1 || server_index > 5) {
    Print_help();
  }
  sprintf( User, "server%d", server_index);
  sprintf( Spread_name, "10270");
}

static void Runloop() {
  int i, j, k;
  int num_groups; /* number of members in the group */
  char sender[MAX_GROUP_NAME];
  int16 mess_type;
  int dummy_endian_mismatch;
  int ret;
  int service_type;
  vs_set_info      vssets[MAX_VSSETS];
  unsigned int     my_vsset_index;
  int              num_vs_sets;
  char             members[MAX_MEMBERS][MAX_GROUP_NAME];
  static Update *recv_mess;
  Update *temp_update;
  Update *temp_update_to_send;

  int room_found = -1;
  static Chatroom *current_room;
  char *temp_name;
  char temp_group_name[MAX_GROUP_NAME];
  char temp_room_name[MAX_GROUP_NAME];
  Line *temp_line;
  Reply *temp_reply = NULL;
  membership_info memb_info;
  int user_index_found;
  int line_number_found;
  bool updateProcessed;
  bool client_id_found;
  int localMax, localMin, updater;

  Status my_status;
  Status *temp_status;
  int received_stati[5][5];
  int status_message_received_count;

  recv_mess = malloc(MAX(MAX_REPLY_SIZE, UpdateMsgSize));

  while (true) {
    ret = SP_receive( Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
                        &mess_type, &dummy_endian_mismatch, UpdateMsgSize, (char *)recv_mess );

    if (Is_reg_memb_mess(service_type)) {
      printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n", sender, num_groups, mess_type );
      if (strcmp(sender, "servers") == 0) {
        current_servers_count = num_groups;
        for (i = 0; i < num_groups; i++) {
          j = atoi(&ret_groups[i][strlen("#server")]);
          current_servers[i] = j;
        }
      }
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

  // Initialize history
  update_history = vector_init(freeUpdate);

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
        updateProcessed = false;

        // Update timestamp, even it is from myself
        if (recv_mess->sender_index > 0) {
          if (update_timestamps[recv_mess->sender_index - 1] >= recv_mess->timestamp) {
            // Message too old for me
            printf("* Disrecard message (%d, %d). Too old.\n", recv_mess->sender_index, recv_mess->timestamp);
            continue;
          }
          update_timestamps[recv_mess->sender_index - 1] = recv_mess->timestamp;
          printTimestamps(update_timestamps);
        }

        // Don't process it if it's from myself
        if (recv_mess->sender_index == server_index) {
          // printf("Ignoring msg from myself.\n");
          continue;
        }

        // Sender is the client private group name
        if (recv_mess->type == JoinRoom) {
          // For partition: check all rooms if the client was there before. If so, erase old name.
          for (i = 0; i < chat_room_count; i++) {
            j = vector_index_of_str(chatrooms[i]->members, recv_mess->user);
            if (j != -1) {
              printf("Remove user %s from old chatroom %s\n", recv_mess->user, chatrooms[i]->name);
              vector_remove(chatrooms[i]->members, j);
            }
          }

          // Join the chatroom
          room_found = FindRoomWithName(recv_mess->room);
          temp_name = malloc(sizeof(char) * MAX_GROUP_NAME);
          if (room_found == -1) {
            // Chat group not found: create the group
            current_room = malloc(sizeof(Chatroom));
            strcpy(current_room->name, recv_mess->room);
            current_room->members = vector_init(freeString);
            strcpy(temp_name, recv_mess->user);
            vector_append(current_room->members, temp_name);
            current_room->lines = vector_init(freeLine);
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
          CreateHistoryReplyWithUserName(&temp_reply, ReplyNewUserJoin, current_room, 25, recv_mess->user);
          ret = SP_multicast( Mbox, AGREED_MESS, temp_group_name, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
          if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
          {
            if( ret < 0 )
            {
              SP_error( ret );
              exit(1);
            }
          }
          updateProcessed = true;
        } else if (recv_mess->type == AddLine) {
          // printChatrooms(); // DEBUG
          room_found = FindRoomWithName(recv_mess->room);
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
            temp_line->likes = vector_init(freeString);
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
            updateProcessed = true;
          }
        } else if (recv_mess->type == AddLike || recv_mess->type == RemoveLike) {
          room_found = FindRoomWithName(recv_mess->room);
          if (room_found == -1) {
            // Cannot find room, error
            printf("Cannot find room %s\n", recv_mess->room);
            Bye();
          } else {
            current_room = chatrooms[room_found];
            if (recv_mess->line_number - 1 < 0 || recv_mess->line_number - 1 >= vector_size(current_room->lines)) {
              printf("Cannot %s line %d: line does not exist.\n", recv_mess->type == AddLike ? "like" : "unlike", recv_mess->line_number);
              // Reply failure
              continue;
            }
            // -1 is important because the visual format starts with 1.
            temp_line = vector_get(current_room->lines, recv_mess->line_number - 1);
            // make sure the guy who adds/removes like is a member of the group
            user_index_found = vector_index_of_str(current_room->members, recv_mess->user);
            if (user_index_found == -1) {
              printf("User %s who wants to %s does not belong to the chatroom %s, which currently has members: ", recv_mess->user, recv_mess->type == AddLike ? "like" : "unlike", current_room->name);
              vector_print(current_room->members);
              break;
            }
            // make sure the guy doesn't like twice
            if (recv_mess->type == AddLike) {
              line_number_found = vector_index_of_str(temp_line->likes, recv_mess->user);
              if (line_number_found != -1) {
                // Process duplicate likes: reply failure
                sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, current_room->name);
                CreateHistoryReplyWithStatus(&temp_reply, ReplyAddLike, current_room, 25, false);
                ret = SP_multicast( Mbox, AGREED_MESS, temp_group_name, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
                if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
                {
                  if( ret < 0 )
                  {
                    SP_error( ret );
                    exit(1);
                  }
                }
                printf("User %s cannot like line %d twice.\n", recv_mess->user, recv_mess->line_number);
              } else {
                // Add like
                temp_name = malloc(sizeof(char) * MAX_GROUP_NAME);
                strcpy(temp_name, recv_mess->user);
                temp_line->like_count++;
                vector_append(temp_line->likes, temp_name);

                // Reply success
                sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, current_room->name);
                CreateHistoryReplyWithStatus(&temp_reply, ReplyAddLike, current_room, 25, true);
                ret = SP_multicast( Mbox, AGREED_MESS, temp_group_name, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
                if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
                {
                  if( ret < 0 )
                  {
                    SP_error( ret );
                    exit(1);
                  }
                }
                printf("User %s liked line %d successfully.\n", recv_mess->user, recv_mess->line_number);
                updateProcessed = true;
              }
            } else {
              // make sure the guy doesn't unlike a not-liked line
              line_number_found = vector_index_of_str(temp_line->likes, recv_mess->user);
              if (line_number_found == -1) {
                // Report failure
                sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, current_room->name);
                CreateHistoryReplyWithStatus(&temp_reply, ReplyRemoveLike, current_room, 25, false);
                ret = SP_multicast( Mbox, AGREED_MESS, temp_group_name, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
                if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
                {
                  if( ret < 0 )
                  {
                    SP_error( ret );
                    exit(1);
                  }
                }
                printf("User %s cannot unlike line %d which has not been liked.\n", recv_mess->user, recv_mess->line_number);
              } else {
                // Unlike the line
                temp_line->like_count--;
                vector_remove(temp_line->likes, line_number_found);
                // Report success
                sprintf(temp_group_name, "server_chatroom_%d_%s", server_index, current_room->name);
                CreateHistoryReplyWithStatus(&temp_reply, ReplyRemoveLike, current_room, 25, true);
                ret = SP_multicast( Mbox, AGREED_MESS, temp_group_name, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
                if (ret != HistoryReplySizeWithCount(vector_size(current_room->lines)))
                {
                  if( ret < 0 )
                  {
                    SP_error( ret );
                    exit(1);
                  }
                }
                printf("User %s unliked line %d successfully.\n", recv_mess->user, recv_mess->line_number);
                updateProcessed = true;
              }
            }
          }
        } else if (recv_mess->type == PrintHistory) {
          room_found = FindRoomWithName(recv_mess->room);
          if (room_found == -1) {
            // Cannot find room, error
            printf("Cannot find room %s\n", recv_mess->room);
            Bye();
          } else {
            current_room = chatrooms[room_found];
            CreateHistoryReply(&temp_reply, ReplyPrintHistory, current_room, 100);
            printf("Create history records.\n");
            ret = SP_multicast(Mbox, AGREED_MESS, sender, ReplyMessageType, HistoryReplySizeWithCount(vector_size(current_room->lines)), (char *)temp_reply);
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
        } else if (recv_mess->type == PrintView) {
          CreatePrintViewReply(&temp_reply);
          ret = SP_multicast(Mbox, AGREED_MESS, sender, ReplyMessageType, PrintViewReplySizeWithCount(current_servers_count), (char *)temp_reply);
          if (ret != PrintViewReplySizeWithCount(current_servers_count))
          {
            if( ret < 0 )
            {
              SP_error( ret );
              exit(1);
            }
          }
          printf("Server view provided to %s.\n", recv_mess->user);
        }

        // Resend this update to other servers, only if the update is processed
        if (updateProcessed) {
          // If sent by client
          if (recv_mess->sender_index == 0) {
            // add time stamp
            recv_mess->sender_index = server_index;
            recv_mess->timestamp = update_timestamps[server_index - 1] + 1;
            ret = SP_multicast( Mbox, AGREED_MESS, "servers", UpdateMessageType, UpdateMsgSize, (char *)recv_mess);
            if( ret != UpdateMsgSize)
            {
              if( ret < 0 )
              {
                SP_error( ret );
                exit(1);
              }
            }
            // printf("* Resend update type %d to the fellow servers\n", recv_mess->type);
          }
          // Save it to local history
          temp_update = malloc(UpdateMsgSize);
          memcpy(temp_update, recv_mess, UpdateMsgSize);
          vector_append(update_history, temp_update);
        }
      } else {
        if (mess_type == StatusMessageType) {
          status_message_received_count++;
          temp_status = (Status *)recv_mess;
          printf("Received vector from %d: ", temp_status->server_index);
          printTimestamps(temp_status->last_updates);
          for (i = 0; i < 5; i++) {
            received_stati[temp_status->server_index - 1][i] = temp_status->last_updates[i];
          }

          if (status_message_received_count == 5) {
            printf("Received status vectors from all servers.\n");
            for (i = 0; i < 5; i++) {
              printf("[");
              for (j = 0; j < 5; j++) {
                printf("%d", received_stati[i][j]);
                if (j + 1 < 5) printf(", ");
              }
              printf("]");
              if (i + 1 < 5) printf(", ");
            }
            printf("\n");

            // Sync
            for (i = 0; i < 5; i++) { // server index - 1's updates needs to be resent
              localMax = -1; // The guy that is chosen to send new updates
              localMin = 2147483647;
              for (j = 0; j < 5; j++) { // server index - 1 that sends new updates
                if (j == server_index - 1) {
                  if (received_stati[j][i] >= localMax) {
                    localMax = received_stati[j][i];
                    updater = j;
                  }
                } else {
                  if (received_stati[j][i] > localMax) {
                    localMax = received_stati[j][i];
                    updater = j;
                  }
                }
                if (received_stati[j][i] < localMin) localMin = received_stati[j][i];
              }
              // If it is my responsibility to send
              if (updater == server_index - 1) {
                for (k = 0; k < vector_size(update_history); k++) {
                  temp_update = (Update *)vector_get(update_history, k);
                  if (temp_update->sender_index - 1 == i && temp_update->timestamp > localMin) {
                    // resend updates
                    printf("Should resend update (%d, %d)\n", temp_update->sender_index, temp_update->timestamp);
                    ret = SP_multicast( Mbox, AGREED_MESS, "servers", UpdateMessageType, UpdateMsgSize, (char *)temp_update);
                    if( ret != UpdateMsgSize)
                    {
                      if( ret < 0 )
                      {
                        SP_error( ret );
                        exit(1);
                      }
                    }
                  }
                }
              }
            }
          }
        } else if (mess_type == ReplyMessageType) {
          temp_reply = (Reply *)recv_mess;
          // printf("* Reply type %d, count %d\n", temp_reply->type, temp_reply->count);
          temp_reply = NULL;
        } else {
          printf("* Unknown msg received with type %d\n", mess_type);
        }
      }
    } else if (Is_membership_mess( service_type)) {
      ret = SP_get_memb_info((char *)recv_mess, service_type, &memb_info);
      if (ret < 0) {
        printf("BUG: membership message does not have valid body\n");
        SP_error(ret);
        exit(1);
      }
      if (Is_reg_memb_mess(service_type)) {
        printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n", sender, num_groups, mess_type );
        for( i=0; i < num_groups; i++) printf("\t%s\n", &ret_groups[i][0] );
        if (strcmp(sender, "servers") == 0) {
          current_servers_count = num_groups;
          for (i = 0; i < num_groups; i++) {
            j = atoi(&ret_groups[i][strlen("#server")]);
            current_servers[i] = j;
          }
        }
        if (strncmp(sender, "server_chatroom_", strlen("server_chatroom_")) == 0) {
          if (Is_caused_join_mess(service_type)) {
            printf("Due to chatroom JOIN of %s\n", memb_info.changed_member);
          }
        }
        if( Is_caused_network_mess( service_type )) {
          printf("Due to NETWORK change with %u VS sets\n", memb_info.num_vs_sets);
          num_vs_sets = SP_get_vs_sets_info((char *)recv_mess, &vssets[0], MAX_VSSETS, &my_vsset_index );
          if (num_vs_sets < 0) {
            printf("BUG: membership message has more then %d vs sets. Recompile with larger MAX_VSSETS\n", MAX_VSSETS);
            SP_error( num_vs_sets );
            exit(1);
          }
          for( i = 0; i < num_vs_sets; i++ ) {
            printf("%s VS set %d has %u members:\n",
             (i  == my_vsset_index) ?
             ("LOCAL") : ("OTHER"), i, vssets[i].num_members );
            ret = SP_get_vs_set_members((char *)recv_mess, &vssets[i], members, MAX_MEMBERS);
            if (ret < 0) {
              printf("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
              SP_error( ret );
              exit( 1 );
            }
            for( j = 0; j < vssets[i].num_members; j++ ) {
              printf("\t%s\n", members[j] );
            }
          }

          // Synchronize after merge
          if (strcmp("servers", sender) == 0 && num_vs_sets > 1) {
            printf("Preparing remerge.\n");
            for (i = 0; i < 5; i++) {
              my_status.server_index = server_index;
              my_status.last_updates[i] = update_timestamps[i];
            }
            status_message_received_count = 0;
            ret = SP_multicast(Mbox, AGREED_MESS, "servers", StatusMessageType, sizeof(Status), (char *)&my_status);
            if(ret != sizeof(Status))
            {
              if( ret < 0 )
              {
                SP_error( ret );
                exit(1);
              }
            }
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

void freeString(void *data) {
  // printf("* Freed: %s\n", (char *)data);
  free(data);
}

void freeLine(void *data) {
  Line *line_to_be_freed = (Line *)data;
  vector_free(line_to_be_freed->likes);
  free(line_to_be_freed);
}

void freeUpdate(void *data) {
  free(data);
}

static int FindRoomWithName(char *roomName) {
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

static void printTimestamps(int *timestamp_array) {
  int i;
  printf("Timestamps: [");
  for (i = 0; i < 5; i++) {
    printf("%d", timestamp_array[i]);
    if (i + 1 < 5) {
      printf(", ");
    }
  }
  printf("]\n");
}

static void CreatePrintViewReply(Reply **reply) {
  if (*reply) free(*reply);
  *reply = malloc(PrintViewReplySizeWithCount(current_servers_count));
  (*reply)->type = ReplyPrintView;
  (*reply)->count = current_servers_count;
  memcpy((*reply)->content, current_servers, sizeof(int) * current_servers_count);
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
  (*reply)->member_count = vector_size(room->members);
  for (i = 0; i < vector_size(room->members); i++) {
    printf("* Copying member %s.\n", vector_get(room->members, i));
    strcpy((*reply)->content + sizeof(char) * MAX_GROUP_NAME * (*reply)->count + sizeof(char) * MAX_LINE_LENGTH * (*reply)->count + sizeof(int) * (*reply)->count + sizeof(char) * MAX_GROUP_NAME + sizeof(bool) + sizeof(char) * MAX_GROUP_NAME * i, (char *)vector_get(room->members, i));
  }
}

static void CreateHistoryReplyWithUserName(Reply **reply, ReplyType type, Chatroom *room, int limit, char *userName) {
  CreateHistoryReply(reply, type, room, limit);
  strcpy((*reply)->content + sizeof(char) * MAX_GROUP_NAME * (*reply)->count + sizeof(char) * MAX_LINE_LENGTH * (*reply)->count + (*reply)->count * sizeof(int), userName);
}

static void CreateHistoryReplyWithStatus(Reply **reply, ReplyType type, Chatroom *room, int limit, bool status) {
  CreateHistoryReply(reply, type, room, limit);
  memcpy((*reply)->content + sizeof(char) * MAX_GROUP_NAME * (*reply)->count + sizeof(char) * MAX_LINE_LENGTH * (*reply)->count + (*reply)->count * sizeof(int) + sizeof(char) * MAX_GROUP_NAME, &status, sizeof(bool));
}
