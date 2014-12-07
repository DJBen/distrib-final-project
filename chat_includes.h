#ifndef _CHAT_INCLUDE_

#include <stdbool.h>

#define _CHAT_INCLUDE_ 0
#define MAX_LINE_LENGTH 120
#define MAX_GROUPS 10

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

typedef enum
{
  AddLine, AddLike, RemoveLike, JoinRoom,
  PrintHistory, PrintView // These should not become a part of server's history
} UpdateType;

typedef struct
{
  UpdateType type;
  char user[MAX_GROUP_NAME];
  char line[MAX_LINE_LENGTH];
  int line_number;
  char room[MAX_GROUP_NAME];
  // 0 for client, 1-5 for server that sends it
  int sender_index;
} Update;

typedef enum
{
  ReplyPrintHistory, ReplyPrintView, ReplyEchoLine, ReplyNewUserJoin, ReplyAddLike, ReplyRemoveLike
} ReplyType;

typedef struct
{
  ReplyType type;
  int count;
  char content[1];
} Reply;

#define UpdateMessageType 1023
#define ReplyMessageType 1123

#define UpdateMsgSize sizeof(Update)

// sender[], lines[], likes[], newperson?
#define HistoryReplySizeWithCount(count) sizeof(Reply) + sizeof(char) * MAX_GROUP_NAME * count + sizeof(char) * MAX_LINE_LENGTH * count + sizeof(int) * count + sizeof(char) * MAX_GROUP_NAME + sizeof(bool)
#define MAX_REPLY_SIZE HistoryReplySizeWithCount(100)
#define PrintViewReplySizeWithCount(count) sizeof(Reply) + sizeof(char) * MAX_GROUP_NAME * count
#define ServerGroupNameWithIndex(index) "server_%d", index
#define ServerClientGroupNameWithIndex(index) "server_clients_%d", index

#endif
