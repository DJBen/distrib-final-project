#ifndef _CHAT_INCLUDE_
#define _CHAT_INCLUDE_ 0
#define MAX_LINE_LENGTH 120
#define MAX_GROUPS 10

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
  int line_index;
  char room[MAX_GROUP_NAME];
  // 0 for client, 1-5 for server that sends it
  int sender_index;
} Update;

typedef enum
{
  ReplyPrintHistory, ReplyPrintView
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
#define PrintHistoryReplySizeWithCount(count) sizeof(Reply) + sizeof(char) * MAX_GROUP_NAME * count + sizeof(char) * MAX_LINE_LENGTH * count + sizeof(int) * count
#define MAX_REPLY_SIZE PrintHistoryReplySizeWithCount(100)
#define PrintViewReplySizeWithCount(count) sizeof(Reply) + sizeof(char) * MAX_GROUP_NAME * count
#define ServerGroupNameWithIndex(index) "server_%d", index
#define ServerClientGroupNameWithIndex(index) "server_clients_%d", index

#endif
