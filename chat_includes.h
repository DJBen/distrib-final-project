#ifndef _CHAT_INCLUDE_
#define _CHAT_INCLUDE_ 0
#define MAX_LINE_LENGTH 120

typedef enum
{
  AddLine, AddLike, RemoveLike
} UpdateType;

typedef struct
{
  UpdateType type;
  char user[MAX_GROUP_NAME];
  char line[MAX_LINE_LENGTH];
} Update;

#define UpdateMsgSize malloc(sizeof(Update))

#endif
