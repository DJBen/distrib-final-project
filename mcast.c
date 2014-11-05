#include "sp.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#define MAX_GROUPS 10
#define WINDOW_SIZE 100

#define NORMAL_TYPE 1337
#define FINISH_TYPE 1338

typedef struct
{
  int process_index;
  int message_index;
  int random_number;
  int type;
  char payload[1];
} message;

#define size_of_message sizeof(message) + sizeof(char) * 1199

static mailbox Mbox;
static char User[80];
static char Spread_name[80];
static int num_of_messages;
static int process_index;
static int num_of_processes;
static char Private_group[MAX_GROUP_NAME];
static char ret_groups[MAX_GROUPS][MAX_GROUP_NAME];
static message *recv_mess;
static message *mess;
static int *recvs;
static int *finishes;

static void Print_help()
{
  printf("mcast <num_of_messages> <process_index> <num_of_processes>");
  exit( 0 );
}

static void Usage(int argc, char const *argv[])
{
  sprintf( User, "slu21" );
  sprintf( Spread_name, "4803");
  if (argc != 4) {
    Print_help();
  }
  num_of_messages = (int)strtol(argv[1], (char **)NULL, 10);
  process_index = (int)strtol(argv[2], (char **)NULL, 10);
  num_of_processes = (int)strtol(argv[3], (char **)NULL, 10);
}

int main(int argc, char const *argv[])
{
  int i, j;
  int ret;
  int service_type;
  int joined_members;
  int num_groups; /* number of members in the group */
  char sender[MAX_GROUP_NAME];
  int16 mess_type;
  int dummy_endian_mismatch;

  int sender_proc_index;
  int aru = 0;
  int random_number_received;
  int sent_count = 0;

  FILE *fw;
  char file_name[80];

  time_t begin;
  double time_spent;

  Usage(argc, argv);
  printf("mcast: %d messages, proc %d/%d\n", num_of_messages, process_index, num_of_processes);

  /* Initializations */
  recvs = calloc(num_of_processes, sizeof(int));
  finishes = calloc(num_of_processes, sizeof(int));
  recv_mess = malloc(size_of_message);
  mess = calloc(1, size_of_message);

  sprintf(file_name, "%d.out", process_index);
  fw = fopen(file_name, "w");

  ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
  if (ret < 0) {
    SP_error(ret);
    exit(1);
  }
  SP_join( Mbox, "awesome" );

  /* Wait for all members to join */
  joined_members = 0;
  while(joined_members < num_of_processes)
  {
    service_type = 0;
    ret = SP_receive( Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
                      &mess_type, &dummy_endian_mismatch, size_of_message, (char *)recv_mess );
    if (ret < 0) {
      SP_error(ret);
      exit(1);
    }

    joined_members = num_groups;
  }
  printf("All processes joined.\n");

  begin = clock();

  if (Is_reg_memb_mess(service_type)) {
    printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n", sender, num_groups, mess_type );
    for( i=0; i < num_groups; i++ ) printf("\t%s\n", &ret_groups[i][0] );
    /* update count of joined members */
    joined_members = num_groups;
  } else {
    printf("received message of other message type 0x%x with ret %d\n", service_type, ret);
  }

  for (i = 1; i <= num_of_messages; i++) {
    mess->message_index = i;
    mess->process_index = process_index;
    mess->random_number = rand() % 1000000;
    if (i == num_of_messages) {
      mess->type = FINISH_TYPE;
    } else {
      mess->type = NORMAL_TYPE;
    }
    ret = SP_multicast( Mbox, AGREED_MESS, "awesome", 0, size_of_message, (char *)mess);
    sent_count++;
    if (i % 1000 == 0) {
      printf("mcast: multicast #%d: %d\n", i, mess->random_number);
    }
    if( ret != size_of_message )
    {
      if( ret < 0 )
      {
        SP_error( ret );
        exit(1);
      }
      printf("sent a different message %d -> %d\n", size_of_message, ret );
    }
    if (i > WINDOW_SIZE) {
      int notdone;
      do {
        service_type = 0;
        ret = SP_receive( Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
                      &mess_type, &dummy_endian_mismatch, size_of_message, (char *)recv_mess );
        if( ret < 0 )
        {
          SP_error( ret );
          exit(1);
        }

        /* update counters of received messages */
        sender_proc_index = recv_mess->process_index;
        random_number_received = recv_mess->random_number;
        recvs[sender_proc_index]++;
        if (recv_mess->message_index % 1000 == 0) {
          printf("mcast: message #%d from %d received: %d\n", recv_mess->message_index, sender_proc_index, random_number_received);
        }
        // Write to file
        fprintf(fw, "%2d, %8d, %8d\n", recv_mess->process_index, recv_mess->message_index,
      recv_mess->random_number );
        if (recvs[sender_proc_index] == aru + 1) {
            /* Update aru */
            aru = INT_MAX;
            for (j = 0; j < num_of_processes; j++) {
                if (recvs[j] == 0) continue;
                if (recvs[j] < aru) aru = recvs[j];
            }
        }
        /* Read loop is done if we send messages and we have received all
         * other senders messages upto 100 less then our current send count
         */
        notdone = aru < (sent_count - WINDOW_SIZE);
      } while(notdone);
    }
  }

  int not_finished;
  if (num_of_messages == 0) finishes[process_index] = 1;
  do {
      not_finished = 0;
      ret = SP_receive( Mbox, &service_type, sender, MAX_GROUPS, &num_groups, ret_groups,
                    &mess_type, &dummy_endian_mismatch, size_of_message, (char *)recv_mess );
      if( ret < 0 )
      {
        SP_error( ret );
        exit(1);
      }
      /* update counters of received messages */
      sender_proc_index = recv_mess->process_index;
      random_number_received = recv_mess->random_number;
      recvs[sender_proc_index]++;
      if (recv_mess->message_index % 1000 == 0) {
        printf("mcast: message #%d from %d received: %d\n", recv_mess->message_index, sender_proc_index, random_number_received);
      }
      // Write to file
      fprintf(fw, "%2d, %8d, %8d\n", recv_mess->process_index, recv_mess->message_index,
    recv_mess->random_number);
      if (recv_mess->type == FINISH_TYPE) {
        finishes[recv_mess->process_index] = 1;
      }
      int k;
      for (k = 0; k < num_of_processes; k++) {
        // printf("finish vector: %d", finishes[k]);
        if (finishes[recv_mess->process_index] == 0) not_finished = 1;
      }
  } while (not_finished);

  printf("mcast: completed multicast of %d messages, %d bytes each.\n", num_of_messages, (int)size_of_message);

  time_spent = (double)(clock()-begin) / CLOCKS_PER_SEC * 10;
  printf("mcast: transferred %d message of %d bytes each in %g seconds\n", num_of_messages, (int)size_of_message, time_spent);

  fclose(fw);

  return 0;
}
